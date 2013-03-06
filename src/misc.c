#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <SDL.h>
#include "common.h"
#include "camera.h"
#include "OpenGL_include.h"
#include "misc.h"

#if ENABLE_SQLITE
/*
 * Prepare SQL statement.
 *
 * Note: *stmt pointer must be NULL before entering this function.
 */
void
prep_stmt(sqlite3_stmt **stmt, const char *sql)
{
        assert(sql && stmt && *stmt == NULL);
        
        extern sqlite3 *db;
        sqlite3_prepare_v2(db, sql, -1, stmt, NULL);
        if (*stmt == NULL)
                fatal_error("[%s] %s", sql, sqlite3_errmsg(db));
}
#endif

/*
 * Convert object type (integer) to its name. Returned string is statically
 * allocated.
 */
const char *
object_name(void *obj)
{
        static char str[40];
        if (obj == NULL) {
                snprintf(str, sizeof(str), "(unknown: NULL)");
                return str;
        }
        
        switch (*(int *)obj) {
        case 0: return "(destroyed?)";
        case OBJTYPE_TILE: return "Tile";
        case OBJTYPE_BODY: return "Body";
        case OBJTYPE_SHAPE: return "Shape";
        case OBJTYPE_SPRITELIST: return "SpriteList";
        case OBJTYPE_CAMERA: return "Camera";
        case OBJTYPE_WORLD: return "World";
        case OBJTYPE_TIMER_C: return "Timer (C)";
        case OBJTYPE_TIMER_LUA: return "Timer (Lua)";
        case OBJTYPE_TIMERPTR: return "Timer pointer";
        }
        
        snprintf(str, sizeof(str), "(unknown: %i)", *(int *)obj);
        return str;
}

static unsigned seed = 1;

void
srand_eglibc(unsigned rand_seed)
{
        seed = rand_seed;
}

/* Random number generator from eglibc source. */
long
rand_eglibc(void)
{
        seed *= 1103515245;
        seed += 12345;
        int result = (unsigned int) (seed / 65536) % 2048;

        seed *= 1103515245;
        seed += 12345;
        result <<= 10;
        result ^= (unsigned int) (seed / 65536) % 1024;

        seed *= 1103515245;
        seed += 12345;
        result <<= 10;
        result ^= (unsigned int) (seed / 65536) % 1024;

        return result;
}

/*
 * Pick random value from interval [start, end]. Endpoints included. `end` may
 * be less than or equal to `start`.
 */
int
random_rangepick(int start, int end)
{
        if (start < end)
                return start + (int)(random() % (end - start + 1));
        return end + (int)(random() % (start - end + 1));
}

/*
 * Pick random value from interval [start, end]. Endpoints included. `end` may
 * be less than or equal to `start`. Values are spaced `step` units apart.
 */
float
random_rangepickf(float start, float end, float step)
{
        assert(step > 0.0);        
        if (start < end) {
                unsigned num_steps = (end - start) / step;
                return start + (random() % (num_steps + 1)) * step;
        }
        unsigned num_steps = (start - end) / step;
        return end + (random() % (num_steps + 1)) * step;
}

/*
 * Look for extension name in the the GL_EXTENSIONS string.
 */
int
check_extension(const char *name)
{
        assert(name != NULL && *name != '\0');
        char *estr = (char *)glGetString(GL_EXTENSIONS);
        if (estr == NULL) {
                log_err("glGetString(GL_EXTENSIONS) returned 0.");
                check_gl_errors();
                return 0;
        }
        size_t namelen = strlen(name);
        char *end = estr + strlen(estr);
        
        while (estr < end) {
                size_t wordlen = strcspn(estr, " "); /* Count nonspace chars. */
                if ((namelen == wordlen) && (strncmp(name, estr, wordlen) == 0))
                        return 1;
                estr += (wordlen + 1);
        }
        return 0;
}

unsigned
nearest_pow2(unsigned number)
{
        unsigned result = 2;
        while (result < number)
                result <<= 1;
        return result;
}

#if PLATFORM_IOS

/*
 * Add "@2x" to image filename.
 * Example:
 *      screw_it.jpg  ->  screw_it@2x.jpg
 *      GarBage       ->  GarBage
 *
 * Returns pointer to new name (which is the same as strbuf parameter).
 */
const char *
x2_add(const char *filename, char *strbuf, unsigned bufsize)
{
        UNUSED(bufsize);
        assert(filename && strbuf && filename != strbuf && bufsize > 0);
        assert(strlen(filename) + strlen("@2x") < bufsize);
        
        char *dot = strrchr(filename, '.');
        if (dot == NULL)
                return filename;

        memcpy(strbuf, filename, dot - filename);
        strcpy(stpcpy(strbuf + (dot - filename), "@2x"), dot);

        return strbuf;
}

/*
 * Perform the reverse of add_x2().
 * Example:
 *      suck_it@2x.jpg  ->  suck_it.jpg
 *
 * If @2x was not found, then filename is returned. Otherwise returns pointer
 * to strbuf. strbuf is filled with stripped filename regardless of whether
 * original filename contains @2x substring or not.
 */
const char *
x2_strip(const char *filename, char *strbuf, unsigned bufsize)
{
        UNUSED(bufsize);
        assert(filename && strbuf && filename != strbuf && bufsize > 0);
        assert(strlen(filename) < bufsize);
        
        char *x2 = strstr(filename, "@2x");
        if (x2 == NULL) {
                strcpy(strbuf, filename);
                return filename;
        }
        
        memcpy(strbuf, filename, x2 - filename);
        strcpy(strbuf + (x2 - filename), x2 + 3);
        
        return strbuf;
}

#endif  /* PLATFROM_IOS */

void check_gl_errors__(void)
{
        GLenum err = glGetError();
        if (err == GL_NO_ERROR)
                return;
        while (err != GL_NO_ERROR) {
                log_err("OpenGL: 0x%x", err);
                err = glGetError();
        }
        //abort();    
}
