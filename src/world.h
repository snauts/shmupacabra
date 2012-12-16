#ifndef GAME2D_WORLD_H
#define GAME2D_WORLD_H

#include "body.h"
#include "collision.h"
#include "common.h"
#include "shape.h"
#include "grid.h"

/*
 * World struct describes a physical world instance.
 */
typedef struct World_t {
        int      objtype;        /* = OBJTYPE_WORLD */
        char     name[20];       /* World name. */

        unsigned step_ms;        /* Duration of one step in milliseconds. */
        float    step_sec;       /* Duration of one step in seconds. */
        uint64_t next_step_time;
        int      paused;         /* Is world paused? */
        
        /* How many steps to skip when recording trace. */
        unsigned trace_skip;
              
        Body     static_body;    /* Topmost body that does not move. */
        Body     *nocturnal;     /* List of bodies that never sleep. */

        Grid     grid;           /* Spatial partitioning structure. */
                
        unsigned next_group_id;  /* Consecutive IDs for collision groups. */
        Group    *groups;        /* Collision group hash. */
        
        /* Map pairs of collision group IDs to their collision handler. */
        Handler handler_map[SHAPEGROUPS_MAX][SHAPEGROUPS_MAX];
        
        /* Ongoing collisions. */
        Collision *collisions;

        int     killme;         /* If true, world should be freed as soon
                                   as possible. */
} World;

World   *world_new(const char *name, unsigned step_ms, BB grid_area,
                   unsigned cell_size, unsigned trace_skip);
void     world_free(World *world);
void     world_kill(World *world);
void     world_step(World *world, lua_State *L);

#endif
