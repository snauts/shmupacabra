#ifndef GAME2D_ASSERT_LUA_H
#define GAME2D_ASSERT_LUA_H

#include "common.h"
#include <stdarg.h>

#if ENABLE_LUA

#ifndef NDEBUG

#define info_assert_va(L, cond, fmt, ...)				    \
do {                                                                        \
        if (!(cond)) {                                                      \
                log_msg("Assertion failed in " SOURCE_LOC);                 \
                luaL_error(L, "Assertion `%s` failed: " fmt, #cond, __VA_ARGS__);\
        }                                                                   \
} while (0)

#define info_assert(L, cond, fmt)                                       \
do {                                                                    \
        if (!(cond)) {                                                  \
                log_msg("Assertion failed in " SOURCE_LOC);             \
                luaL_error(L, "Assertion `%s` failed: " fmt, #cond);    \
        }                                                               \
} while (0)

#else

#define info_assert(...)        ((void)0)
#define info_assert_va(...)     ((void)0)

#endif  /* !NDEBUG */

#endif  /* ENABLE_LUA */

#endif  /* GAME2D_ASSERT_LUA_H */
