#ifndef GAME2D_GRID_H
#define GAME2D_GRID_H

#include "geometry.h"
#include "mem.h"
#include "uthash_tuned.h"

enum {
        GRIDFLAG_STORED         = 1<<0,
        GRIDFLAG_VISITED        = 1<<1
};

/*
 * This object can be inserted into the grid and is usually part of a user's
 * structure.
 */
typedef struct {
        void            *ptr;   /* Pointer to user data. */
        BB              area;   /* Area that object occupies. */
        int             flags;
} GridObject;

typedef struct GridCell_t {
        GridObject              *gridobj;
        struct GridCell_t       *next;
} GridCell;

typedef struct {
        uint            size;                   /* Size of each cell. */
        BB              cells;                  /* Cell index range. */
        BB              area;                   /* Grid area coords. */
        uint            cols, num_cells;        /* Number of columns, and total number of cells. */
        GridCell        **array;                /* Array of linked lists of cells. */
#ifndef NDEBUG
        uint            num_expansions;         /* Number of expansions. */
        uint            num_objects;            /* Number of objects added. */
        uint            num_peak;               /* Peak number of objects. */
        uint            num_toobig;             /* Number of object too big (occupy more than 9 cells). */
        struct {
                uint    current;
                uint    peak;
        } *cellstat;
#endif
} Grid;

typedef int (*GridFilter)(void *ptr);

void    grid_init(Grid *g, BB area, int size);
void    grid_destroy(Grid *g);
void    grid_info(Grid *g, const char *name);

#define grid_stored(object) ((object)->flags & GRIDFLAG_STORED)
void    grid_add(Grid *g, GridObject *object, void *ptr, BB bb);
void    grid_remove(Grid *g, GridObject *object);
void    grid_update(Grid *g, GridObject *object, BB bb);
uint    grid_lookup(Grid *g, BB bb, void **result, uint max_results, GridFilter gf);

#endif  /* GAME2D_GRID_H */
