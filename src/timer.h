#ifndef GAME2D_TIMER_H
#define GAME2D_TIMER_H

#include "common.h"

struct Timer_t;

/* Timer function. */
typedef void (*TimerFunction)(void *obj, intptr_t data);
typedef void (*TimerClear)(struct Timer_t *t);

/*
 * Timer structure holds a task that has been scheduled to run at a later time.
 *
 * objtype      OBJTYPE_TIMER_LUA or OBJTYPE_TIMER_C, depending on what the
 *              "func" member designates.
 * owner        Object that owns the timer.
 * timer_id     User scripts may hold on to Timer pointers so that they would be
 *              able to cancel the timer. If a timer has already been executed,
 *              then its memory might be reused by another timer. Thus blindly
 *              cancelling it would be a mistake. timer_id gives us the ability
 *              to tell if user is referencing the correct timer.
 *              Zero is a valid timer_id value.
 * func         TimerFunction pointer or index into eapi.__idToObjectMap for Lua
 *              functions.
 * data         User callback data.
 * created      Step number when timer was created.
 * scheduled    Step number when timer must be executed.
 * canceled     Step number when timer was cancelled. UINT_MAX if not yet
 *              canceled.
 */
typedef struct Timer_t {
        int             objtype;
        
        void            *owner;
        unsigned        timer_id;
        intptr_t        func;
        intptr_t        data;
        
        unsigned        created;
        unsigned        scheduled;
        unsigned        canceled;
        
        TimerClear      clearfunc;
        
        struct Timer_t  *prev, *next;
} Timer;

/*
 * Structure that is returned to user scripts. In addition to a Timer pointer,
 * it also holds the timer_id which lets us know if user is referencing the
 * correct Timer.
 */
typedef struct {
        int       objtype;
        Timer     *ptr;
        unsigned  timer_id;
} TimerPtr;

Timer   *timer_new(void *owner, unsigned now, unsigned sched, int type,
                   intptr_t func, intptr_t data);
void     timer_free(Timer *timer, int clear_state);

#endif /* GAME2D_TIMER_H */
