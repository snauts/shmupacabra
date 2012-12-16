#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "common.h"
#include "log.h"
#include "mem.h"

#if MEMDEBUG

#include "uthash.h"
#include <SDL.h>

SDL_mutex *alloc_mutex;

typedef struct allocation_t {
        void *ptr;                      /* hash key */
        uint size;                      /* size of allocation */
        char descr[30];                 /* name of allocation. */
        UT_hash_handle hh;
} allocation;

static allocation *alloc_hash;
static uint total_bytes;

static const char *
human(uint size)
{
        static uint index;
        static char str[3][20];
        if (++index >= sizeof(str)/sizeof(str[0]))
                index = 0;
        
        if (size < 1024) {
                snprintf(str[index], sizeof(str[0]), "%d bytes", size);
                return str[index];
        }
        if (size < 1024 * 1024) {
                snprintf(str[index], sizeof(str[0]), "%d KB", size / 1024);
                return str[index];
        }
        if (size < 1024 * 1024 * 1024) {
                snprintf(str[index], sizeof(str[0]), "%d MB", size/1024/1024);
                return str[index];
        }
        snprintf(str[index], sizeof(str[0]), "WTF dude..");
        return str[index];
}
#endif

/*
 * Allocate memory.
 *
 * size         Number of bytes to allocate.
 * descr        Short description of what this memory will be used for.
 */
void *
mem_alloc(uint size, const char *descr)
{
        UNUSED(descr);
        assert(size > 0 && descr);
        void *ptr = malloc(size);
#if MEMDEBUG
        if (alloc_mutex == NULL)        /* XXX Race condition.. */
                alloc_mutex = SDL_CreateMutex();
        SDL_mutexP(alloc_mutex);
        {
                assert(ptr);
                allocation *alc = malloc(sizeof(allocation));
                alc->ptr = ptr;
                alc->size = size;
                snprintf(alc->descr, sizeof(alc->descr), "%s", descr);
                HASH_ADD_PTR(alloc_hash, ptr, alc);
                
                /* Update total number of allocated bytes. */
                total_bytes += size;
                log_msg("[MEM] Alloc %p=%s for `%s` (total: %s)", ptr,
                        human(size), descr, human(total_bytes));
        }
        SDL_mutexV(alloc_mutex);
#endif
        return ptr;
}

/*
 * Reallocate memory. 
 * 
 * ptr          Address of a pointer to the resizable memory.
 * size         Number of bytes to allocate.
 * descr        Short description of what this memory will be used for.
 */
void
mem_realloc(void **ptr, uint size, const char *descr)
{
        assert(ptr && descr);
        
        /* If *ptr is NULL, then do mem_alloc(). */
        if (*ptr == NULL) {
                *ptr = mem_alloc(size, descr);
                return;
        }
        
        *ptr = realloc(*ptr, size);
#if MEMDEBUG
        SDL_mutexP(alloc_mutex);
        {
                /* Find previous allocation. */
                assert(*ptr);
                allocation *alc;
                HASH_FIND_PTR(alloc_hash, ptr, alc);
                assert(alc != NULL);
                
                /* Update total number of allocated bytes. */
                total_bytes += (size - alc->size);
                log_msg("[MEM] Realloc %p=%s -> %p=%s for `%s` (total: %s)",
                        alc->ptr, human(alc->size), *ptr, human(size), descr,
                        human(total_bytes));
                
                /* Remove from hash and update allocation data. */
                HASH_DEL(alloc_hash, alc);
                alc->ptr = *ptr;
                alc->size = size;
                assert(strcmp(descr, alc->descr) == 0);
                
                /* Re-add to hash. */
                HASH_ADD_PTR(alloc_hash, ptr, alc);
        }
        SDL_mutexV(alloc_mutex);
#endif
}

/*
 * Free memory.
 */
void
mem_free(void *ptr)
{
#if MEMDEBUG
        SDL_mutexP(alloc_mutex);
        {
                assert(ptr);
                allocation *alc;
                HASH_FIND_PTR(alloc_hash, &ptr, alc);
                assert(alc != NULL);
                
                /* Update total numer of allocated bytes. */
                total_bytes -= alc->size;
                log_msg("[MEM] Free %p=%s for `%s` (total: %s)", ptr,
                        human(alc->size), alc->descr, human(total_bytes));
                
                /* Remove from hash and free allocation. */
                HASH_DEL(alloc_hash, alc);
                free(alc);
        }
        SDL_mutexV(alloc_mutex);
#endif
        free(ptr);
}

/*
 * Initialize a new memory pool.
 *
 * record_size  Size of one data record.
 * num_records  Number of estimated records.
 * name         Short description of what will be stored in this pool.
 */
void
mem_pool_init(mem_pool *mp, unsigned record_size, unsigned num_records,
              const char *name)
{
        assert(name && *name && strlen(name) < sizeof(mp->name));
        if (record_size == 0 || num_records == 0) {
                fatal_error("Invalid `%s` memory pool init data "
                            "(record_size:%d records:%d).", name, record_size,
                            num_records);
        }
                
        memset(mp, 0, sizeof(*mp));
        snprintf(mp->name, sizeof(mp->name), "%i %s", num_records, name);
        mp->cell_size = 2 * sizeof(void *) + record_size;
        mp->num_cells = num_records;
        
        /* Allocate and clear memory. */
        uint blocksize = mp->cell_size * mp->num_cells;
        mp->block = mem_alloc(blocksize, mp->name);
        memset(mp->block, 0, blocksize);
        
        /* Init lists. */
        mp->free_cells = mp->block;
        mp->inuse_cells = NULL;

        /*
         * Create a linked list of cells: the pointer in the current cell is set
         * to point to the previous and next cell.
         */
        char *ptr = NULL;
        for (uint i = 0; i < mp->num_cells; i++) {
                ptr = (char *)mp->block + mp->cell_size * i;
                *((void **)ptr+0) = ptr - mp->cell_size;        /* prev */
                *((void **)ptr+1) = ptr + mp->cell_size;        /* next */
        }
        *(void **)mp->block = NULL;         /* head->prev = NULL */
        *((void **)ptr+1) = NULL;           /* last->next = NULL */

        /* Set free_cells_last to point to last element in free cell list. */
        mp->free_cells_last = ptr;
}

/*
 * Free any resources associated with memory pool mp.
 */
void
mem_pool_free(mem_pool *mp)
{
#if MEMDEBUG
        assert(mp);

        /* Print statistics. */
        log_msg("[MEM] Destroy '%s' (%i, %i, %i, %i, %i)", mp->name,
                mp->num_cells, mp->stat_current, mp->stat_alloc, mp->stat_free,
                mp->stat_peak);
#endif
        /* Free all cells and the memory pool structure itself. */
        mem_free(mp->block);
        mem_free(mp);
}

/*
 * Allocate memory from pool mp.
 *
 * The memory contains all zeros. The only time this will not be true is if
 * something has messed with the memory after it's been mp_free()d.
 */
void *
mp_alloc(mem_pool *mp)
{
        assert(mp);
        if (mp->free_cells == NULL) {
                fatal_error("[MEM] Pool `%s` out of memory. Please increase "
                            "pool size in config.lua file.", mp->name);
        }
#if MEMDEBUG
        /* Adjust statistics. */
        mp->stat_current += 1;
        mp->stat_alloc += 1;
        if (mp->stat_current > mp->stat_peak)
                mp->stat_peak = mp->stat_current;
#endif
        /* Remove first cell from free cell list. */
        void **prev = ((void **)mp->free_cells+0);
        void **next = ((void **)mp->free_cells+1);
        assert(*prev == NULL);
        void *ptr = (void **)mp->free_cells+2;          /* Ptr to item data. */
        assert(*next != NULL || mp->free_cells == mp->free_cells_last);
        mp->free_cells = *next;                         /* head = head->next. */
        if (mp->free_cells != NULL)
                *((void **)mp->free_cells+0) = NULL;    /* head->prev = NULL */
        else
                mp->free_cells_last = NULL;             /* Last element gone. */

        /* Add cell to allocated cell list. */
        *next = mp->inuse_cells;                        /* item->next = head */
        if (mp->inuse_cells != NULL)
                *((void **)mp->inuse_cells+0) = prev;   /* head->prev = item */
        mp->inuse_cells = prev;                         /* head = ptr */

        return ptr;
}

/*
 * Free memory from a memory pool. The memory will be memset() to all zeros.
 */
void
mp_free(mem_pool *mp, void *ptr)
{
#if MEMDEBUG        
        /* Check if address belongs to the pool. */
        assert(mp && ptr);
        uint blocksize = mp->cell_size * mp->num_cells;
        if (ptr < mp->block || ptr >= mp->block + blocksize)
                fatal_error("[MEM] mp_free(): pointer %p does not belong to "
                            "'%s.'", ptr, mp->name);

        /* Adjust statistics. */
        mp->stat_current -= 1;
        mp->stat_free += 1;
#endif
        /* Clear memory. */
        memset(ptr, 0, mp->cell_size - 2 * sizeof(void *));

        /* Remove cell from allocated cell list. */
        void **prev = ((void **)ptr-2);
        void **next = ((void **)ptr-1);
        if (*prev == NULL)
                mp->inuse_cells = *next;        /* head = ptr->next */
        else
                *((void **)(*prev)+1) = *next;  /* ptr->prev->next = ptr->next*/
        if (*next != NULL)
                *((void **)(*next)+0) = *prev;  /* ptr->next->prev = ptr->prev*/
                
        /* Add ptr cell to the end of free cell list. */
        *next = NULL;                                   /* ptr->next = NULL */
        *prev = mp->free_cells_last;                    /* ptr->prev = last */
        if (mp->free_cells_last != NULL) {
                assert(*((void **)mp->free_cells_last+1) == NULL);
                *((void **)mp->free_cells_last+1) = prev; /* last->next = ptr */
        } else {
                assert(mp->free_cells == NULL);         /* No elements. */
                mp->free_cells = prev;                  /* head = ptr */
        }
        mp->free_cells_last = prev;                     /* last = ptr */
}

/*
 * Free all memory from a memory pool.
 */
void
mp_free_all(mem_pool *mp)
{
#if MEMDEBUG
        /* Print statistics. */
        assert(mp);
        log_msg("[MEM] Free all from '%s' (%i, %i, %i, %i, %i)", mp->name,
            mp->num_cells, mp->stat_current, mp->stat_alloc, mp->stat_free,
            mp->stat_peak);
#endif
        /* Free all cells one by one. */
        while (mp->inuse_cells != NULL) {
                mp_free(mp, (char *)mp->inuse_cells + 2 * sizeof(void *));
        }
#if MEMDEBUG
        assert(mp->stat_current == 0 && mp->stat_alloc == mp->stat_free);
#endif
}

/*
 * Return first pointer from the allocated cell list.
 */
void *
mp_first(mem_pool *mp)
{
        assert(mp);
        if (mp->inuse_cells == NULL)
                return NULL;
        return (char *)mp->inuse_cells + 2 * sizeof(void *);
}

/*
 * Iterate over allocated structures. A pointer that belongs to the list is
 * passed as an argument, next in the list is returned. If the end of the
 * allocated cell list is reached, NULL is returned.
 *
 * If memory pool has been modified during iteration (items have been either
 * allocated or freed), then the following rules apply:
 *   - do not ask for mp_next() on a freed pointer
 *   - 
 */
void *
mp_next(void *ptr)
{
        assert(ptr);
        void **next = (void **)ptr-1;
        if (*next == NULL)
                return NULL;
        return (void **)(*next)+2;
}

/*
 * Execute a routine for each allocated structure.
 */
void
mp_foreach(mem_pool *mp, mem_func func, void *cb_data)
{
        void **ptr = mp_first(mp);
        while (ptr) {
                func(ptr, cb_data);
                ptr = mp_next(ptr);
        }
}
