#include <SDL.h>
#include <assert.h>
#include <math.h>
#include "log.h"
#include "camera.h"
#include "config.h"
#include "shape.h"
#include "tile.h"
#include "utlist.h"
#include "world.h"
#include "util_lua.h"

/*
 * Remember current body positions.
 */
static void
save_state(World *world, Body  *active_bodies[], unsigned num_bodies)
{
        /* Save active body positions. */
        Body *b;
        for (unsigned i = 0; i < num_bodies; i++) {
                b = active_bodies[i];
                b->prevstep_pos = body_pos(b);                
        }

        /* Save positions of camera bodies. */
        extern Camera *cam_list;
        Camera *cam;
        DL_FOREACH(cam_list, cam) {
                if (cam->body.world != world || cam->disabled)
                        continue;
                cam->body.prevstep_pos = body_pos(&cam->body);
        }
#if TRACE_MAX
        /* See if this trace recording step should be skipped. */
        if (world->static_body.step % (world->trace_skip + 1) != 0)
                return;

        /* Record static body state. */
        if (body_traced(&world->static_body))
                body_record_trace(&world->static_body);
        
        /* Record active body state. */
        for (unsigned i = 0; i < num_bodies; i++) {
                Body *b = active_bodies[i];
                if (body_traced(b))
                        body_record_trace(b);
        }
        
        /* Record camera body state. */
        DL_FOREACH(cam_list, cam) {
                if (cam->body.world != world || cam->disabled)
                        continue;
                if (body_traced(&cam->body))
                        body_record_trace(&cam->body);
        }
#endif /* !TRACE_MAX */
}

static int
invoke_collision_handler(Handler *handler, Shape *A, Shape *B, int new,
                         BB *resolve, lua_State *L)
{
        if (handler->type == HANDLER_C) {
                CollisionFunc cf = (CollisionFunc)handler->func;
                return cf(A, B, new, resolve, handler->data);
        }
#if ENABLE_LUA
        extern int callfunc_index;
        lua_pushvalue(L, callfunc_index);
        assert(lua_isfunction(L, -1));          /* ... callfunc */
        
        assert(handler->type == HANDLER_LUA);
        lua_pushinteger(L, handler->func);      /* ... callfunc func_id */
        lua_pushinteger(L, handler->data);      /* + arg_id */
        lua_pushboolean(L, 0);                  /* + false */                
        
        /* Push shape or `nil` if it was destroyed. */
        if (A != NULL)
                lua_pushlightuserdata(L, A);
        else
                lua_pushnil(L);
                
        /* Push shape or `nil` if it was destroyed. */
        if (B != NULL)
                lua_pushlightuserdata(L, B);
        else
                lua_pushnil(L);
        
        /* Push the resolution info or `nil` for separation callback. */
        if (resolve != NULL)
                L_push_BB(L, *resolve);
        else
                lua_pushnil(L);
        
        /* Push the `new collision` flag. */
        lua_pushboolean(L, new);
        
        /* Call Lua function. */
        extern int errfunc_index;
        if (lua_pcall(L, 7, 1, errfunc_index))
                fatal_error("[Lua] %s", lua_tostring(L, -1));
                
        /* Get user return value. */
        int ignore = lua_toboolean(L, -1);
        lua_pop(L, 1);
        return ignore;
#else
        abort();
#endif
}

static void
add_potential_collisions(Shape *s, Collision *collision_array,
                         unsigned max_collisions, unsigned *num_collisions)
{
        UNUSED(max_collisions);
        
        /*
         * Expand shape bounding box. We want to get all nearby shapes within
         * collision distance.
         */
        BB exp_shape_bb = {
                .l=s->go.area.l - config.collision_dist,
                .b=s->go.area.b - config.collision_dist,
                .r=s->go.area.r + config.collision_dist,
                .t=s->go.area.t + config.collision_dist
        };
            
        /* Get a list of shapes that this one potentially intersects. */
        World *world = s->body->world;
        void *intersect_maybe[1000];
        unsigned num_shapes = grid_lookup(&world->grid, exp_shape_bb,
                                      intersect_maybe, ARRAYSZ(intersect_maybe),
                                      shape_filter);
        
        /*
         * Now iterate over shapes that we found can potentially intersect with
         * shape [s]. As we do this, we keep filling an array of Collision
         * structs. If the shapes have a collision handler registered for them,
         * then an entry is created in this array.
         */
        for (unsigned i = 0; i < num_shapes; i++) {
                Shape *other_s = intersect_maybe[i];
                assert(other_s->group != 0);
                if (s->body == other_s->body)
                        continue;       /* Skip shapes with the same body. */
                
                /*
                 * Find if there's a collision routine registered for these
                 * shapes.
                 */
                unsigned g1_index = s->group->index;
                unsigned g2_index = other_s->group->index;
                Handler *handler = &world->handler_map[g1_index][g2_index];
                if (handler->func != 0) {
                        assert(*num_collisions < max_collisions);
                        Collision *col = &collision_array[(*num_collisions)++];
                        col->handler = *handler;
                        col->shape_A = s;
                        col->shape_B = other_s;
                        col->group_A = s->group;
                        col->group_B = other_s->group;
                }
        }
}

/*
 * Comparison function used by qsort(). Compares Collision struct priorities in
 * such a way that those Collisions with higher priority end up in the beginning
 * of the array.
 * Also note that in cases where priorities are equal, the two collision structs
 * are compared by their shape pointers. It is done in this way so we could
 * later (while iterating over the array) identify and discard duplicate
 * collision structs.
 */
static inline int
collision_priority_cmp(const void *a, const void *b)
{
        const Collision *ca = a;
        const Collision *cb = b;
                
        if (ca->handler.priority == cb->handler.priority) {
                if (ca->shape_A == cb->shape_A) {
                        if (ca->shape_B == cb->shape_B)
                                return 0;
                        return (ca->shape_B < cb->shape_B) ? 1 : -1;
                }
                return (ca->shape_A < cb->shape_A) ? 1 : -1;
        }
        return (ca->handler.priority < cb->handler.priority) ? 1 : -1;
}

/*
 * Go through all shapes that belong to dynamic bodies (bodies whose position
 * could have changed during step function calls) and execute collision handlers
 * for those shapes that intersect.
 */
static void
resolve_collisions(World *world, Shape *active_shapes[], unsigned num_shapes,
                   lua_State *L)
{
        /* Create an array of potential collisions. */
        unsigned num_collisions = 0;
        Collision collision_array[1000];
        for (unsigned i = 0; i < num_shapes; i++) {
                Shape *s = active_shapes[i];
                if (s->body == NULL || s->body->world != world)
                        continue;       /* Shape was Destroy()ed. */
                add_potential_collisions(s, collision_array,
                                         ARRAYSZ(collision_array),
                                         &num_collisions);
        }
        
        /*
         * Sort collisions by priority. Then iterate over them and execute their
         * handler functions.
         */
        unsigned keylen = offsetof(Collision, shape_B) + sizeof(Shape *) -
                      offsetof(Collision, shape_A);
        qsort(collision_array, num_collisions, sizeof(Collision),
              collision_priority_cmp);
        for (unsigned i = 0; i < num_collisions; i++) {
                Collision *col = &collision_array[i];
                Shape *shape_A = col->shape_A;
                Shape *shape_B = col->shape_B;
                
                /* 
                 * It is possible (if unlikely) that a shape was destroyed, and
                 * then some other shape reused its memory. Here we make sure
                 * that the shape that's currently there belongs to this world
                 * and collision group is the same as before. So even if it is
                 * a different shape, at least it's kind of like the old one.
                 */
                if (shape_A->body == NULL ||
                    shape_B->body == NULL ||
                    shape_A->body->world != world ||
                    shape_B->body->world != world ||
                    shape_A->group != col->group_A ||
                    shape_B->group != col->group_B)
                        continue;
                
                /* Compute resolution box. */
                BB resolve;
                if (!shape_vs_shape(shape_B, shape_A, &resolve))
                        continue;       /* No collision. */
#ifndef NDEBUG
                /*
                 * Mark shapes as intersecting. This means they will be
                 * drawn in a different color than non-colliding shapes.
                 */
                shape_A->flags |= SHAPE_INTERSECT;
                shape_B->flags |= SHAPE_INTERSECT;
#endif
                /* See if collision for shape pair already exists. */
                int new = 0;
                Collision *past_col;
                HASH_FIND(hh, world->collisions, &col->shape_A, keylen,
                          past_col);
                if (past_col == NULL) {
                        new = 1;
                        
                        /* Allocate and setup collision struct. */
                        extern mem_pool mp_collision;
                        past_col = mp_alloc(&mp_collision);
                        *past_col = *col;
                        past_col->ignore = 0;
                        
                        /* Add to hash. */
                        HASH_ADD(hh, world->collisions, shape_A, keylen,
                                 past_col);
                }
                
                /*
                 * Mark collision as active and invoke collision handler
                 * if necessary.
                 */
                past_col->active = 1;
                if (!past_col->ignore && (new || col->handler.update)) {
                        past_col->ignore = invoke_collision_handler(
                            &col->handler, shape_A, shape_B, new, &resolve, L);
                }
        }
        
        /* Invoke separation handlers for inactive collisions. */
        Collision *col, *tmp_col;
        HASH_ITER(hh, world->collisions, col, tmp_col) {
                if (col->active) {
                        /* Unset `active` flag and move on to next collision. */
                        col->active = 0;
                        continue;
                }
                
                Shape *shape_A = col->shape_A;
                Shape *shape_B = col->shape_B;
                
                /* If shape A (or B) was destroyed, set its pointer to NULL. */
                if (shape_A->body == NULL ||
                    shape_A->body->world != world ||
                    shape_A->group != col->group_A) {
                        shape_A = NULL;
                }
                if (shape_B->body == NULL ||
                    shape_B->body->world != world ||
                    shape_B->group != col->group_B) {
                        shape_B = NULL;
                }
                
                /* Invoke collision handler if necessary. */
                if (!col->ignore) {
                        invoke_collision_handler(&col->handler, shape_A,
                                                 shape_B, 0, NULL, L);
                }
                
                /* Destroy collision. */
                extern mem_pool mp_collision;
                HASH_DEL(world->collisions, col);
                mp_free(&mp_collision, col);
        }
}

/*
 * Create a new world and return its pointer.
 *
 * world                World about to be intialized.
 * step_ms              World step duration in milliseconds.
 * grid_area            Space partitioning grid area.
 * cell_size            Size of each rectangular grid cell.
 */
World *
world_new(const char *name, unsigned step_ms, BB grid_area, unsigned cell_size,
          unsigned trace_skip)
{
        log_msg("Create world '%s'", name);

        /* Allocate. */
        extern mem_pool mp_world;
        World *world = mp_alloc(&mp_world);
        
        world->objtype = OBJTYPE_WORLD;
        world->next_group_id = 1;

        assert(name && strlen(name) < sizeof(world->name));
        strcpy(world->name, name);
        
        assert(step_ms > 0 && step_ms < 1000);
        world->step_ms = step_ms;
        world->step_sec = (float)step_ms / 1000.0;
        world->trace_skip = trace_skip;
        
        extern uint64_t game_time;
        world->next_step_time = game_time;
        
        /* Set up space partitioning. */
        grid_init(&world->grid, grid_area, cell_size);
        
        /* Init static body and return world. */
        body_init(&world->static_body, NULL, world, (vect_f){0.0, 0.0}, 0);
        return world;
}

/*
 * Destroy world and free its memory.
 */
void
world_free(World *world)
{
        log_msg("Destroy world '%s' (%p).", world->name, world);
        
        /* World must be already cleared. */
        assert(world->killme);
        
        /* Free memory. */
        extern mem_pool mp_world;
        mp_free(&mp_world, world);
}

/*
 * Destroy everything owned by world.
 */
void
world_kill(World *world)
{
#ifndef NDEBUG
        /* Report grid usage statistics. */
        if (config.grid_info)
                grid_info(&world->grid, world->name);
#endif
        /* Destroy static body (and all other bodies along with it). */
        body_destroy(&world->static_body);
        
        /* Clear out any cameras that are "filming" this world. */
        extern Camera *cam_list;
        Camera *cam, *cam_tmp;
        DL_FOREACH_SAFE(cam_list, cam, cam_tmp) {
                if (cam->body.world != world)
                        continue;       /* Camera not inside this world. */
         
                /* Remove camera from global list, then free its memory. */
                DL_DELETE(cam_list, cam);
                cam_free(cam);
        }
        
        /* Clear group hash. */
        Group *group;
        extern mem_pool mp_group;
        while (world->groups) {
                group = world->groups;
                HASH_DEL(world->groups, group);
                mp_free(&mp_group, group);
        }
        world->next_group_id = 1;       /* Reset ID counter. */
        
        /* Clear collision handler map. */
        memset(world->handler_map, 0, sizeof(world->handler_map));
        
        /* Destroy any remaining collisions. */
        Collision *col;
        extern mem_pool mp_collision;
        while (world->collisions) {
                col = world->collisions;
                HASH_DEL(world->collisions, col);
                mp_free(&mp_collision, col);
        }
        
        /* Destroy world grid. */
        grid_destroy(&world->grid);
        
        /* Mark world as ready for being freed. */
        world->killme = 1;
}

static void
run_timers(World *world, Body *active_bodies[], unsigned num_active,
           lua_State *L)
{        
        /* Run timers for static body. */
        if (body_active(&world->static_body))
                body_run_timers(&world->static_body, L);
                
        /* Invoke timers for active bodies. */
        Body *b;
        for (unsigned i = 0; i < num_active; i++) {
                if ((b = active_bodies[i])->world != world)
                        continue;       /* Body was Destroy()ed. */
                if (body_active(b))
                        body_run_timers(b, L);
        }
                
        /* Run camera timers. */
        extern Camera *cam_list;
        for (Camera *cam = cam_list; cam != NULL; cam = cam->next) {
                if (cam->body.world != world || cam->disabled)
                        continue;
                if (body_active(&cam->body))
                        body_run_timers(&cam->body, L);
        }
}

static void
step_bodies(World *world, Body *active_bodies[], unsigned num_active,
            lua_State *L, void (*step_func)(Body *, lua_State *, void *))
{
        /* Step static body. */
        if (body_active(&world->static_body))
                step_func(&world->static_body, L, &world->static_body);

        /* Step active bodies. */
        Body *b;
        for (unsigned i = 0; i < num_active; i++) {
                if ((b = active_bodies[i])->world != world)
                        continue;       /* Body was Destroy()ed. */
                if (body_active(b))
                        step_func(b, L, b);
        }
                
        /* Step camera bodies. */
        extern Camera *cam_list;
        for (Camera *cam = cam_list; cam != NULL; cam = cam->next) {
                if (cam->body.world != world || cam->disabled)
                        continue;
                if (body_active(&cam->body))
                        step_func(&cam->body, L, cam);
        }
}

unsigned        g_num_active_bodies;
Body            **g_active_bodies;

unsigned        g_num_active_shapes;
Shape           **g_active_shapes;

#if !ALL_NOCTURNAL && ENABLE_TILE_GRID

static int
smart_filter(void *ptr)
{
        /*
         * Ignore nocturnal bodies and their shapes (added later).
         * Ignore paused bodies.
         */
        Shape *s = ptr;
        Body *b = s->body;      /* Same offset for Shape and Tile. */
        if ((b->flags & BODY_NOCTURNAL) || !body_active(b))
                return 0;
        
        /*
         * Add bodies to array that:
         *   - are not the static body or a camera (parent != NULL);
         *   - have not yet been added (VISITED flag not set);
         */
        if (b->parent != NULL && !(b->flags & BODY_VISITED)) {
                assert(g_num_active_bodies < ACTIVE_BODIES_MAX);
                g_active_bodies[g_num_active_bodies++] = b;
                b->flags |= BODY_VISITED;
        }
        
        /*
         * Add shapes to their array that:
         *   - are shapes (not tiles);
         *   - are not yet added (VISITED flag not set);
         *   - have at least one collision handler registered for them.
         */
        if (s->objtype == OBJTYPE_SHAPE) {
                if (!(s->flags & SHAPE_VISITED) && s->group->num_handlers > 0) {
                        assert(g_num_active_shapes < ACTIVE_SHAPES_MAX);
                        g_active_shapes[g_num_active_shapes++] = s;
                        s->flags |= SHAPE_VISITED;
                }
        }
        return 0;
}
#else   /* !ALL_NOCTURNAL && ENABLE_TILE_GRID */

static void
dumb_add_all(Body *b)
{
        /* Ignore shapes of paused bodies. */
        if (body_active(b)) {
                Shape *s;
                DL_FOREACH(b->shapes, s) {
                        if (s->group->num_handlers == 0)
                                continue;
                        assert(g_num_active_shapes < ACTIVE_SHAPES_MAX);
                        g_active_shapes[g_num_active_shapes++] = s;
                }
        }

        /*
         * Add children to `active_bodies` and descend further down the body
         * hierarchy.
         */
        Body *child;
        DL_FOREACH(b->children, child) {
                if (body_active(child)) {
                        assert(g_num_active_bodies < ACTIVE_BODIES_MAX);
                        g_active_bodies[g_num_active_bodies++] = child;
                }
                
                /* Recursive descent. */
                dumb_add_all(child);
        }
}

#endif  /* ALL_NOCTURNAL || !ENABLE_TILE_GRID */

#ifndef NDEBUG
/*
 * Unset intersect flag for all shapes that belong to body and do it recursively
 * for all its child bodies as well.
 */
static void
unset_intersect_flag(Body *b)
{
        Shape *s;
        DL_FOREACH(b->shapes, s) {
                s->flags &= ~SHAPE_INTERSECT;
        }
        
        Body *child;
        DL_FOREACH(b->children, child) {
                unset_intersect_flag(child);
        }
}
#endif  /* NDEBUG */

/*
 * Perform one world step.
 *
 * world        World that will be stepped.
 * L            Lua state pointer (NULL if not using Lua).
 */
void
world_step(World *world, lua_State *L)
{
        assert(world && !world->killme);
        assert(offsetof(Shape, body) == offsetof(Tile, body));
        if (world->paused)
                return;         /* Do nothing if world is paused. */
        
        /*
         * Put body/shape array pointers in global variables so smart_filter()
         * can access them.
         */
        extern Camera *cam_list;
        Camera *cam;
        Body  *active_bodies[ACTIVE_BODIES_MAX];
        Shape *active_shapes[ACTIVE_SHAPES_MAX];
        g_active_bodies = active_bodies;
        g_active_shapes = active_shapes;
        g_num_active_bodies = 0;
        g_num_active_shapes = 0;
#if !ALL_NOCTURNAL && ENABLE_TILE_GRID
        /*
         * Get shapes and bodies within camera vicinity.
         */
        DL_FOREACH(cam_list, cam) {
                if (cam->body.world != world)
                        continue;       /* Camera not inside this world. */
                
                vect_f cam_pos = body_pos(&cam->body);
                vect_f vicinity = {
                        cam->size.x * config.cam_vicinity_factor,
                        cam->size.y * config.cam_vicinity_factor
                };
                BB activity_bb = {
                        .l=cam_pos.x - cam->size.x/2 - vicinity.x,
                        .r=cam_pos.x + cam->size.x/2 + vicinity.x,
                        .b=cam_pos.y - cam->size.y/2 - vicinity.y,
                        .t=cam_pos.y + cam->size.y/2 + vicinity.y
                };
                grid_lookup(&world->grid, activity_bb, NULL, 0, smart_filter);
        }
                
        /*
         * Unset VISITED flag for both bodies and shapes.
         */
        unsigned num_bodies = g_num_active_bodies;
        for (unsigned i = 0; i < num_bodies; i++) {
                active_bodies[i]->flags &= ~BODY_VISITED;
        }
        unsigned num_shapes = g_num_active_shapes;
        for (unsigned i = 0; i < num_shapes; i++) {
                active_shapes[i]->flags &= ~SHAPE_VISITED;
        }
        
        /* Process nocturnal bodies. */
        for (Body *noc = world->nocturnal; noc != NULL;
             noc = noc->nocturnal_next) {
                if (!body_active(noc))
                        continue;       /* Ignore paused bodies. */
                
                /* Add body to `active_bodies`. */
                assert(num_bodies < ARRAYSZ(active_bodies));
                active_bodies[num_bodies++] = noc;
                
                /* Add shapes to `active_shapes`. */
                Shape *s;
                DL_FOREACH(noc->shapes, s) {
                        if (s->group->num_handlers > 0) {
                                assert(num_shapes < ARRAYSZ(active_shapes));
                                active_shapes[num_shapes++] = s;
                        }
                }
        }        
#else   /* !ALL_NOCTURNAL && ENABLE_TILE_GRID */
        /*
         * Since all bodies are nocturnal, add all shapes and bodies to
         * activity arrays.
         */
        dumb_add_all(&world->static_body);
        DL_FOREACH(cam_list, cam) {
                if (cam->body.world != world)
                        continue;       /* Camera not inside this world. */
                dumb_add_all(&cam->body);
        }
        unsigned num_bodies = g_num_active_bodies;
        unsigned num_shapes = g_num_active_shapes;
#endif  /* ALL_NOCTURNAL || !ENABLE_TILE_GRID */
        
        /* Remember state before continuing with the step. */
        save_state(world, active_bodies, num_bodies);
                
        /* Execute step functions and timers. */
        step_bodies(world, active_bodies, num_bodies, L, body_step);
        run_timers(world, active_bodies, num_bodies, L);
#ifndef NDEBUG
        /* Unset INTERSECT flag from prevous step. */
        unset_intersect_flag(&world->static_body);
#endif
        /* Now that body positions have possibly changed, resolve collisions. */
        resolve_collisions(world, active_shapes, num_shapes, L);
        
        /* Call after-step functions. */
        step_bodies(world, active_bodies, num_bodies, L, body_afterstep);
}
