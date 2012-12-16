#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include "common.h"
#include "body.h"
#include "event.h"
#include "log.h"
#include "shape.h"
#include "tile.h"
#include "world.h"
#include "utlist.h"

/*
 * The following macros are the same thing as DL_APPEND and DL_DELETE from
 * utlist.h linked list library. The difference is that they allow `prev` and
 * `next` structure members to have an arbitrary prefix, so that the same
 * structure may be placed in multiple lists at the same time.
 */
#define DL_APPEND_P(head,add,prefix) \
do { \
  if (head) { \
      (add)->prefix##prev = (head)->prefix##prev; \
      (head)->prefix##prev->prefix##next = (add); \
      (head)->prefix##prev = (add); \
      (add)->prefix##next = NULL; \
  } else { \
      (head)=(add); \
      (head)->prefix##prev = (head); \
      (head)->prefix##next = NULL; \
  } \
} while (0)

#define DL_DELETE_P(head,del,prefix) \
do { \
  assert((del)->prefix##prev != NULL); \
  if ((del)->prefix##prev == (del)) { \
      (head)=NULL; \
  } else if ((del)==(head)) { \
      (del)->prefix##next->prefix##prev = (del)->prefix##prev; \
      (head) = (del)->prefix##next; \
  } else { \
      (del)->prefix##prev->prefix##next = (del)->prefix##next; \
      if ((del)->prefix##next) { \
          (del)->prefix##next->prefix##prev = (del)->prefix##prev; \
      } else { \
          (head)->prefix##prev = (del)->prefix##prev; \
      } \
  } \
} while (0)

void
body_init(Body *b, Body *parent, World *world, vect_f pos, unsigned flags)
{
        assert(b && world);
        assert(parent == NULL || parent->world == world);
        assert(b->parent == NULL);
        
        /* Set nonzero attributes. */
        b->objtype = OBJTYPE_BODY;
        b->flags = flags;
        b->world = world;
        
        /* Set position. */
        b->pos = prop_new();
        b->pos->_.vectf.start = pos;
        b->prevstep_pos = pos;
        
        /* Add to parent's child list. */
        if (parent != NULL) {
                b->parent = parent;
                DL_APPEND(parent->children, b);
        }
#if !ALL_NOCTURNAL
        /* If body does not sleep, add to nocturnal list. */
        if (flags & BODY_NOCTURNAL)
                DL_APPEND_P(world->nocturnal, b, nocturnal_);
#endif
}

static void
body_bb_changed(Body *b)
{
#if ENABLE_TILE_GRID
        /* Update tiles. */
        for (Tile *t = b->tiles; t != NULL; t = t->next) 
                tile_bb_changed(t);
#endif
        /* Update shapes. */
        for (Shape *s = b->shapes; s != NULL; s = s->next)
                shape_bb_changed(s);
                
        /* Update children bodies. */
        for (Body *child = b->children; child != NULL; child = child->next)
                body_bb_changed(child);        
}

/*
 * Get position of body relative to world origin.
 */
vect_f
body_absolute_pos(Body *b)
{
        vect_f pos = body_pos(b);
        while ((b = b->parent) != NULL) {
                vect_f parent_pos = body_pos(b);
                pos.x += parent_pos.x;
                pos.y += parent_pos.y;
        }
        return pos;
}

void
body_sweep_bb(Body *b, BB *bb)
{
        do {
                /* Translate box to body position. */
                vect_f start_pos = b->pos->_.vectf.start;
                bb_add_vect(bb, (vect_i){posround(start_pos.x), 
                                         posround(start_pos.y)});
                
                /* Sweep box along motion vector if necessary. */
                if (b->pos->anim_type != ANIM_NONE) {
                        vect_f motion = vect_f_sub(b->pos->_.vectf.end,
                                                   start_pos);
                        if (motion.x > 0)
                                bb->r += ceilf(motion.x);
                        else
                                bb->l += floorf(motion.x);
                        
                        if (motion.y > 0)
                                bb->t += ceilf(motion.y);
                        else
                                bb->b += floorf(motion.y);
                }
        } while ((b = b->parent) != NULL);      /* Move up the hierarchy. */
}

/*
 * Set body `pause` flag.
 */
void
body_pause(Body *b)
{
        b->flags |= BODY_PAUSED;
}

/*
 * Unset body `pause` flag.
 */
void
body_resume(Body *b)
{
        b->flags &= ~BODY_PAUSED;
}

#if TRACE_MAX
/*
 * Allocate trace arrays for body and its tiles and shapes.
 */
static void
allocate_trace(Body *b)
{
        /* Allocate buffer for body states. */
        extern mem_pool mp_bodytrace;
        assert(!body_traced(b) && b->trace == NULL);
        b->trace = mp_alloc(&mp_bodytrace);
        
        /* Allocate trace buffer for each tile. */
        Tile *t;
        DL_FOREACH(b->tiles, t) {
                extern mem_pool mp_tiletrace;
                assert(t->trace == NULL);
                t->trace = mp_alloc(&mp_tiletrace);
        }
        
        /* Allocate trace buffer for each shape. */
        Shape *s;
        DL_FOREACH(b->shapes, s) {
                extern mem_pool mp_shapetrace;
                assert(s->trace == NULL);
                s->trace = mp_alloc(&mp_shapetrace);
        }
}

void
body_start_recording(Body *b)
{
        allocate_trace(b);
        b->flags |= BODY_TRACED;
}

/*
 * Store current state in body's `trace` array. Returns body time.
 */
float
body_snapshot(Body *b)
{
        /* Allocate trace arrays if not already allocated. */
        assert(!body_traced(b));
        if (b->trace == NULL)
                allocate_trace(b);
        
        return b->step * b->world->step_sec;
}

void
body_record_trace(Body *b)
{
        unsigned trace_index = b->trace_next++;
        BodyState *bs = &b->trace[trace_index];
        
        /* Free previously stored properties. */
        if (bs->pos != NULL) {
                prop_free(bs->pos);
        }
        
        /* Copy all properties into trace. */
        bs->step = b->step;
        bs->pos = prop_copy(b->pos);    /* Always non-NULL. */
        bs->vel = b->vel;
        bs->step_func = b->step_func;
        bs->step_cb_data = b->step_cb_data;
        
        /* Handle `trace_next` wrap-around. */
        if (b->trace_next == TRACE_MAX)
                b->trace_next = 0;
        
        /* Record all owned tiles. */
        Tile *t;
        DL_FOREACH(b->tiles, t) {
                tile_record_trace(t, trace_index);
        }
        
        /* Record all owned shapes. */
        Shape *s;
        DL_FOREACH(b->shapes, s) {
                shape_record_trace(s, trace_index);
        }
}

static void
restore_body_state(Body *b, BodyState *bs)
{
        /* Free properties. */
        prop_free(b->pos);
        
        /* Copy body state. */
        b->step = bs->step;
        b->pos = prop_copy(bs->pos);
        b->vel = bs->vel;
        b->step_func = bs->step_func;
        b->step_cb_data = bs->step_cb_data;
}

static void
restore_tile_state(Tile *t, TileState *ts)
{
        /* Free properties. */
        prop_free(t->pos);
        prop_free(t->size);
        if (t->frame != NULL)
                prop_free(t->frame);
        if (t->color != NULL)
                prop_free(t->color);
        if (t->angle != NULL)
                prop_free(t->angle);
        
        /* Copy properties. */
        t->pos = prop_copy(ts->pos);
        t->size = prop_copy(ts->size);
        t->frame = ts->frame != NULL ? prop_copy(ts->frame) : NULL;
        t->color = ts->color != NULL ? prop_copy(ts->color) : NULL;
        t->angle = ts->angle != NULL ? prop_copy(ts->angle) : NULL;
}

static void
restore_shape_state(Shape *s, ShapeState *ss)
{
        /* Free properties. */
        prop_free(s->def);
        
        /* Copy properties. */
        s->def = prop_copy(ss->def);
}

/* Incremented/decremented trace index (handle wrap-around). */
#define NEXT_TRACEI(i) ((i) != TRACE_MAX - 1 ? (i) + 1 : 0)
#define PREV_TRACEI(i) ((i) != 0             ? (i) - 1 : TRACE_MAX - 1)

static BodyState *
rewind_body_state(Body *b, float step, unsigned *state_index)
{
        /* Presently selected saved state. */
        int first_index = PREV_TRACEI(b->trace_next);
        BodyState *cs = &b->trace[first_index];
        if (cs->pos == NULL)
                return NULL;      /* State does not exist. */
       
        if (step <= cs->step) {
                /* Go left. */
                int ci = first_index;
                do {
                        cs = &b->trace[ci];
                        int state_i = PREV_TRACEI(ci);
                        BodyState *state = &b->trace[state_i];
                        if (state->pos == NULL) {
                                b->trace_next = NEXT_TRACEI(ci);
                                *state_index = ci;
                                return cs;      /* State does not exist. */
                        }
                        if (step >= state->step) {
                                /* See which state is nearer. */
                                if (step - state->step < cs->step - step) {
                                        b->trace_next = NEXT_TRACEI(state_i);
                                        *state_index = state_i;
                                        return state;
                                }
                                b->trace_next = NEXT_TRACEI(ci);
                                *state_index = ci;
                                return cs;
                        }
                        ci = state_i;
                } while (ci != first_index);
               
                /* Went over all states and nothing was found. */
                return NULL;
        }
       
        /* Go right. */
        int ci = first_index;
        do {
                cs = &b->trace[ci];
                int state_i = NEXT_TRACEI(ci);
                BodyState *state = &b->trace[state_i];
                if (state->pos == NULL) {
                        b->trace_next = state_i;
                        *state_index = ci;
                        return cs;      /* State does not exist. */
                }
                if (step <= state->step) {
                        /* See which state is nearer. */
                        if (state->step - step < step - cs->step) {
                                b->trace_next = NEXT_TRACEI(state_i);
                                *state_index = state_i;
                                return state;
                        }
                        b->trace_next = state_i;
                        *state_index = ci;
                        return cs;
                }
                ci = state_i;
        } while (ci != first_index);
       
        /* Went over all states and nothing was found. */
        return NULL;
}

/*
 * Rewind body to a recorded state `time` seconds ago. Since not every time
 * value corresponds directly to a recorded state, closest recorded state is
 * chosen. If a reasonably matching state cannot be found, the function returns
 * false; otherwise it returns true. If body was successfully rewound, the
 * actual time from that trace is stored in `rewound_time`.
 */
int
body_rewind(Body *b, float time, float *rewound_time)
{
        assert(b && time >= 0.0 && rewound_time);
        
        /* Rewind and restore body state. */
        float step_sec = b->world->step_sec;
        unsigned state_index;
        BodyState *bs = rewind_body_state(b, time / step_sec, &state_index);
        if (bs == NULL) {
                log_warn("INDEX NOT FOUND");
                return 0;
        }
        restore_body_state(b, bs);
        
        /* Restore tile state. */
        Tile *t;
        DL_FOREACH(b->tiles, t) {
                restore_tile_state(t, &t->trace[state_index]);
        }
        
        /* Rewind and restore shape state. */
        Shape *s;
        DL_FOREACH(b->shapes, s) {
                restore_shape_state(s, &s->trace[state_index]);
        }
        
        /* Update grid. */
        body_bb_changed(b);

        *rewound_time = b->step * step_sec;
        return 1;
}

#endif  /* TRACE_MAX */

/*
 * Destroy stuff owned by body, but not the body itself.
 */
void
body_destroy(Body *b)
{
        /* Remove body from parent's child list. */
        if (b->parent != NULL)
                DL_DELETE(b->parent->children, b);
        
        /* Destroy children. */
        while (b->children != NULL)
                body_free(b->children);

        /* Free owned tiles. */
        while (b->tiles != NULL)
                tile_free(b->tiles);

        /* Free owned shapes. */
        while (b->shapes != NULL)
                shape_free(b->shapes);
        
        /* Clear timers. */
        Timer *timer, *tmp;
        DL_FOREACH_SAFE(b->timer_list, timer, tmp) {
                DL_DELETE(b->timer_list, timer);
                timer_free(timer, 1);
        }
        
        /* Free properties. */
        prop_free(b->pos);

#if TRACE_MAX
        /* Free trace properties and the trace array itself. */
        BodyState *trace = b->trace;
        if (trace != NULL) {
                /* Free properties. */
                for (unsigned i = 0; i < TRACE_MAX; i++) {
                        if (trace[i].pos != NULL) {
                                prop_free(trace[i].pos);
                        }
                }
                
                /* Free trace array. */
                extern mem_pool mp_bodytrace;
                mp_free(&mp_bodytrace, trace);
        }
#endif
        
#if !ALL_NOCTURNAL
        /* Remove from nocturnal list if necessary. */
        if (b->nocturnal_prev != NULL || b->nocturnal_next != NULL) {
                DL_DELETE_P(b->world->nocturnal, b, nocturnal_);
        }
#endif
}

void
body_free(Body *body)
{
        extern mem_pool mp_body;

        assert(body != &body->world->static_body);
        body_destroy(body);
        mp_free(&mp_body, body);
}

Body *
body_new(Body *parent, vect_f pos, unsigned flags)
{
        extern mem_pool mp_body;
        Body *b = mp_alloc(&mp_body);
        body_init(b, parent, parent->world, pos, flags);
        return b;
}

Body *
body_clone(const Body *orig)
{
        /* Allocate and set objtype. */
        extern mem_pool mp_body;
        Body *b = mp_alloc(&mp_body);
        b->objtype = OBJTYPE_BODY;
        
        /* Copy properties. */
        b->world = orig->world;
        b->pos = prop_copy(orig->pos);
        b->vel = orig->vel;
        b->acc = orig->acc;
        b->prevstep_pos = orig->prevstep_pos;
        b->flags = orig->flags;
        
        /* Copy step function data. */
        b->step = orig->step;
        b->step_func = orig->step_func;
        b->step_cb_data = orig->step_cb_data;
        b->afterstep_func = orig->afterstep_func;
        b->afterstep_cb_data = orig->afterstep_cb_data;
        
        /* Clone owned tiles. */
        Tile *t;
        DL_FOREACH(orig->tiles, t) {
                DL_APPEND(b->tiles, tile_clone(b, t));
        }
        
        /* Clone owned shapes. */
        Shape *s;
        DL_FOREACH(orig->shapes, s) {
                DL_APPEND(b->shapes, shape_clone(b, s));
        }
        
        /* Add to parent's child list. */
        Body *parent = orig->parent;
        if (parent != NULL) {
                b->parent = parent;
                DL_APPEND(parent->children, b);
        }
#if !ALL_NOCTURNAL
        /* If body does not sleep, add to nocturnal list. */
        if (b->flags & BODY_NOCTURNAL)
                DL_APPEND_P(b->world->nocturnal, b, nocturnal_);
#endif
        return b;
}

/*
 * Execute body's step function.
 *
 * body         The body whose step function will be called.
 * L            Lua state (NULL if Lua is not used).
 * script_ptr   For normal Body objects, this is the same as the 'body' pointer.
 *              For objects such as Camera and Parallax (they contain Body
 *              objects), this should point to the Camera or Parallax objects
 *              respectively. This 'script_ptr' is the pointer that Lua scripts
 *              are getting. And scripts should not have access to the
 *              underlying body object; instead they assume that they are
 *              dealing directly with Camera, Parallax, etc.
 */
void
body_step(Body *body, lua_State *L, void *script_ptr)
{
        /* Increment step number; do nothing if step function is not set. */
        assert(body_active(body));
        body->step++;
        if (body->step_func == 0)
                return;
        
        /* If C version of step function exists, run it. */
        if (body->flags & BODY_STEP_C) {
                StepFunction sf = (StepFunction)body->step_func;
                sf(L, script_ptr, body->step_cb_data);
                return;
        }
#if ENABLE_LUA
        /* Push arguments. */
        extern int callfunc_index;
        lua_pushvalue(L, callfunc_index);               /* ... callfunc */
        assert(lua_isfunction(L, -1));                  
        lua_pushinteger(L, body->step_func);            /* + func_id */
        lua_pushinteger(L, body->step_cb_data);         /* + arg_id */
        lua_pushboolean(L, 0);                          /* + false */
        lua_pushlightuserdata(L, script_ptr);           /* + script_ptr */
        
        /* Execute Lua function. */
        extern int errfunc_index;
        if (lua_pcall(L, 4, 0, errfunc_index))
                fatal_error("[Lua] %s", lua_tostring(L, -1));
#endif  /* ENABLE_LUA */
}

/*
 * Almost the same thing as body_step(), only this one should be executed after
 * collision detection/response has been performed instead of before. It runs
 * Lua (or C) function identified by afterstep_func member, rather than
 * step_func.
 */
void
body_afterstep(Body *body, lua_State *L, void *script_ptr)
{
        /* Do nothing if afterstep function is not set. */
        assert(body_active(body));
        if (body->afterstep_func == 0)
                return;
        
        /* Run C version of afterstep function if it exists. */
        if (body->flags & BODY_AFTERSTEP_C) {
                AfterStepFunction asf = (AfterStepFunction)body->afterstep_func;
                asf(L, script_ptr, body->afterstep_cb_data);
                return;
        }
#if ENABLE_LUA
        /* Push arguments. */
        extern int callfunc_index;
        lua_pushvalue(L, callfunc_index);               /* ... callfunc */
        assert(lua_isfunction(L, -1));                  
        lua_pushinteger(L, body->afterstep_func);       /* + func_id */
        lua_pushinteger(L, body->afterstep_cb_data);    /* + arg_id */
        lua_pushboolean(L, 0);                          /* + false */
        lua_pushlightuserdata(L, script_ptr);           /* + script_ptr */
        
        /* Execute Lua function. */
        extern int errfunc_index;
        if (lua_pcall(L, 4, 0, errfunc_index))
                fatal_error("[Lua] %s", lua_tostring(L, -1));
#endif  /* ENABLE_LUA */
}

vect_f
body_pos(Body *b)
{
        Property *anim = b->pos;
        if (anim->anim_type == ANIM_NONE)
                return anim->_.vectf.start;         /* No animation. */
        
        vect_f start = anim->_.vectf.start;
        vect_f end = anim->_.vectf.end;
        
        /* Get current time, and time since animation start. */
        World *world = b->world;
        float now = b->step * world->step_sec;
        float delta = now - anim->start_time;
        float duration = anim->duration;
        
        /* If animation has not started yet, return start value. */
        if (delta <= 0.0)
                return start;
        
        /* Calculate current value. */
        switch (anim->anim_type) {
        case ANIM_LOOP: {
                delta = fmod(delta, duration);
                return (vect_f){
                        start.x + (end.x - start.x) * delta / duration,
                        start.y + (end.y - start.y) * delta / duration
                };
        }
        case ANIM_CLAMP: {
                if (delta >= duration) {
                        body_set_pos(b, end);
                        return end;
                }
                return (vect_f){
                        interp_linear(start.x, end.x, duration, delta),
                        interp_linear(start.y, end.y, duration, delta)
                };
        }
        case ANIM_CLAMP_EASEIN: {
                if (delta >= duration) {
                        body_set_pos(b, end);
                        return end;
                }
                return (vect_f){
                        interp_easein(start.x, end.x, duration, delta),
                        interp_easein(start.y, end.y, duration, delta)
                };
        }
        case ANIM_CLAMP_EASEOUT: {
                if (delta >= duration) {
                        body_set_pos(b, end);
                        return end;
                }
                return (vect_f){
                        interp_easeout(start.x, end.x, duration, delta),
                        interp_easeout(start.y, end.y, duration, delta)
                };
        }
        case ANIM_CLAMP_EASEINOUT: {
                if (delta >= duration) {
                        body_set_pos(b, end);
                        return end;
                }
                return (vect_f){
                        interp_easeinout(start.x, end.x, duration, delta),
                        interp_easeinout(start.y, end.y, duration, delta)
                };
        }
        }
        fatal_error("Invalid animation type: (%i).", anim->anim_type);
        abort();
}

/*
 * Change body's position.
 */
void
body_set_pos(Body *b, vect_f pos)
{
        assert(b != &b->world->static_body);
        
        /* Destroy and create property with new value. */
        prop_free(b->pos);
        b->pos = prop_new();
        b->pos->_.vectf.start = pos;
        
        body_bb_changed(b);
}

#define SET_ANIM(body, member, prop, type, start_value, end_value, start_time, duration) \
do { \
        Property *anim = (body)->member; \
        (anim)->_.prop.start = (start_value); \
        (anim)->_.prop.end = (end_value); \
        (anim)->anim_type = (type); \
        (anim)->start_time = (start_time) + (body)->step * (body)->world->step_sec; \
        (anim)->duration = (duration); \
} while (0)

void
body_anim_pos(Body *b, uint8_t type, vect_f end, float duration,
              float start_time)
{
        /* Start value. */
        assert(b != &b->world->static_body);
        vect_f start_value = body_pos(b);
        
        /* Destroy previous animation and create a new one. */
        if (b->pos != NULL)
                prop_free(b->pos);
        b->pos = prop_new();
        
        SET_ANIM(b, pos, vectf, type, start_value, end, start_time, duration);        
        body_bb_changed(b);
}

/*
 * Add a timer to world.
 *
 * body         Body that you want to add the timer to.
 * when         When is this timer supposed to run (offset since now).
 * type         OBJTYPE_TIMER_C or OBJTYPE_TIMER_LUA.
 * func         Lua function ID or C function pointer to invoke once it's time.
 * data         User callback data.
 */
Timer *
body_add_timer(Body *b, void *owner, float when, int type, intptr_t func,
               intptr_t data)
{
        unsigned sched = (unsigned)lroundf(b->step + when / b->world->step_sec);
        Timer *timer = timer_new(owner, b->step, sched, type, func, data);
        
        if (b->timer_list == NULL || sched <= b->timer_list->scheduled) {
                DL_PREPEND(b->timer_list, timer);    /* As first. */
                return timer;
        }
        if (sched >= b->timer_list->prev->scheduled) {
                DL_APPEND(b->timer_list, timer);
                return timer;                        /* As last. */
        }
        
        /* Find spot where we can insert. */
        for (Timer *iter = b->timer_list->next;; iter = iter->next) {
                assert(iter != NULL);
                if (sched <= iter->scheduled) {
                        timer->prev = iter->prev;
                        timer->next = iter;
                        iter->prev->next = timer;
                        iter->prev = timer;
                        return timer;
                }
        }
        abort();
}

void
body_cancel_timer(Body *body, Timer *timer)
{
        /* Remove from list and free timer memory. */
        DL_DELETE(body->timer_list, timer);
        timer_free(timer, 1);
}

void
body_run_timers(Body *body, lua_State *L)
{
        /* Get current time in seconds. */
        assert(body_active(body));
        unsigned now = body->step;

        /*
         * Build temporary array of timers to be executed: we do this because
         * timers may cancel other timers.
         */
        unsigned num_timers = 0;
        Timer *timer_array[50];
        for (Timer *tmr = body->timer_list; tmr != NULL; tmr = tmr->next) {
                /*
                 * As soon as we reach a timer that has been scheduled for
                 * later we can bail out because the list is sorted.
                 */
                if (tmr->scheduled > now)
                        break;
                
                assert(num_timers < ARRAYSZ(timer_array));
                timer_array[num_timers++] = tmr;
        }
        
        /* Run timers that were put into temporary array. */
        for (unsigned i = 0; i < num_timers; i++) {
                Timer *timer = timer_array[i];
                if (timer->objtype == 0)
                        continue;       /* Destroyed timer. */
                        
                int objtype = timer->objtype;
                void *owner = timer->owner;
                intptr_t data = timer->data;
                intptr_t func = timer->func;
                
                /* Remove timer from list and destroy it. */
                DL_DELETE(body->timer_list, timer);
                timer_free(timer, 0);
                
                if (objtype == OBJTYPE_TIMER_C) {                        
                        ((TimerFunction)func)(owner, data);
                        continue;
                }
#if ENABLE_LUA
                /* Push arguments. */
                assert(objtype == OBJTYPE_TIMER_LUA);
                extern int callfunc_index;
                lua_pushvalue(L, callfunc_index);   /* ... callfunc */
                assert(lua_isfunction(L, -1));
                lua_pushinteger(L, func);           /* + func_id */
                lua_pushinteger(L, data);           /* + arg_id  */
                lua_pushboolean(L, 1);              /* + false.  */
                lua_pushlightuserdata(L, owner);    /* + owner   */
                
                /* Call Lua function. */
                extern int errfunc_index;
                if (lua_pcall(L, 4, 0, errfunc_index))
                        fatal_error("[Lua] %s", lua_tostring(L, -1));
#else
                abort();
#endif
        }
}

