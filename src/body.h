#ifndef GAME2D_BODY_H
#define GAME2D_BODY_H

#include "common.h"
#include "geometry.h"
#include "property.h"
#include "timer.h"
#include "grid.h"
#include "uthash_tuned.h"

struct Tile_t;
struct Shape_t;
struct World_t;

#if TRACE_MAX
/*
 * BodyState records body state at a given moment in time.
 */
typedef struct {
        unsigned step;
        Property *pos;
        vect_f   vel;
        intptr_t step_func, step_cb_data;
} BodyState;
#endif  /* TRACE_MAX */

/*
 * Body flags:
 *
 * BODY_NOCTURNAL       If set, body is always active, even if outside camera
 *                      vicinity.
 * BODY_STEP_C          Step function is a C function.
 * BODY_AFTERSTEP_C     Afterstep function is a C function.
 * BODY_VISITED         Used within world_step() to check for duplicate bodies.
 * BODY_SMOOTH_POS      Do not round body positions during rendering.
 */
enum {
        BODY_NOCTURNAL   = 1<<1,
        BODY_STEP_C      = 1<<2,
        BODY_AFTERSTEP_C = 1<<3,
        BODY_VISITED     = 1<<4,
#if !ALL_SMOOTH
        BODY_SMOOTH_POS  = 1<<5,
#endif
        BODY_PAUSED      = 1<<6,
        BODY_TRACED      = 1<<7
};

typedef struct Body_t {
        int             objtype;        /* = OBJTYPE_BODY */
        struct World_t *world;          /* World body belongs to. */
        
        Property        *pos;           /* Offset from parent body. */
        vect_f          vel;            /* Velocity. */
        vect_f          acc;            /* Acceleration. */
        vect_f          prevstep_pos;   /* Position in the previous step. */
        unsigned        flags;          /* Misc state. */
        
#if TRACE_MAX
        /* Remember previous body states to be able to rewind time. */
        BodyState       *trace;
        int             trace_next;
#endif
        struct Tile_t   *tiles;         /* List of tiles. */
        struct Shape_t  *shapes;        /* List of shapes. */
        
        /*
         * Body step functions are executed every world step before collision
         * handling. After-step functions are executed after collision handling
         * has been performed.
         *
         * Step (and after-step) function are not called if body is far
         * off-screen.
         */
        unsigned        step;                   /* Step number. */
        intptr_t        step_func;
        intptr_t        step_cb_data;           /* User data pointer. */
        intptr_t        afterstep_func;
        intptr_t        afterstep_cb_data;      /* User data pointer. */
        
        /* List of timers bound to the body. */
        Timer           *timer_list;
                
        /* Children list. */
        struct Body_t   *parent, *children;
        struct Body_t   *prev, *next;
#if !ALL_NOCTURNAL
        struct Body_t   *nocturnal_prev, *nocturnal_next;
#endif
} Body;

/* Step function types. */
typedef void (*StepFunction)(lua_State *L, void *body, intptr_t data);
typedef void (*AfterStepFunction)(lua_State *L, void *body, intptr_t data);

void     body_sweep_bb(Body *b, BB *bb);

/* Position. */
vect_f   body_pos(Body *b);
vect_f   body_absolute_pos(Body *b);
void     body_set_pos(Body *b, vect_f pos);
void     body_anim_pos(Body *b, uint8_t type, vect_f end, float duration,
                       float start_time);

/* Lifetime. */
void     body_init(Body *b, Body *parent, struct World_t *world, vect_f pos,
                   unsigned flags);
Body    *body_new(Body *parent, vect_f pos, unsigned flags);
Body    *body_clone(const Body *orig);
void     body_destroy(Body *b);
void     body_free(Body *b);

/* Recording trace. */
void     body_record_trace(Body *b);
void     body_start_recording(Body *b);
#define  body_traced(b) ((b)->flags & BODY_TRACED)
float    body_snapshot(Body *b);
int      body_rewind(Body *b, float time, float *rew_time);
void     body_pause(Body *b);
#define  body_active(b) (!((b)->flags & BODY_PAUSED))
void     body_resume(Body *b);

/* Step function and timers. */
void     body_step(Body *b, lua_State *L, void *script_ptr);
void     body_afterstep(Body *b, lua_State *L, void *script_ptr);

Timer   *body_add_timer(Body *body, void *owner, float when, int type,
                        intptr_t func, intptr_t data);
void     body_cancel_timer(Body *body, Timer *timer);
void     body_run_timers(Body *body, lua_State *L);

#endif /* GAME2D_BODY_H */
