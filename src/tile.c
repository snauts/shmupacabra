#include <assert.h>
#include <math.h>
#include "misc.h"
#include "tile.h"
#include "utlist.h"
#include "world.h"

#if ENABLE_TILE_GRID

int
tile_filter(void *ptr)
{
        return *(int *)ptr == OBJTYPE_TILE;
}

/*
 * Compute tile bounding box (values are in its body's coordinate space).
 */
static BB
tile_local_bb(Tile *t)
{
        vect_f start_pos = t->pos->_.vectf.start;
        vect_f start_sz = t->size->_.vectf.start;
        if (start_sz.x < 0) {
                start_sz.x = -start_sz.x;
                start_sz.y = -start_sz.y;
        }
        assert(start_sz.x > 0 && start_sz.y > 0);
        BB bb = {
                .l=floorf(start_pos.x),
                .r=ceilf(start_pos.x + start_sz.x),
                .b=floorf(start_pos.y),
                .t=ceilf(start_pos.y + start_sz.y)
        };
        
        /* See if position and/or size is animated. */
        if (t->pos->anim_type != ANIM_NONE || t->size->anim_type != ANIM_NONE) {
                vect_f end_pos = (t->pos->anim_type != ANIM_NONE) ? t->pos->_.vectf.end : start_pos;
                vect_f end_sz = (t->size->anim_type != ANIM_NONE) ? t->size->_.vectf.end : start_sz;

                /* Union of start and final bounding boxes. */                
                bb_union(&bb, (BB){
                        .l=floorf(end_pos.x),
                        .r=ceilf(end_pos.x + end_sz.x),
                        .b=floorf(end_pos.y),
                        .t=ceilf(end_pos.y + end_sz.y)
                });
        }
        
        /*
         * If angle property is present, add bounding box that contains any
         * angle.
         */
        Property *rot = t->angle;
        if (rot != NULL) {
                /*
                 * Figure out maximum distance from pivot point to all four
                 * corners.
                 */
                vect_f pivot = rot->_.angle.pivot;
                float dot, dist = 0.0;
                vect_f LB = (vect_f){bb.l - pivot.x, bb.b - pivot.y};
                dot = vect_f_dot(LB, LB);
                if (dot > dist)
                        dist = dot;
                vect_f LT = (vect_f){bb.l - pivot.x, bb.t - pivot.y};
                dot = vect_f_dot(LT, LT);
                if (dot > dist)
                        dist = dot;
                vect_f RT = (vect_f){bb.r - pivot.x, bb.t - pivot.y};
                dot = vect_f_dot(RT, RT);
                if (dot > dist)
                        dist = dot;
                vect_f RB = (vect_f){bb.r - pivot.x, bb.b - pivot.y};
                dot = vect_f_dot(RB, RB);
                if (dot > dist)
                        dist = dot;
                
                /* Get actual distance by doing a square root. */
                dist = sqrtf(dist);
                bb_union(&bb, (BB){
                        .l=floorf(pivot.x - dist),
                        .r=ceilf(pivot.x + dist),
                        .b=floorf(pivot.y - dist),
                        .t=ceilf(pivot.y + dist)
                });
        }
        
        return bb;
}

/*
 * Call this when tile bounding box might have changed.
 */
void
tile_bb_changed(Tile *t)
{
        if (!grid_stored(&t->go))
                return;         /* Tile is not in the tree. */

        /* Tree update sequence. */
        BB bb = tile_local_bb(t);
        body_sweep_bb(t->body, &bb);
        grid_update(&t->body->world->grid, &t->go, bb);
}

#endif  /* ENABLE_TILE_GRID */

Tile *
tile_new(Body *body, vect_f pos, vect_f size, float depth, int grid_store)
{
        extern mem_pool mp_tile;
        Tile *t = mp_alloc(&mp_tile);
        
        assert(t && body);
        t->objtype = OBJTYPE_TILE;
        t->body = body;
        t->depth = depth;
        
        /* Set size and position. */
        t->pos = prop_new();
        t->size = prop_new();
        t->pos->_.vectf.start = pos;
        t->size->_.vectf.start = size;
                        
        /* Add to body's tile list. */
        DL_APPEND(body->tiles, t);
        
#if ENABLE_TILE_GRID
        /* Add to grid if requested. */
        if (grid_store) {
                BB bb = tile_local_bb(t);
                body_sweep_bb(body, &bb);
                grid_add(&body->world->grid, &t->go, t, bb);
        }
#else
        UNUSED(grid_store);
#endif
        return t;
}

Tile *
tile_clone(Body *parent, const Tile *orig)
{
        /* Allocate and set objtype. */
        extern mem_pool mp_tile;
        Tile *t = mp_alloc(&mp_tile);
        t->objtype = OBJTYPE_TILE;
        
        /* Copy properties. */
        t->sprite_list = orig->sprite_list;
        t->pos = prop_copy(orig->pos);
        t->size = prop_copy(orig->size);
        if (orig->frame != NULL)
                t->frame = prop_copy(orig->frame);
        if (orig->color != NULL)
                t->color = prop_copy(orig->color);
        if (orig->angle != NULL)
                t->angle = prop_copy(orig->angle);
        t->depth = orig->depth;
        t->flags = orig->flags;
                
        /* Add to parent. */
        t->body = parent;
        DL_APPEND(parent->tiles, t);
        
#if ENABLE_TILE_GRID
        /* Add to grid if original was stored there. */
        if (orig->go.flags & GRIDFLAG_STORED) {
                BB bb = tile_local_bb(t);
                body_sweep_bb(parent, &bb);
                grid_add(&parent->world->grid, &t->go, t, bb);
        }
#endif
        return t;
}

void
tile_free(Tile *t)
{
        assert(t && t->body);
        
        /* Remove from quad tree if it's in there. */
        if (grid_stored(&t->go))
                grid_remove(&t->body->world->grid, &t->go);
        
        /* Remove from body's tile list. */
        assert((t->prev != NULL || t->next != NULL) && t->body->tiles);
        DL_DELETE(t->body->tiles, t);
        
        /* Free properties. */
        prop_free(t->pos);
        prop_free(t->size);
        if (t->frame != NULL)
                prop_free(t->frame);
        if (t->color != NULL)
                prop_free(t->color);
        if (t->angle != NULL)
                prop_free(t->angle);
        
#if TRACE_MAX
        /* Free trace properties and the trace array itself. */
        TileState *trace = t->trace;
        if (trace != NULL) {
                /* Free properties. */
                for (unsigned i = 0; i < TRACE_MAX; i++) {
                        if (trace[i].pos != NULL) {
                                prop_free(trace[i].pos);
                                prop_free(trace[i].size);
                                if (trace[i].frame != NULL)
                                        prop_free(trace[i].frame);
                                if (trace[i].color != NULL)
                                        prop_free(trace[i].color);
                                if (trace[i].angle != NULL)
                                        prop_free(trace[i].angle);
                        }
                }
                
                /* Free trace array. */
                extern mem_pool mp_tiletrace;
                mp_free(&mp_tiletrace, trace);
        }
#endif
        extern mem_pool mp_tile;
        mp_free(&mp_tile, t);
}

#if TRACE_MAX

/*
 * Add current state to tile's trace array in a round-robin fashion.
 */
void
tile_record_trace(Tile *t, unsigned trace_index)
{
        assert(t && t->trace);
        TileState *ts = &t->trace[trace_index];
        
        /* Free previously stored properties. */
        if (ts->pos != NULL) {
                prop_free(ts->pos);
                prop_free(ts->size);
                if (ts->frame != NULL)
                        prop_free(ts->frame);
                if (ts->color != NULL)
                        prop_free(ts->color);
                if (ts->angle != NULL)
                        prop_free(ts->angle);
        }
        
        /* Copy all properties into trace. */
        ts->pos = prop_copy(t->pos);    /* Always non-NULL */
        ts->size = prop_copy(t->size);  /* Always non-NULL */
        ts->frame = t->frame != NULL ? prop_copy(t->frame) : NULL;
        ts->color = t->color != NULL ? prop_copy(t->color) : NULL;
        ts->angle = t->angle != NULL ? prop_copy(t->angle) : NULL;
}

#endif  /* TRACE_MAX */

#define GET_TIME(tile, anim, now, delta, duration) \
do { \
        (now) = (tile)->body->step * (tile)->body->world->step_sec; \
        (delta) = (now) - (anim)->start_time; \
        (duration) = (anim)->duration; \
} while (0)

/*
 * Given tile animation parameters and time since animation start, calculate
 * what is the current frame (from tile's sprite list) that must be displayed.
 */
unsigned
tile_frame(Tile *t)
{
        Property *anim = t->frame;
        if (anim == NULL)
                return 0;         /* No animation. */
        
        /* If animation type is NONE, return the `start` value. */
        assert(t->sprite_list != NULL);
        if (anim->anim_type == ANIM_NONE)
                return anim->_.frame.start;
        
        int start = anim->_.frame.start;
        int end = anim->_.frame.end;
        float now, delta, duration;
        GET_TIME(t, anim, now, delta, duration);
        
        /* If animation has not started yet, return start value. */
        if (delta <= 0.0)
                return start;
        
        /* Calculate current frame index. */
        switch (anim->anim_type) {
        case ANIM_LOOP: {
                delta = fmod(delta, duration);
                return round(interp_linear(start, end, duration, delta));
        }
        case ANIM_CLAMP: {
                if (delta >= duration) {
                        tile_set_frame(t, end);
                        return end;
                }
                return round(interp_linear(start, end, duration, delta));
        }
        case ANIM_REVERSE_CLAMP: {
                if (delta >= duration * 2) {
                        tile_set_frame(t, start);
                        return start;
                }
                if (delta > duration)
                        delta = duration * 2 - delta;
                return round(interp_linear(start, end, duration, delta));
        }
        case ANIM_REVERSE_LOOP:
                if ((delta = fmod(delta, duration * 2)) > duration)
                        delta = duration * 2 - delta;
                return round(interp_linear(start, end, duration, delta));
        }
        fatal_error("Invalid animation type: (%i).", anim->anim_type);
        abort();
}

void
tile_set_frame(Tile *t, unsigned frame)
{
        /* If default value is chosen, destroy property. */
        if (frame == 0) {
                if (t->frame != NULL) {
                        prop_free(t->frame);
                        t->frame = NULL;
                }
                return;
        }
        
        /* Destroy and create property with new value. */
        if (t->frame != NULL)
                prop_free(t->frame);
        t->frame = prop_new();
        t->frame->_.frame.start = frame;
}

uint32_t
tile_color(Tile *t)
{
        Property *anim = t->color;
        if (anim == NULL)
                return 0xFFFFFFFF;    /* No animation -- return opaque white. */
        
        /* If animation type is NONE, return the `start` value. */
        if (anim->anim_type == ANIM_NONE)
                return anim->_.color.start;
        
        uint32_t start = anim->_.color.start;
        uint32_t end = anim->_.color.end;
        
        /* Get time values. */
        float now, delta, duration;
        GET_TIME(t, anim, now, delta, duration);
        
        uint32_t val;
        if (interp_color(anim->anim_type, start, end, duration, delta, &val))
                tile_set_color(t, val);
        return val;
}

void
tile_set_color(Tile *t, uint32_t color)
{
        /* If default value is chosen, destroy property. */
        if (color == 0xFFFFFFFF) {
                if (t->color != NULL) {
                        prop_free(t->color);
                        t->color = NULL;
                }
                return;
        }

        /* Destroy and create property with new value. */
        if (t->color != NULL)
                prop_free(t->color);
        t->color = prop_new();
        t->color->_.color.start = color;
}

float
tile_angle(Tile *t)
{
        Property *anim = t->angle;
        if (anim == NULL)
                return 0.0;         /* No animation -- return zero angle. */
        
        /* If animation type is NONE, return the `start` value. */
        if (anim->anim_type == ANIM_NONE)
                return anim->_.angle.start;
        
        float start = anim->_.angle.start;
        float end = anim->_.angle.end;
        
        /* Get time values. */
        float now, delta, duration;
        GET_TIME(t, anim, now, delta, duration);
        
        /* If animation has not started yet, return start value. */
        if (delta <= 0.0)
                return start;
        
        /* Calculate current value. */
        switch (anim->anim_type) {
        case ANIM_LOOP: {
                delta = fmod(delta, duration);
                return start + (end - start) * delta / duration;
        }
        case ANIM_CLAMP: {
                if (delta >= duration) {
                        tile_set_angle(t, t->angle->_.angle.pivot, end);
                        return end;
                }
                return interp_linear(start, end, duration, delta);
        }
        case ANIM_REVERSE_LOOP: {
                if ((delta = fmod(delta, duration * 2)) > duration)
                        delta = duration * 2 - delta;
                return interp_linear(start, end, duration, delta);
        }
        case ANIM_REVERSE_CLAMP: {
                if (delta >= duration * 2) {
                        tile_set_angle(t, t->angle->_.angle.pivot, start);
                        return start;
                }
                if (delta > duration)
                        delta = duration * 2 - delta;
                return interp_linear(start, end, duration, delta);
        }
        }
        fatal_error("Invalid animation type: (%i).", anim->anim_type);
        abort();
}

void
tile_set_angle(Tile *t, vect_f pivot, float angle)
{
        /* If default values are chosen, destroy property. */
        if (angle == 0.0 && pivot.x == 0.0 && pivot.y == 0.0) {
                if (t->angle != NULL) {
                        prop_free(t->angle);
                        t->angle = NULL;
                }
                tile_bb_changed(t);
                return;
        }
        
        /* Destroy and create property with new value. */
        if (t->angle != NULL)
                prop_free(t->angle);
        t->angle = prop_new();
        t->angle->_.angle.start = angle;
        t->angle->_.angle.pivot = pivot;
        
        tile_bb_changed(t);
}

vect_f
tile_size(Tile *t)
{
        Property *anim = t->size;
        if (anim->anim_type == ANIM_NONE)
                return anim->_.vectf.start;         /* No animation. */
        
        /* Do not forget to convert negative values into positive. */
        vect_f start = {fabs(anim->_.vectf.start.x), fabs(anim->_.vectf.start.y)};
        vect_f end = {fabs(anim->_.vectf.end.x), fabs(anim->_.vectf.end.y)};
        
        float now, delta, duration;
        GET_TIME(t, anim, now, delta, duration);
        
        /* If animation has not started yet, return start value. */
        if (delta <= 0.0)
                return start;
        
        /* Calculate current value. */
        switch (anim->anim_type) {
        case ANIM_LOOP: {
                delta = fmod(delta, duration);
                return (vect_f){
                        interp_linear(start.x, end.x, duration, delta),
                        interp_linear(start.y, end.y, duration, delta)
                };
        }
        case ANIM_CLAMP: {
                if (delta >= duration) {
                        tile_set_size(t, end);
                        return end;
                }
                return (vect_f){
                        interp_linear(start.x, end.x, duration, delta),
                        interp_linear(start.y, end.y, duration, delta)
                };
        }
        case ANIM_CLAMP_EASEIN: {
                if (delta >= duration) {
                        tile_set_size(t, end);
                        return end;
                }
                return (vect_f){
                        interp_easein(start.x, end.x, duration, delta),
                        interp_easein(start.y, end.y, duration, delta)
                };
        }
        case ANIM_CLAMP_EASEOUT: {
                if (delta >= duration) {
                        tile_set_size(t, end);
                        return end;
                }
                return (vect_f){
                        interp_easeout(start.x, end.x, duration, delta),
                        interp_easeout(start.y, end.y, duration, delta)
                };
        }
        case ANIM_CLAMP_EASEINOUT: {
                if (delta >= duration) {
                        tile_set_size(t, end);
                        return end;
                }
                return (vect_f){
                        interp_easeinout(start.x, end.x, duration, delta),
                        interp_easeinout(start.y, end.y, duration, delta)
                };
        }
        case ANIM_REVERSE_LOOP: {
                if ((delta = fmod(delta, duration * 2)) > duration)
                        delta = duration * 2 - delta;
                return (vect_f){
                        interp_linear(start.x, end.x, duration, delta),
                        interp_linear(start.y, end.y, duration, delta)
                };;
        }
        case ANIM_REVERSE_CLAMP: {
                if (delta >= duration * 2) {
                        tile_set_size(t, start);
                        return start;
                }
                if (delta > duration)
                        delta = duration * 2 - delta;
                return (vect_f){
                        interp_linear(start.x, end.x, duration, delta),
                        interp_linear(start.y, end.y, duration, delta)
                };
        }
        }
        fatal_error("Invalid animation type: (%i).", anim->anim_type);
        abort();
}

void
tile_set_size(Tile *t, vect_f size)
{
        assert(size.x != 0 && size.y != 0);
        
        /* Destroy and create a new size property. */
        prop_free(t->size);
        t->size = prop_new();
        t->size->_.vectf.start = size;
        
        tile_bb_changed(t);
}

vect_f
tile_pos(Tile *t)
{
        Property *anim = t->pos;
        if (anim->anim_type == ANIM_NONE)
                return anim->_.vectf.start;         /* No animation. */
        
        vect_f start = anim->_.vectf.start;
        vect_f end = anim->_.vectf.end;
        
        /* Get time values. */
        float now, delta, duration;
        GET_TIME(t, anim, now, delta, duration);
        
        /* If animation has not started yet, return start value. */
        if (delta <= 0.0)
                return start;
                
        /* Calculate current value. */
        switch (anim->anim_type) {
        case ANIM_LOOP: {
                /* Delta remainder. */
                delta = fmod(delta, duration);
                
                /* Interpolate. */
                return (vect_f){
                        interp_linear(start.x, end.x, duration, delta),
                        interp_linear(start.y, end.y, duration, delta)
                };                
        }
        case ANIM_CLAMP: {
                if (delta >= duration) {
                        tile_set_pos(t, end);
                        return end;
                }
                return (vect_f){
                        interp_linear(start.x, end.x, duration, delta),
                        interp_linear(start.y, end.y, duration, delta)
                };
        }
        case ANIM_CLAMP_EASEIN: {
                if (delta >= duration) {
                        tile_set_pos(t, end);
                        return end;
                }
                return (vect_f){
                        interp_easein(start.x, end.x, duration, delta),
                        interp_easein(start.y, end.y, duration, delta)
                };
        }
        case ANIM_CLAMP_EASEOUT: {
                if (delta >= duration) {
                        tile_set_pos(t, end);
                        return end;
                }
                return (vect_f){
                        interp_easeout(start.x, end.x, duration, delta),
                        interp_easeout(start.y, end.y, duration, delta)
                };
        }
        case ANIM_CLAMP_EASEINOUT: {
                if (delta >= duration) {
                        tile_set_pos(t, end);
                        return end;
                }
                return (vect_f){
                        interp_easeinout(start.x, end.x, duration, delta),
                        interp_easeinout(start.y, end.y, duration, delta)
                };
        }
        case ANIM_REVERSE_LOOP: {
                if ((delta = fmod(delta, duration * 2)) > duration)
                        delta = duration * 2 - delta;
                return (vect_f){
                        interp_linear(start.x, end.x, duration, delta),
                        interp_linear(start.y, end.y, duration, delta)
                };
        }
        case ANIM_REVERSE_CLAMP: {
                if (delta >= duration * 2) {
                        tile_set_pos(t, start);
                        return start;
                }
                if (delta > duration)
                        delta = duration * 2 - delta;
                return (vect_f){
                        interp_linear(start.x, end.x, duration, delta),
                        interp_linear(start.y, end.y, duration, delta)
                };
        }
        }
        fatal_error("Invalid animation type: (%i).", anim->anim_type);
        abort();
}

void
tile_set_pos(Tile *t, vect_f pos)
{
        /* Destroy and create a new position property. */
        prop_free(t->pos);
        t->pos = prop_new();
        t->pos->_.vectf.start = pos;
        
        tile_bb_changed(t);
}

#define SET_ANIM(tile, member, prop, type, start_value, end_value, start_time, duration) \
do { \
        Property *anim = (tile)->member; \
        (anim)->_.prop.start = (start_value); \
        (anim)->_.prop.end = (end_value); \
        (anim)->anim_type = (type); \
        (anim)->start_time = (start_time) + (tile)->body->step * (tile)->body->world->step_sec; \
        (anim)->duration = (duration); \
} while (0)

void
tile_anim_frame(Tile *t, uint8_t type, unsigned end, float duration,
                float start_time)
{
        /* Start value. */
        unsigned start_value = tile_frame(t);
        
        /* Destroy previous animation and create a new one. */
        if (t->frame != NULL)
                prop_free(t->frame);
        t->frame = prop_new();
        
        SET_ANIM(t, frame, frame, type, start_value, end, start_time, duration);
}

void
tile_anim_color(Tile *t, uint8_t type, uint32_t end, float duration,
                float start_time)
{
        /* Start value. */
        uint32_t start_value = tile_color(t);
        
        /* Destroy previous animation and create a new one. */
        if (t->color != NULL)
                prop_free(t->color);
        t->color = prop_new();
        
        SET_ANIM(t, color, color, type, start_value, end, start_time, duration);
}

void
tile_anim_angle(Tile *t, uint8_t type, vect_f pivot, float end, float duration,
                float start_time)
{
        /* Start value. */
        float start_value = tile_angle(t);
        
        /* Destroy previous animation and create a new one. */
        if (t->angle != NULL)
                prop_free(t->angle);
        t->angle = prop_new();
        
        SET_ANIM(t, angle, angle, type, start_value, end, start_time, duration);
        t->angle->_.angle.pivot = pivot;
        tile_bb_changed(t);
}

void
tile_anim_pos(Tile *t, uint8_t type, vect_f end, float duration,
              float start_time)
{
        /* Start value. */
        vect_f start_value = tile_pos(t);
        
        /* Destroy previous animation and create a new one. */
        if (t->pos != NULL)
                prop_free(t->pos);
        t->pos = prop_new();
        
        SET_ANIM(t, pos, vectf, type, start_value, end, start_time, duration);        
        tile_bb_changed(t);
}

void
tile_anim_size(Tile *t, uint8_t type, vect_f end, float duration,
               float start_time)
{
        /* Size must be positive. */
        assert(end.x > 0 && end.y > 0);
        
        /* Start value. */
        vect_f start_value = tile_size(t);
        if (start_value.x < 0) {
                start_value.x = -start_value.x;
                start_value.y = -start_value.y;
        }
        assert(start_value.x > 0 && start_value.y > 0);
        
        /* Destroy previous animation and create a new one. */
        if (t->size != NULL)
                prop_free(t->size);
        t->size = prop_new();
        
        SET_ANIM(t, size, vectf, type, start_value, end, start_time, duration);
        tile_bb_changed(t);
}
