#ifndef GAME2D_COLLISION_H
#define GAME2D_COLLISION_H

#include "shape.h"
#include "uthash.h"

struct World_t;

enum {
        HANDLER_NONE = 0,
        HANDLER_C    = 1,
        HANDLER_LUA  = 2
};

/*
 * Collision handler struct.
 */
typedef struct {
        uint8_t         type;           /* Handler type (C or Lua). */
        intptr_t        func;           /* Collision handler function ID. */
        int             update;         /* If false, only begin/end events are given to user. */
        int             priority;       /* Handler priority determines order in
                                         which handlers are executed when
                                         shape intersects with more than one
                                         other shape simultaneously. */
        intptr_t        data;           /* Callback data. */
} Handler;

/*
 * Each shape belongs to exactly one collision group.
 * Collision handlers can be registered for each pair of groups.
 *
 * The hash is used to find existing groups by name.
 */
typedef struct Group_t {
        char    name[30];               /* Name of collision group. */
        uint    index;                  /* Index into world's `handler_map`. */
        uint    num_handlers;           /* Number of collision handlers for this group. */
        UT_hash_handle  hh;
} Group;

/*
 * When two colliding shapes are found, the below structure is filled and stored
 * in an array. Once all collisions for a particular shape are found and stored
 * in this way, they are sorted by priority. Then we iterate over the sorted
 * array and execute each handler.
 */
typedef struct {
        Shape           *shape_A, *shape_B;     /* Shapes involved in collision: Used as combined hash key. */
        Handler         handler;                /* Collision handler function. */
        Group           *group_A, *group_B;     /* Both involved collision groups. */
        
        int             active;                 /* True if shapes are colliding in current step. */
        int             ignore;                 /* True if user requested that no further callbacks be invoked. */
        
        UT_hash_handle  hh;
} Collision;

typedef int (*CollisionFunc)(Shape *A, Shape *B, int new_collision, BB *resolve, intptr_t data);

#endif  /* GAME2D_COLLISION_H */
