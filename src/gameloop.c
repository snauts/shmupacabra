#include "camera.h"
#include "config.h"
#include "world.h"
#include "render.h"
#include "gameloop.h"
#include "audio.h"

Camera  *cam_list;      /* List of all cameras (sorted by "sort" value). */
Camera  *debug_cam;     /* Camera rendering debugging visuals. */

/* Keep track of last frame time. */
static uint32_t before;

/* FPS variables. */
static uint32_t fps_time;       /* Last FPS update time. */
static int fps_count;
static float frames_per_second;

/*
 * Game time is the time spent actually advancing (stepping) worlds. It can be
 * less than actual time (time since program start) due to lag or someone
 * suspending engine process.
 */
uint64_t game_time;

void
run_game(lua_State *L)
{
        uint32_t now = SDL_GetTicks();   /* Current real time. */
        
        /*
         * Compute how much time has passed since last time. Watch out for time
         * wrap-around.
         */
        uint32_t delta_time = (now >= before) ? now - before :
        (uint32_t)-1 - before + now;
        before = now;
        
        /*
         * If there was some huge lag, don't make worlds catch
         * up. Instead, assume last frame took 50ms.
         */
        if (delta_time > 50)
                delta_time = 50;
        
        /* Game speed always normal. */
        uint32_t game_delta_time = delta_time;
        game_time += game_delta_time;	/* Advance game time. */
        
        /* Calculate frames per second. */
        fps_count++;
        if (now - fps_time >= config.FPSUpdateInterval && config.debug) {
                frames_per_second = fps_count*1000.0 / (now - fps_time);
                fps_time = now;
                fps_count = 0;
		extern int total_tile_count;
                log_msg("FPS: %.2f, tiles=%d", 
			frames_per_second,
			total_tile_count);
        }
        
        process_events(L);
        
#if ENABLE_AUDIO
        /* Dynamically adjust volume. */
        audio_adjust_volume();
#endif
        
        /* Step worlds. */
        extern mem_pool mp_world;
        for (World *world = mp_first(&mp_world); world != NULL;
             world = mp_next(world)) {
                if (world->killme)
                        continue;       /* We deal with these below. */
                
                /* Bring world up to present game time. */
                while (game_time >= world->next_step_time) {
                        world->next_step_time += world->step_ms;
                        
                        /*
                         * Step world -- execute body step functions, timers,
                         * collision handlers.
                         */
                        world_step(world, L);
                        
                        /*
                         * Handle user input. To be more responsive, we do this
                         * here between steps too.
                         */
                        process_events(L);
                        
                        if (world->killme)
                                break;	/* No need to keep going. */
                }
        }
        
        /*
         * Deal with worlds that have either been destroyed or created in the
         * loop above. Must do this here so scripts get a chance to set
         * everything up before a frame is rendered.
         */
        for (World *world = mp_first(&mp_world); world != NULL;) {
                if (world->killme) {
                        /*
                         * Remove dying worlds. Take care to get next world
                         * pointer before destruction.
                         */
                        World *tmp = world;
                        world = mp_next(world);
                        world_free(tmp);
                        continue;
                }
                
                /* Perform the initial step on recently created worlds. */
                if (world->static_body.step == 0) {
                        /*
                         * We must give scripts control over what the contents
                         * of the world look like before drawing it. Otherwise
                         * we get such artifacts as camera centered on origin
                         * even though it should be tracking a player character.
                         */
                        world_step(world, L);
                }
                world = mp_next(world);
        }
        
        /*
         * Draw what each camera sees.
         */
        render_clear();
        for (Camera *cam = cam_list; cam != NULL; cam = cam->next) {
                if (!cam->disabled)
                        render(cam);
        }
        
#ifndef NDEBUG
        /* Debug stuff is drawn over normal stuff. */
        if (debug_cam != NULL && debug_cam->objtype == OBJTYPE_CAMERA)
                render_debug(debug_cam);
#endif
}
