#ifndef GAME2D_MEM_H
#define GAME2D_MEM_H

#include "common.h"

/*
 * The memory module defines memory pools; their use hopefully speeds things up
 * and saves some memory when lots of small object allocations happen. Some
 * other benefits include: ability to iterate over all allocated structures,
 * memory use statistics, ability to free everything that's in the pool in one
 * go.
 *
 * A pool, in this implementation, is a sequence of contiguous "cells" of memory
 * that form a linked list. All cells have the same size that is equal to the
 * size of the record they can hold plus space for two pointers: to next and
 * previous cell. Alloc()ed cells are removed from the beginning of the list;
 * free()d cells are returned to the end of the list (the list only contains
 * free cells). Both operations are O(1).
 *
 * The reason for not returning freed records to the beginning of free cell list
 * is so that the memory would not get reused immediately. Immediate reuse is
 * bad because some code may refer to this "freed" memory (which is no longer
 * freed but reused), thinking it's the same old object that it was expecting to
 * be there. But now the new object that has been placed there is affected, and
 * no errors or segfaults are generated. Thus immediately reusing memory can
 * obscure hard to track bugs.
 *
 * Memory freed by mp_free() is cleared to contain zero bytes only. Also memory
 * returned by mp_alloc() is all zeros as well.
 */

/* Change to 1 for statistics and debugging info to be printed. */
#define MEMDEBUG 0

/*
 * Memory pool structure.
 */
typedef struct {
        uint    cell_size;      /* Size of one cell. */
        uint    num_cells;      /* Total number of cells. */
        void    *block;         /* Memory owned by pool. */
        void    *free_cells;    /* Beginning of free cell list. */
        void    *free_cells_last; /* Last cell in free cell list. */
        void    *inuse_cells;   /* Beginning of allocated cell list. */
        char    name[32];       /* A short description of what's in the pool. */

#if MEMDEBUG
        /* Allocation statistics. */
        uint    stat_current;   /* Number of currently allocated cells. */
        uint    stat_alloc;     /* Number of mp_alloc() calls for this pool. */
        uint    stat_free;      /* Number of mp_free() calls for this pool. */
        uint    stat_peak;      /* Peak number of allocated cells. */
#endif
} mem_pool;

/* Standard allocation with some error checking. */
void    *mem_alloc(uint size, const char *descr);
void     mem_realloc(void **ptr, uint size, const char *descr);
void     mem_free(void *ptr);

/* Create and destroy memory pools. */
void     mem_pool_init(mem_pool *mp, uint record_size, uint num_records,
                       const char *name);
void     mem_pool_free(mem_pool *mp);

/* Pool allocation routines. */
void    *mp_alloc(mem_pool *mp);
void     mp_free(mem_pool *mp, void *ptr);
void     mp_free_all(mem_pool *mp);

/* Traverse allocated structures. */
void    *mp_first(mem_pool *mp);
void    *mp_next(void *ptr);
typedef void (*mem_func)(void *record, void *cb_data);
void     mp_foreach(mem_pool *mp, mem_func func, void *cb_data);

#endif
