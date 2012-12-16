#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "mem.h"
#include "timer.h"

Timer *
timer_new(void *owner, unsigned now, unsigned sched, int type, intptr_t func,
          intptr_t data)
{
        assert(owner && func && sched >= now);
        assert(type == OBJTYPE_TIMER_C || type == OBJTYPE_TIMER_LUA);
        
        /* Timer IDs keep increasing; zero is not a valid ID. */
        static unsigned timer_id_gen;
        
        /* Allocate. */
        extern mem_pool mp_timer;
        Timer *timer = mp_alloc(&mp_timer);
        
        /* Initialize. */
        timer->objtype = type;
        timer->owner = owner;
        timer->func = func;
        timer->data = data;
        timer->created = now;
        timer->scheduled = sched;
        timer->canceled = UINT_MAX;
        
        /* Timer ID wrap-around check. */
        timer->timer_id = ++timer_id_gen ? timer_id_gen : ++timer_id_gen;
        
        return timer;
}

void
timer_free(Timer *timer, int clear_state)
{
        /*
         * Execute destructor if set (used for clearing associated script
         * state).
         */
        if (clear_state && timer->clearfunc != NULL) {
                timer->clearfunc(timer);
        }

        extern mem_pool mp_timer;
        mp_free(&mp_timer, timer);
}
