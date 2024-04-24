#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <SDL.h>
#include "OpenGL_include.h"
#include "common.h"
#include "camera.h"
#include "misc.h"

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

void check_gl_errors__(void)
{
        GLenum err = glGetError();
        if (err == GL_NO_ERROR)
                return;
        while (err != GL_NO_ERROR) {
                log_err("OpenGL: 0x%x", err);
                err = glGetError();
        }
}
