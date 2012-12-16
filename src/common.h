#ifndef GAME2D_COMMON_H
#define GAME2D_COMMON_H

#include <math.h>
#include "game2d-cfg.h"

typedef unsigned int uint;

#include <stdint.h>

#if ENABLE_LUA
  #include <lua.h>
#else
  typedef int lua_State;
#endif

#if ENABLE_SQLITE
  #include <sqlite3.h>
#else
  typedef int sqlite3;
  typedef int sqlite3_stmt;
#endif

#if !ENABLE_AUDIO
  typedef int Sound;
#endif

#if !ENABLE_SDL2
  typedef int SDL_Window;
#endif

/*
 * snprintf() is not available on Windows.
 */
#ifdef _WIN32
  #define snprintf(str, n, fmt, args...) sprintf(str, fmt, ## args)
  #define srandom srand_eglibc
  #define random rand_eglibc
#endif /* Windows 32-bit. */

/* Maximum value, or "infinity", for 64-bit unsigned integer. */
#define UINT64_INF ((uint64_t)-1)

/* Source location. */
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define SOURCE_LOC __FILE__ ":" TOSTRING(__LINE__)

/* Position value rounding. */
#define posround(x) floorf((x) + 0.5)

/*
 * Game object type enumeration.
 */
enum ObjType {
        OBJTYPE_TILE         = 115870,
        OBJTYPE_BODY,
        OBJTYPE_SHAPE,
        OBJTYPE_SPRITELIST,
        OBJTYPE_CAMERA,
        OBJTYPE_WORLD,
        OBJTYPE_TIMER_LUA,
        OBJTYPE_TIMER_C,
        OBJTYPE_TIMERPTR
};

/* Suppress unused parameter warning in debug mode. */
#define UNUSED(x) do { (void)sizeof(x); } while (0)

/* Number of elements in an array. */
#define ARRAYSZ(a) (sizeof(a) / sizeof((a)[0]))

/* Check integer return value of function in debug mode. */
#ifndef NDEBUG
#define RCCHECK(fcall, value) assert((fcall) == (value))
#define RCLESS(fcall, value) assert((fcall) < (value))
#else
#define RCCHECK(fcall, value) (void)(fcall)
#define RCLESS(fcall, value) (void)(fcall)
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

/*
 * Degrees to radians and radians to degrees conversion.
 */
#define DEG_RAD(angle) ((float)(angle) * M_PI / 180.0)
#define RAD_DEG(angle) ((float)(angle) * 180.0 / M_PI)

#define TOGGLE(flag) (flag) = (flag) ? 0 : 1;
#define CLAMP(x, left, right) (((x) < (left)) ? (left) : ((x) > (right) ? (right) : (x)))
#define SETFLAG(flagstore, name, val) \
do { \
        if (val) \
                (flagstore) |= (name); \
        else \
                (flagstore) &= ~(name); \
} while (0)

#ifndef SWAP
#define SWAP(a, b) \
do { \
        assert(sizeof(a) == sizeof(b)); \
        uchar swap_temp[sizeof(a)]; \
        memcpy(swap_temp, &a, sizeof(a)); \
        memcpy(&a, &b, sizeof(a)); \
        memcpy(&b, swap_temp, sizeof(a)); \
} while (0)
#endif

#ifndef MIN
#define MIN(A,B) ({ __typeof__(A) _a = (A); __typeof__(B) _b = (B); _a < _b ? _a : _b; })
#endif

#ifndef MAX
#define MAX(A,B) ({ __typeof__(A) _a = (A); __typeof__(B) _b = (B); _a < _b ? _b : _a; })
#endif

#endif  /* GAME2D_COMMON_H */
