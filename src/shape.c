#include <assert.h>
#include <math.h>
#include <string.h>
#include "grid.h"
#include "log.h"
#include "mem.h"
#include "shape.h"
#include "world.h"
#include "utlist.h"

int
shape_filter(void *ptr)
{
        return *(int *)ptr == OBJTYPE_SHAPE;
}

/*
 * Return shape bounding box (values are in its body's coordinate space).
 */
static BB
shape_local_bb(Shape *s)
{        
        ShapeDef start_def = s->def->_.shapedef.start;
        
        switch (s->shape_type) {
                case SHAPE_CIRCLE: {
                        BB bb = (BB){
                                .l=start_def.circle.offset.x - start_def.circle.radius,
                                .r=start_def.circle.offset.x + start_def.circle.radius,
                                .b=start_def.circle.offset.y - start_def.circle.radius,
                                .t=start_def.circle.offset.y + start_def.circle.radius 
                        };
                        if (s->def->anim_type == ANIM_NONE)
                                return bb;
                        
                        ShapeDef end_def = s->def->_.shapedef.end;
                        bb_union(&bb, (BB){
                                .l=end_def.circle.offset.x - end_def.circle.radius,
                                .r=end_def.circle.offset.x + end_def.circle.radius,
                                .b=end_def.circle.offset.y - end_def.circle.radius,
                                .t=end_def.circle.offset.y + end_def.circle.radius 
                        });
                        return bb;
                }
                case SHAPE_RECTANGLE: {
                        BB bb = start_def.rect;
                        if (s->def->anim_type == ANIM_NONE)
                                return bb;
                        
                        ShapeDef end_def = s->def->_.shapedef.end;
                        bb_union(&bb, end_def.rect);
                        return bb;
                }
        }
        fatal_error("Invalid shape type (%i).", s->shape_type);
        abort();
}

/*
 * Call this when shape bounding box might have changed.
 */
void
shape_bb_changed(Shape *s)
{
        if (!grid_stored(&s->go))
                return;         /* Shape is not in the tree. */
 
        /* Grid update sequence. */
        BB bb = shape_local_bb(s);
        body_sweep_bb(s->body, &bb);
        grid_update(&s->body->world->grid, &s->go, bb);
}

/*
 * Return true if shape overlaps bounding box (coords relative to world origin).
 */
int
shape_vs_bb(Shape *s, BB bb)
{
        ShapeDef def = shape_def(s);
        
        /* Rounded body position. */
        vect_f bpos = body_absolute_pos(s->body);
        int body_x = posround(bpos.x);
        int body_y = posround(bpos.y);
        
        switch (s->shape_type) {
        case SHAPE_CIRCLE:
                fatal_error("not implemented");
                return 0;
        case SHAPE_RECTANGLE:
                /* Basic bounding box overlap test. */
                def.rect.l += body_x;
                def.rect.r += body_x;
                def.rect.b += body_y;
                def.rect.t += body_y;
                return bb_overlap(&def.rect, &bb);
        }
        fatal_error("Invalid shape type (%i).", s->shape_type);
        abort();
}

/*
 * Return collision resolve if two shapes collide.
 */
int
shape_vs_shape(Shape *a, Shape *b, BB *resolve)
{
        ShapeDef def_a = shape_def(a);
        ShapeDef def_b = shape_def(b);
        
        vect_f bpos_a = body_absolute_pos(a->body);
        vect_f bpos_b = body_absolute_pos(b->body);
        
        assert(a->shape_type == SHAPE_RECTANGLE);
        assert(b->shape_type == SHAPE_RECTANGLE);
        
        /* Translate to world coords. */
        int body_x = posround(bpos_a.x);
        int body_y = posround(bpos_a.y);
        def_a.rect.l += body_x;
        def_a.rect.r += body_x;
        def_a.rect.b += body_y;
        def_a.rect.t += body_y;
        
        /* Translate to world coords. */
        body_x = posround(bpos_b.x);
        body_y = posround(bpos_b.y);
        def_b.rect.l += body_x;
        def_b.rect.r += body_x;
        def_b.rect.b += body_y;
        def_b.rect.t += body_y;
        
        return bb_intersect_resolve(&def_a.rect, &def_b.rect, resolve);
}

Shape *
shape_new(Body *body, Group *group, uint8_t shape_type, ShapeDef def)
{
        /* Allocate and set objtype. */
        extern mem_pool mp_shape;
        Shape *s = mp_alloc(&mp_shape);
        s->objtype = OBJTYPE_SHAPE;
                
        /* Set shape type, definition, and group. */
        s->shape_type = shape_type;
        s->def = prop_new();
        shape_set_def(s, def);
        s->group = group;
 
        /* Add to body. */
        s->body = body;
        DL_APPEND(body->shapes, s);
        
        /* Add to grid. */
        BB bb = shape_local_bb(s);
        body_sweep_bb(body, &bb);
        grid_add(&body->world->grid, &s->go, s, bb);

        return s;
}

Shape *
shape_clone(Body *parent, const Shape *orig)
{
        /* Allocate and set objtype. */
        extern mem_pool mp_shape;
        Shape *s = mp_alloc(&mp_shape);
        s->objtype = OBJTYPE_SHAPE;
        
        /* Copy properties. */
        s->shape_type = orig->shape_type;
        s->def = prop_copy(orig->def);
        s->color = orig->color;
        s->flags = orig->flags;
        s->group = orig->group;
        
        /* Add to parent. */
        s->body = parent;
        DL_APPEND(parent->shapes, s);
        
        /* Add to grid. */
        BB bb = shape_local_bb(s);
        body_sweep_bb(parent, &bb);
        grid_add(&parent->world->grid, &s->go, s, bb);

        return s;
}

void
shape_free(Shape *s)
{
        assert(s && s->body);
        assert(s->shape_type == SHAPE_CIRCLE ||
               s->shape_type == SHAPE_RECTANGLE);
        
        /* Remove from shape tree if it's in there. */
        Body *body = s->body;
        if (grid_stored(&s->go))
                grid_remove(&body->world->grid, &s->go);
        
        /* Remove shape from its body's list. */
        assert((s->prev != NULL || s->next != NULL) && body->shapes);
        DL_DELETE(body->shapes, s);
        
        /* Destroy shape definition property. */
        prop_free(s->def);
        
#if TRACE_MAX
        /* Free trace properties and the trace array itself. */
        ShapeState *trace = s->trace;
        if (trace != NULL) {
                /* Free properties. */
                for (unsigned i = 0; i < TRACE_MAX; i++) {
                        if (trace[i].def != NULL) {
                                prop_free(trace[i].def);
                        }
                }
                
                /* Free trace array. */
                extern mem_pool mp_shapetrace;
                mp_free(&mp_shapetrace, trace);
        }
#endif
        extern mem_pool mp_shape;
        mp_free(&mp_shape, s);
}

#if TRACE_MAX
/*
 * Add current state to shape's trace array in a round-robin fashion.
 */
void
shape_record_trace(Shape *s, unsigned trace_index)
{
        ShapeState *ss = &s->trace[trace_index];
        
        /* Free previously stored properties. */
        if (ss->def != NULL) {
                prop_free(ss->def);
        }
        
        /* Copy all properties into trace. */
        ss->def = prop_copy(s->def);    /* Always non-NULL. */
}
#endif  /* TRACE_MAX */

ShapeDef
shape_def(Shape *s)
{
        Property *anim = s->def;
        if (anim->anim_type == ANIM_NONE)
                return anim->_.shapedef.start;  /* No animation. */
        
        ShapeDef start = anim->_.shapedef.start;
        ShapeDef end = anim->_.shapedef.end;
        
        /* Get current time, and time since animation start. */
        Body *b = s->body;
        float now = b->step * b->world->step_sec;
        float delta = now - anim->start_time;
        float duration = anim->duration;
        
        /* If animation has not started yet, return start value. */
        if (delta <= 0.0)
                return start;
        
        /* Calculate current value. */
        switch (anim->anim_type) {
        case ANIM_LOOP: {
                /* Delta remainder. */
                delta = fmod(delta, duration);
                
                /* Interpolate. */
                assert(s->shape_type == SHAPE_RECTANGLE);
                return (ShapeDef){
                        .rect = {
                                .l = posround(interp_linear(start.rect.l, end.rect.l, duration, delta)),
                                .r = posround(interp_linear(start.rect.r, end.rect.r, duration, delta)),
                                .b = posround(interp_linear(start.rect.b, end.rect.b, duration, delta)),
                                .t = posround(interp_linear(start.rect.t, end.rect.t, duration, delta))
                        }
                };
        }
        case ANIM_CLAMP: {
                if (delta >= duration) {
                        shape_set_def(s, end);
                        return end;
                }
                
                /* Interpolate. */
                assert(s->shape_type == SHAPE_RECTANGLE);
                return (ShapeDef){
                        .rect = {
                                .l = posround(interp_linear(start.rect.l, end.rect.l, duration, delta)),
                                .r = posround(interp_linear(start.rect.r, end.rect.r, duration, delta)),
                                .b = posround(interp_linear(start.rect.b, end.rect.b, duration, delta)),
                                .t = posround(interp_linear(start.rect.t, end.rect.t, duration, delta))
                        }
                };
        }
        }
        fatal_error("Invalid animation type: (%i).", anim->anim_type);
        abort();
}

void
shape_set_def(Shape *s, ShapeDef def)
{
        /* Destroy and create shapedef property. */
        prop_free(s->def);
        s->def = prop_new();
        s->def->_.shapedef.start = def;
        
        shape_bb_changed(s);
}

#define SET_ANIM(shape, member, prop, type, start_value, end_value, start_time, duration) \
do { \
        Property *anim = (shape)->member; \
        (anim)->_.prop.start = (start_value); \
        (anim)->_.prop.end = (end_value); \
        (anim)->anim_type = (type); \
        (anim)->start_time = (start_time) + (shape)->body->step * (shape)->body->world->step_sec; \
        (anim)->duration = (duration); \
} while (0)

void
shape_anim_def(Shape *s, uint8_t type, ShapeDef end, float duration,
               float start_time)
{
        /* Start value. */
        assert(s->shape_type == SHAPE_RECTANGLE && bb_valid(end.rect));
        ShapeDef start_value = shape_def(s);
        
        /* Destroy previous animation and create a new one. */
        if (s->def != NULL)
                prop_free(s->def);
        s->def = prop_new();
        
        SET_ANIM(s, def, shapedef, type, start_value, end, start_time, duration);        
        shape_bb_changed(s);
}

