#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include "common.h"
#include "config.h"
#include "grid.h"
#include "log.h"
#include "mem.h"
#include "uthash_tuned.h"
#include "utlist.h"

/*
 * Initialize grid.
 */
void
grid_init(Grid *grid, BB area, int size)
{
        assert(grid != NULL && size > 0);
        assert(area.l < area.r && area.b < area.t);
        
        memset(grid, 0, sizeof(*grid));
        grid->size = size;
        grid->area = area;
                
        /* Calculate cell index ranges. */
        grid->cells = (BB){
                .l=(area.l < 0) ? -((-area.l-1)/size)-1 : area.l/size,
                .r=(area.r <= 0) ? -(-area.r/size)-1 : (area.r-1)/size,
                .b=(area.b < 0) ? -((-area.b-1)/size)-1 : area.b/size,
                .t=(area.t <= 0) ? -(-area.t/size)-1 : (area.t-1)/size
        };
        
        /* Number of columns and total number of cells. */
        grid->cols = grid->cells.r - grid->cells.l + 1;
        grid->num_cells = (grid->cells.t - grid->cells.b + 1) * grid->cols;
                
        grid->array = mem_alloc(sizeof(**grid->array) * grid->num_cells, "Grid cells");
        memset(grid->array, 0, sizeof(**grid->array) * grid->num_cells);
        
#ifndef NDEBUG
        /* Cell usage statistics. */
        grid->cellstat = mem_alloc(sizeof(*grid->cellstat) * grid->num_cells, "Grid stats");
        memset(grid->cellstat, 0, sizeof(*grid->cellstat) * grid->num_cells);
#endif
}

/*
 * Destroy grid.
 */
void
grid_destroy(Grid *grid)
{
#ifndef NDEBUG
        /* Make sure the grid is empty. */
        assert(grid->num_objects == 0);
        for (unsigned i = 0; i < grid->num_cells; i++) {
                assert(grid->array[i] == NULL);
                assert(grid->cellstat[i].current == 0);
        }
        
        mem_free(grid->cellstat);
#endif
        mem_free(grid->array);
        memset(grid, 0, sizeof(*grid));
}

#ifndef NDEBUG
/*
 * Report grid usage statistics.
 */
void
grid_info(Grid *grid, const char *name)
{
        log_msg("Grid usage information for `%s`:", name);
        log_msg("    Cell size: %i", grid->size);
        log_msg("    Number of cells: %i", grid->num_cells);
        log_msg("    Peak number of objects:    %i", grid->num_peak);
        log_msg("    Number of objects too big: %i", grid->num_toobig);
        
        /* Count cells with peak object count of more than `many`. */
        unsigned numcells_populous = 0;
        unsigned numcells_used = 0;
        for (unsigned i = 0; i < grid->num_cells; i++) {
                if (grid->cellstat[i].peak == 0)
                        continue;
                numcells_used++;
                if (grid->cellstat[i].peak > config.grid_many)
                        numcells_populous++;
        }
        log_msg("    Number of cells with more than %d objects:   %d",
                config.grid_many, numcells_populous);
        
        /* Give some suggestions. */
        if (grid->num_toobig * 10 >= grid->num_peak)
                log_msg("  Suggest increased cell size.");
        if (grid->num_expansions > 0) {
                log_msg("  Suggest changing grid area to: (BB){.l=%i,.r=%i,.b=%i,.t=%i}",
                        grid->area.l, grid->area.r, grid->area.b, grid->area.t);
        }
}

/*
 * Expand grid to include `objarea`.
 */
static void
grid_expand(Grid *grid, BB objarea)
{
        unsigned size = grid->size;
        BB area = grid->area;
        bb_union(&area, objarea);
        
        /* Count expansions. */
        grid->num_expansions++;
        
        /* Calculate new cell index ranges. */
        BB newcells = {
                .l=(area.l < 0) ? -((-area.l-1)/size)-1 : area.l/size,
                .r=(area.r <= 0) ? -(-area.r/size)-1 : (area.r-1)/size,
                .b=(area.b < 0) ? -((-area.b-1)/size)-1 : area.b/size,
                .t=(area.t <= 0) ? -(-area.t/size)-1 : (area.t-1)/size
        };
        
        /* Allocate new array of grid cells. */
        unsigned newcols = newcells.r - newcells.l + 1;
        unsigned new_numcells = (newcells.t - newcells.b + 1) * newcols;
        GridCell **newarray = mem_alloc(sizeof(**grid->array) * new_numcells, "Grid cells");
        memset(newarray, 0, sizeof(**grid->array) * new_numcells);
        
        /* Allocate new `cellstat` array. */
        void *new_cellstat = mem_alloc(sizeof(*grid->cellstat) * new_numcells, "Grid stats");
        memset(new_cellstat, 0, sizeof(*grid->cellstat) * new_numcells);
        
        /* Copy cell data from old arrays into new arrays. */
        BB oldcells = grid->cells;
        for (int y = oldcells.b; y <= oldcells.t; y++) {
                int row_index = (y - oldcells.b) * grid->cols;
                int index = (oldcells.l - newcells.l) + (y - newcells.b) * newcols;
                memcpy(&newarray[index], &grid->array[row_index], grid->cols * sizeof(GridCell *));
                memcpy((char *)new_cellstat + index * sizeof(*grid->cellstat),
                       &grid->cellstat[row_index], grid->cols * sizeof(*grid->cellstat));
        }
                        
        /* Free old arrays. */
        mem_free(grid->array);
        mem_free(grid->cellstat);
        
        /* Save new values. */
        grid->cells = newcells;
        grid->area = area;
        grid->cols = newcols;
        grid->num_cells = new_numcells;
        grid->array = newarray;
        grid->cellstat = new_cellstat;
}
#endif

/*
 * Add object to grid.
 *
 * grid         Grid.
 * object       Object that is to be inserted into the grid. Its memory should
 *              be all zeroes.
 * ptr          Pointer to user structure.
 * bb           Space that body occupies.
 */
void
grid_add(Grid *grid, GridObject *object, void *ptr, BB newarea)
{                
#ifndef NDEBUG
        assert(grid && grid->array && bb_valid(newarea));
        assert(object && !(object->flags & GRIDFLAG_STORED));
        
        /* Increase number of objects stored. */
        if (++(grid->num_objects) > grid->num_peak)
                grid->num_peak = grid->num_objects;

        /* Make sure object bounding box fits inside grid. */
        BB area = grid->area;
        if (newarea.l < area.l || newarea.r > area.r || newarea.b < area.b || newarea.t > area.t) {
                if (!config.grid_expand) {
                        log_err("Object (%p) with bounding box {l=%i,r=%i,b=%i,t=%i} "
                                "is outside partitioned space {l=%i,r=%i,b=%i,t=%i}. Did "
                                "something fall through the floor? Maybe grid area "
                                "argument of eapi.NewWorld() should be increased?", object,
                                newarea.l, newarea.r, newarea.b, newarea.t, area.l,
                                area.r, area.b, area.t);
                        bb_union(&area, newarea);
                        log_msg("Suggested grid area: (BB){.l=%i,.r=%i,.b=%i,.t=%i}",
                                area.l, area.r, area.b, area.t);
                        abort();
                }
                
                grid_expand(grid, newarea);
        }
#endif
        /* Calculate cell index ranges. */
        int size = grid->size;
        BB cells = grid->cells;
        BB objcells = {
                .l=(newarea.l < 0) ? -((-newarea.l-1)/size)-1 : newarea.l/size,
                .r=(newarea.r <= 0) ? -(-newarea.r/size)-1 : (newarea.r-1)/size,
                .b=(newarea.b < 0) ? -((-newarea.b-1)/size)-1 : newarea.b/size,
                .t=(newarea.t <= 0) ? -(-newarea.t/size)-1 : (newarea.t-1)/size
        };
        
#ifndef NDEBUG
        assert(objcells.r >= objcells.l && objcells.t >= objcells.b);
        assert(objcells.l >= cells.l && objcells.r <= cells.r);
        assert(objcells.b >= cells.b && objcells.t <= cells.t);

        unsigned obj_numcells = (objcells.r - objcells.l + 1) * (objcells.t - objcells.b + 1);
        if (obj_numcells > 9)
                grid->num_toobig++;
#endif
        /* Add to cell lists. */
        int cols = grid->cols;
        GridCell **array = grid->array;
        for (int y = objcells.b; y <= objcells.t; y++) {
                for (int x = objcells.l; x <= objcells.r; x++) {
                        int index = (x - cells.l) + (y - cells.b) * cols;
                        assert(index < (int)(grid->num_cells * sizeof(void *)));
                        
                        extern mem_pool mp_gridcell;
                        GridCell *cell = mp_alloc(&mp_gridcell);
                        cell->gridobj = object;
                        LL_PREPEND(array[index], cell);
#ifndef NDEBUG
                        unsigned current = ++(grid->cellstat[index].current);
                        if (current > grid->cellstat[index].peak)
                                grid->cellstat[index].peak = current;
#endif
                }
        }
        
        /* Fill in GridObject values. */
        object->ptr = ptr;
        object->area = newarea;
        object->flags |= GRIDFLAG_STORED;
}

/*
 * Remove object from grid.
 *
 * grid         Grid.
 * object       Object that is to be removed from the grid.
 */
void
grid_remove(Grid *grid, GridObject *object)
{
#ifndef NDEBUG
        assert(grid && grid->array && object && (object->flags & GRIDFLAG_STORED));
        
        /* Decrease number of objects stored. */
        assert(grid->num_objects > 0);
        grid->num_objects--;
#endif
        
        /* Calculate cell index ranges. */
        int size = grid->size;
        BB objarea = object->area;
        BB objcells = {
                .l=(objarea.l < 0) ? -((-objarea.l-1)/size)-1 : objarea.l/size,
                .r=(objarea.r <= 0) ? -(-objarea.r/size)-1 : (objarea.r-1)/size,
                .b=(objarea.b < 0) ? -((-objarea.b-1)/size)-1 : objarea.b/size,
                .t=(objarea.t <= 0) ? -(-objarea.t/size)-1 : (objarea.t-1)/size
        };
        
        /* Remove from cell lists. */
        int cols = grid->cols;
        BB cells = grid->cells;
        GridCell **array = grid->array;
        for (int y = objcells.b; y <= objcells.t; y++) {
                for (int x = objcells.l; x <= objcells.r; x++) {
                        int index = (x - cells.l) + (y - cells.b) * cols;
                        assert(index < (int)(grid->num_cells * sizeof(void *)));
                        GridCell *cell_list = array[index];
                        assert(cell_list != NULL);
#ifndef NDEBUG
                        assert(grid->cellstat[index].current > 0);
                        grid->cellstat[index].current--;
#endif
                        /* Handle case where first list element is the one we're looking for. */
                        extern mem_pool mp_gridcell;
                        if (cell_list->gridobj == object) {
                                array[index] = cell_list->next;         /* Remove from list. */
                                mp_free(&mp_gridcell, cell_list);       /* Free cell struct. */
                                continue;
                        }
                        
                        /* Find object within list. */
                        GridCell *cell = cell_list->next;
                        for (;;) {
                                if (cell->gridobj == object) {
                                        cell_list->next = cell->next;   /* Remove from list. */
                                        mp_free(&mp_gridcell, cell);    /* Free cell struct. */
                                        break;
                                }
                                cell_list = cell;
                                cell = cell->next;
                        }
                }
        }
        
        memset(object, 0, sizeof(*object));
}

void
grid_update(Grid *grid, GridObject *object, BB newarea)
{
        assert(grid && grid->array && object &&
               (object->flags & GRIDFLAG_STORED));
#ifndef NDEBUG
        /* Make sure object bounding box fits inside grid. */
        BB area = grid->area;
        if (newarea.l < area.l || newarea.r > area.r || newarea.b < area.b ||
            newarea.t > area.t) {
                if (!config.grid_expand) {
                        log_err("Object (%p) with bounding box "
                                "{l=%i,r=%i,b=%i,t=%i} is outside partitioned "
                                "space {l=%i,r=%i,b=%i,t=%i}. Did something "
                                "fall through the floor? Maybe grid area "
                                "argument of eapi.NewWorld() should be "
                                "increased?", object, newarea.l, newarea.r,
                                newarea.b, newarea.t, area.l, area.r, area.b,
                                area.t);
                        bb_union(&area, newarea);
                        log_msg("Suggested grid area: (BB){.l=%i,.r=%i,.b=%i,."
                                "t=%i}", area.l, area.r, area.b, area.t);
                        abort();
                }
                grid_expand(grid, newarea);
        }
#endif
        /* Calculate present cell index ranges. */
        int size = grid->size;
        BB oldarea = object->area;
        BB prev_objcells = {
                .l=(oldarea.l < 0) ? -((-oldarea.l-1)/size)-1 : oldarea.l/size,
                .r=(oldarea.r <= 0) ? -(-oldarea.r/size)-1 : (oldarea.r-1)/size,
                .b=(oldarea.b < 0) ? -((-oldarea.b-1)/size)-1 : oldarea.b/size,
                .t=(oldarea.t <= 0) ? -(-oldarea.t/size)-1 : (oldarea.t-1)/size
        };
        
        /* Calculate new cell index ranges. */
        BB new_objcells = {
                .l=(newarea.l < 0) ? -((-newarea.l-1)/size)-1 : newarea.l/size,
                .r=(newarea.r <= 0) ? -(-newarea.r/size)-1 : (newarea.r-1)/size,
                .b=(newarea.b < 0) ? -((-newarea.b-1)/size)-1 : newarea.b/size,
                .t=(newarea.t <= 0) ? -(-newarea.t/size)-1 : (newarea.t-1)/size
        };
        
        /* Set object bounding box. */
        object->area = newarea;
        
        /* Nothing to do if object belongs to the same cells. */
        if (bb_equal(prev_objcells, new_objcells))
                return;
        
        /* Remove from present cell lists. */
        int cols = grid->cols;
        BB cells = grid->cells;
        GridCell **array = grid->array;
        for (int y = prev_objcells.b; y <= prev_objcells.t; y++) {
                for (int x = prev_objcells.l; x <= prev_objcells.r; x++) {
                        int index = (x - cells.l) + (y - cells.b) * cols;
                        assert(index < (int)(grid->num_cells * sizeof(void *)));
                        GridCell *cell_list = array[index];
                        assert(cell_list != NULL);
#ifndef NDEBUG
                        assert(grid->cellstat[index].current > 0);
                        grid->cellstat[index].current--;
#endif
                        /*
                         * Handle case where first list element is the one we're
                         * looking for.
                         */
                        extern mem_pool mp_gridcell;
                        if (cell_list->gridobj == object) {
                                /* Remove from list and free cell struct. */
                                array[index] = cell_list->next;   
                                mp_free(&mp_gridcell, cell_list);
                                continue;
                        }
                        
                        /* Find object within list. */
                        GridCell *cell = cell_list->next;
                        for (;;) {
                                if (cell->gridobj == object) {
                                        /* Remove from list and free cell. */
                                        cell_list->next = cell->next;
                                        mp_free(&mp_gridcell, cell);
                                        break;
                                }
                                cell_list = cell;
                                cell = cell->next;
                        }
                }
        }
        
        /* Add to new cell lists. */
        for (int y = new_objcells.b; y <= new_objcells.t; y++) {
                for (int x = new_objcells.l; x <= new_objcells.r; x++) {
                        int index = (x - cells.l) + (y - cells.b) * cols;
                        assert(index < (int)(grid->num_cells * sizeof(void *)));
                        
                        extern mem_pool mp_gridcell;
                        GridCell *cell = mp_alloc(&mp_gridcell);
                        cell->gridobj = object;
                        LL_PREPEND(array[index], cell);
#ifndef NDEBUG
                        unsigned current = ++(grid->cellstat[index].current);
                        if (current > grid->cellstat[index].peak)
                                grid->cellstat[index].peak = current;
#endif
                }
        }
}

/*
 * Look up objects from the grid that fall into the provided bounding box.
 *
 * grid         Grid used for lookup.
 * bb           Get objects located in area specified by this bounding box.
 * result       Add found objects to this array.
 * max_results  Size of "result" array (i.e., how many objects can be inserted
 *              into it).
 * num_results  After lookup is done, the variable that this points to will be
 *              set to the actual number of objects that were found during
 *              lookup.
 * gf           User provided filtering routine.
 *
 * User filter C prototype:
 *      int filter(void *ptr);
 *
 *      Return false for objects that should not be added, true for those that
 *      should.
 */
unsigned
grid_lookup(Grid *grid, BB bb, void **result, unsigned max_results,
            GridFilter gf)
{
        UNUSED(max_results);
        assert(grid && bb_valid(bb));
        assert((!result && !max_results && gf) || (result && max_results));
                
        /* Calculate cell index ranges. */
        int size = grid->size;
        BB cells = grid->cells;
        BB lookcells = {
                .l=(bb.l < 0) ? -((-bb.l-1)/size)-1 : bb.l/size,
                .r=(bb.r <= 0) ? -(-bb.r/size)-1 : (bb.r-1)/size,
                .b=(bb.b < 0) ? -((-bb.b-1)/size)-1 : bb.b/size,
                .t=(bb.t <= 0) ? -(-bb.t/size)-1 : (bb.t-1)/size
        };
        assert(lookcells.r >= lookcells.l && lookcells.t >= lookcells.b);

        /*
         * Clamp visible cells to actual existing cells.
         * This is necessary in case user provided area falls outside of grid
         * area.
         */
        if (lookcells.l < cells.l)
                lookcells.l = cells.l;
        if (lookcells.r > cells.r)
                lookcells.r = cells.r;
        if (lookcells.b < cells.b)
                lookcells.b = cells.b;
        if (lookcells.t > cells.t)
                lookcells.t = cells.t;
                
        /*
         * Accumulate visited objects so we can reset their `visited` flag
         * later.
         */
        unsigned num_visited = 0;
        GridObject *visited[GRID_VISITED_MAX];
        
        unsigned num_results = 0;
        int cols = grid->cols;
        GridCell **array = grid->array;
        if (gf == NULL) {
                /*
                 * Add all objects that overlap the lookup area to result array
                 * (skip duplicates).
                 */
                for (int y = lookcells.b; y <= lookcells.t; y++) {
                        for (int x = lookcells.l; x <= lookcells.r; x++) {
                                int index = (x - cells.l) + (y - cells.b) * cols;
                                assert(index < (int)(grid->num_cells * sizeof(void *)));
                                GridCell *cell;
                                LL_FOREACH(array[index], cell) {
                                        GridObject *obj = cell->gridobj;
                                        if (!(obj->flags & GRIDFLAG_VISITED)) {
                                                /* Mark as visited and put into `visited` array. */
                                                obj->flags |= GRIDFLAG_VISITED;
                                                assert(num_visited < ARRAYSZ(visited));
                                                visited[num_visited++] = obj;

                                                /* Ignore if does not overlap lookup area. */
                                                if (obj->area.r < bb.l || obj->area.l > bb.r ||
                                                    obj->area.t < bb.b || obj->area.b > bb.t)
                                                        continue;
                                                
                                                /* Add as a result. */
                                                assert(num_results < max_results);
                                                result[num_results++] = obj->ptr;
                                        }
                                }
                        }
                }
        } else {
                /*
                 * Same as above, but let user function decide which objects are
                 * added.
                 */
                for (int y = lookcells.b; y <= lookcells.t; y++) {
                        for (int x = lookcells.l; x <= lookcells.r; x++) {
                                int index = (x - cells.l) + (y - cells.b) * cols;
                                assert(index < (int)(grid->num_cells * sizeof(void *)));
                                GridCell *cell;
                                LL_FOREACH(array[index], cell) {
                                        GridObject *obj = cell->gridobj;
                                        if (!(obj->flags & GRIDFLAG_VISITED)) {
                                                /* Mark as visited and put into `visited` array. */
                                                obj->flags |= GRIDFLAG_VISITED;
                                                assert(num_visited < ARRAYSZ(visited));
                                                visited[num_visited++] = obj;
                                                
                                                /* Ignore if does not overlap lookup area. */
                                                if (obj->area.r < bb.l || obj->area.l > bb.r ||
                                                    obj->area.t < bb.b || obj->area.b > bb.t)
                                                        continue;
                                                
                                                /* Add as a result if passes user filter. */
                                                if (gf(obj->ptr)) {
                                                        assert(num_results < max_results);
                                                        result[num_results++] = obj->ptr;
                                                }
                                        }
                                }
                        }
                }
        }
        
        /* Unset `visited` flag for visited objects. */
        for (unsigned i = 0; i < num_visited; i++)
                visited[i]->flags &= ~GRIDFLAG_VISITED;
                
        return num_results;
}

