#include <SDL.h>
#include <SDL_image.h>
#include "texture_async.h"
#include "config.h"
#include "mem.h"
#include "misc.h"
#include "utlist.h"

/* Maximum number of finished tasks. Make sure this is at least 2. */
#define TEXASYNC_FINISHED_MAX 2

typedef struct Task_t {
        int                             active;
        char                            filename[128];

        /* User may provide memory location to read image data from. */
        void                            *img_data;
        uint                            img_size;

        /*
         * The properties below, unlike others, may be changed at any time by
         * calling load() with a different set of arguments.
         */
        uintptr_t                       group;
        uint                            flags;
        TextureLoaded                   sync_cb;
        void                            *cb_data;

        /* Surface is loaded into OpeGL synchronously during `runsync`. */
        SDL_Surface                     *surf;
        
        struct Task_t                   *prev, *next;
        UT_hash_handle                  hh;
} Task;

/*
 * Two task queues: one for pending and ongoing tasks, and the other for
 * finished tasks.
 * Also a hash where we can look up tasks by their name (filename).
 */
static Task             *active_tasks;
static Task             *finished_tasks;
static uint             num_finished;
static Task             *task_hash;

static SDL_mutex        *storage_mutex; /* Safe access to task storage. */
static SDL_cond         *checktask_cond;  /* Signalled when a task is inserted into queue. */
static mem_pool         mp_tasks;       /* Memory pool for tasks. */

static void
load(const char *filename, uint flags, void *img_data, uint img_size, uintptr_t group, TextureLoaded sync_cb, void *cb_data)
{
        assert(sync_cb != NULL);
        SDL_mutexP(storage_mutex);
        {
                /* See if task for this filename already exists. */
                Task *task;
                HASH_FIND_STR(task_hash, filename, task);
                if (task == NULL) {
                        task = mp_alloc(&mp_tasks);
                        
                        /* Set task filename and add to hash. */
                        assert(*filename != '\0' && strlen(filename) < sizeof(task->filename));
                        strcpy(task->filename, filename);
                        HASH_ADD_STR(task_hash, filename, task);
                        
                        /* Set source buffer and size. */
                        task->img_data = img_data;
                        task->img_size = img_size;
                        
                        if (texture_is_loaded(filename, flags)) {
                                /* Already loaded: add to finished_tasks. */
                                DL_PREPEND(finished_tasks, task);
                                num_finished++;
                        } else {
                                /*
                                 * Insert into active list and signal task
                                 * processing thread.
                                 */
                                task->active = 1;
                                DL_PREPEND(active_tasks, task);
                                SDL_CondSignal(checktask_cond);
                        }
                } else if (task->active) {
                        /* Move to the front of active task queue. */
                        DL_DELETE(active_tasks, task);
                        DL_PREPEND(active_tasks, task);
                }
                
                /* Set/change group, flags, callback and its data pointer. */
                task->group = (group != 0) ? group : (uintptr_t)sync_cb;
                task->sync_cb = sync_cb;
                task->cb_data = cb_data;
                task->flags = flags;                
        }
        SDL_mutexV(storage_mutex);        
}

void
texasync_load(const char *filename, uint flags, uintptr_t group, TextureLoaded sync_cb, void *cb_data)
{
        load(filename, flags, NULL, 0, group, sync_cb, cb_data);
}

void
texasync_loadmem(const char *filename, uint flags, void *img_data, uint img_size,
                 uintptr_t group, TextureLoaded sync_cb, void *cb_data)
{
        assert(img_data && img_size > 0);
        load(filename, flags, img_data, img_size, group, sync_cb, cb_data);
}

/*
 * Release task memory.
 *
 * Before calling this function, task storage mutex must be held, and task must
 * already be removed from either active_tasks or finished_tasks lists.
 */
static void
free_task(Task *task)
{
        /* Free surface if it's been allocated. */
        if (task->surf != NULL)
                SDL_FreeSurface(task->surf);
        
        /* Free user buffer if it was given. */
        if (task->img_data != NULL)
                mem_free(task->img_data);
        
        /*
         * Remove from task hash and release task memory.
         */
        HASH_DEL(task_hash, task);
        mp_free(&mp_tasks, task);
}

/*
 * Clear all tasks. There may still remain one currently executing task.
 */
void
texasync_clear(void)
{
        SDL_mutexP(storage_mutex);
        {
                while (finished_tasks != NULL) {
                        Task *task = finished_tasks;
                        DL_DELETE(finished_tasks, task);
                        free_task(task);
                        
                        /* Update finished task counter. */
                        assert(num_finished > 0);
                        num_finished--;
                }
                
                while (active_tasks != NULL) {
                        Task *task = active_tasks;
                        DL_DELETE(active_tasks, task);
                        free_task(task);
                }
        }
        SDL_mutexV(storage_mutex);
}

/*
 * Go through the list of finished tasks and execute synchronous callbacks for
 * those tasks that belong to requested (argument) groups.
 *
 * All finished tasks are removed from list after they've been processed.
 *
 * Must be called from main thread.
 */
void
texasync_runsync(uintptr_t group, ...)
{
        /* Group array will be filled with argument groups. */
        assert(group != 0);
        uint num_groups = 1;
        uintptr_t group_array[10] = {group};
        
        /* Process variable argument list. */
        va_list ap;
        va_start(ap, group);
        uintptr_t iter_group;
        while ((iter_group = va_arg(ap, uintptr_t)) != 0) {
                assert(num_groups < sizeof(group_array)/sizeof(group_array[0]));
                group_array[num_groups++] = iter_group;
        }
        va_end(ap);
        
        SDL_mutexP(storage_mutex);
        {
                Task *task, *tmp;
                DL_FOREACH_SAFE(finished_tasks, task, tmp) {
                        assert(task->sync_cb && !task->active);
                        for (uint i = 0; i < num_groups; i++) {
                                if (task->group != group_array[i])
                                        continue;
                                                                
                                /*
                                 * Execute synchronous callback with mutex
                                 * released.
                                 *
                                 * Note: it is safe to access the task members
                                 *  below because they can only be changed from
                                 *  user thread -- same thread that is executing
                                 *  this very function right now.
                                 */
                                SDL_mutexV(storage_mutex);
                                {
                                        /* Preload texture from surface. */
                                        if (task->surf != NULL) {
                                                texture_preload_surface(task->filename, task->flags, task->surf);
                                                SDL_FreeSurface(task->surf);
                                                task->surf = NULL;
                                        }
                                        task->sync_cb(task->filename, task->flags, task->cb_data);
                                }
                                SDL_mutexP(storage_mutex);
                                
                                /* Free task. */
                                DL_DELETE(finished_tasks, task);
                                free_task(task);
                                
                                /* Update finished task counter. */
                                assert(num_finished > 0);
                                num_finished--;
                                break;
                        }                        
                }
                
                /*
                 * If number of finished tasks is below maximum value, signal
                 * async thread to check for new tasks to process.
                 */
                if (num_finished < TEXASYNC_FINISHED_MAX)
                        SDL_CondSignal(checktask_cond);
        }
        SDL_mutexV(storage_mutex);
}

static int
run_task(Task *task)
{
        assert(task->surf == NULL);
        assert(task->filename && *task->filename != '\0');
        
        /* See if user gave us a buffer to read image data from. */
        if (task->img_data != NULL) {
                task->surf = IMG_Load_RW(SDL_RWFromConstMem(task->img_data, task->img_size), 0);
                assert(task->surf != NULL);
                
                /* Free user buffer. */
                mem_free(task->img_data);
                task->img_data = NULL;
                return 1;
        }
#if ENABLE_SQLITE
        /* Prepare statement. */
        static sqlite3_stmt *stmt;
        if (stmt == NULL)
                prep_stmt(&stmt, "SELECT data FROM Image WHERE name=?1");
        
        /* Insert @2x into name if this is a high-res device. */
        const char *name = task->filename;
        char x2_name[128];
        if (config.screen_width > 500)
                name = x2_add(name, x2_name, sizeof(x2_name));
        
        /* Bind name argument and run the query. */
        log_msg("[TEXTURE-ASYNC] Load '%s' from DB.", name);
        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
        RCCHECK(sqlite3_step(stmt), SQLITE_ROW);
        
        /* Extract data. */
        int img_size = sqlite3_column_bytes(stmt, 0);
        const void *img_data = sqlite3_column_blob(stmt, 0);
        assert(img_size > 0 && img_data != NULL);
        
        /* Load image from database blob data and reset statement. */
        task->surf = IMG_Load_RW(SDL_RWFromConstMem(img_data, img_size), 0);
        assert(task->surf != NULL);        
        RCCHECK(sqlite3_reset(stmt), SQLITE_OK);
        
        return 1;
#else
        abort();
#endif
}

static int
texasync_thread(void *data)
{
        UNUSED(data);
        for (;;) {
                SDL_mutexP(storage_mutex);
                {
                /*
                 * If queue is empty, unlock the mutex and wait until we are
                 * signalled to check for a new task.
                 */
                if (active_tasks == NULL || num_finished >= TEXASYNC_FINISHED_MAX)
                        SDL_CondWait(checktask_cond, storage_mutex);
                
                /*
                 * Since loading textures is memory-intensive, we only keep a
                 * small amount of finished tasks around.
                 */
                if (active_tasks != NULL && num_finished < TEXASYNC_FINISHED_MAX) {
                        /*
                         * Pick first task from the queue (most recently
                         * added).
                         */
                        Task *task = active_tasks;
                        DL_DELETE(active_tasks, task);
                        assert(task && task->active);
                        
                        /* Unlock mutex to do blocking operation. */
                        SDL_mutexV(storage_mutex);
                        {
                                run_task(task);
                        }
                        SDL_mutexP(storage_mutex);
                        
                        /* Change task state. */
                        task->active = 0;
                        
                        /*
                         * Add to finished task list if synchronous callback is
                         * set. If not, destroy the task.
                         */
                        if (task->sync_cb != NULL) {
                                DL_APPEND(finished_tasks, task);
                                num_finished++;
                        }
                        else {
                                free_task(task);
                        }
                }
                }
                SDL_mutexV(storage_mutex);
        }
        abort();
}

void
texasync_thread_start(void)
{
        /* Init task memory pool. */
        mem_pool_init(&mp_tasks, sizeof(Task), 100, "Async Texture Tasks");
        
        /* Set up queue mutex and condition variable. */
        storage_mutex = SDL_CreateMutex();
        checktask_cond = SDL_CreateCond();
        
#if ENABLE_SDL2
        SDL_CreateThread(texasync_thread, "Async Texture Loader Thread", NULL);
#else
        SDL_CreateThread(texasync_thread, NULL);
#endif
}
