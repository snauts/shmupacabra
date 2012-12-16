#ifndef GAME2D_UTIL_LUA_H
#define GAME2D_UTIL_LUA_H

#include "common.h"

#if ENABLE_LUA

#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include "geometry.h"
#include "misc.h"
#include "shape.h"
#include "spritelist.h"
#include "world.h"

#ifndef NDEBUG

#define L_numarg_range(L, atleast, atmost)                                     \
        if (lua_gettop((L)) < (atleast) || lua_gettop((L)) > (atmost)) {       \
                log_msg("Assertion failed in " SOURCE_LOC);                    \
                if (atleast == atmost) {                                       \
                        luaL_error(L, "Received %d arguments, expected %d.",   \
                                   lua_gettop(L), atleast);                    \
                } else {                                                       \
                        luaL_error(L, "Received %d arguments, excpected at "   \
                                   "least %d and at most %d.", lua_gettop(L),  \
                                   atleast, atmost);                           \
                }                                                              \
        }
#else

#define L_numarg_range(...)	((void)0)

#endif /* !NDEBUG */

/* Misc. */
void		 L_printstk(lua_State *L, const char *prefix);
const char	*L_statstr(int status_code);

/* Get table string/integer field without metamethods. */
void             L_get_strfield(lua_State *L, int index, const char *keyname);
void             L_get_intfield(lua_State *L, int index, int key);

const char      *L_arg_cstr(lua_State *L, int index);
void            *L_arg_userdata(lua_State *L, int index);
int              L_arg_int(lua_State *L, int index);
uint             L_arg_uint(lua_State *L, int index);
int              L_arg_bool(lua_State *L, int index);
float            L_arg_float(lua_State *L, int index);
BB               L_arg_BB(lua_State *L, int index);
TexFrag          L_arg_TexFrag(lua_State *L, int index);
vect_f           L_arg_vectf(lua_State *L, int index);
vect_i           L_arg_vecti(lua_State *L, int index);
uint32_t         L_arg_color(lua_State *L, int index);

#define          L_argdef_userdata(L, index, def) (lua_isnoneornil((L), (index)) ? def : L_arg_userdata((L), (index)))
#define          L_argdef_cstr(L, index, def)     (lua_isnoneornil((L), (index)) ? def : L_arg_cstr((L), (index)))
#define          L_argdef_uint(L, index, def)     (lua_isnoneornil((L), (index)) ? def : L_arg_uint((L), (index)))
#define          L_argdef_bool(L, index, def)     (lua_isnoneornil((L), (index)) ? def : L_arg_bool((L), (index)))
#define          L_argdef_int(L, index, def)      (lua_isnoneornil((L), (index)) ? def : L_arg_int((L), (index)))
#define          L_argdef_float(L, index, def)    (lua_isnoneornil((L), (index)) ? def : L_arg_float((L), (index)))
static inline vect_i
L_argdef_vecti(lua_State *L, int index, vect_i def)
{
        return lua_isnoneornil(L, index) ? def : L_arg_vecti(L, index);
}
static inline vect_f
L_argdef_vectf(lua_State *L, int index, vect_f def)
{
        return lua_isnoneornil(L, index) ? def : L_arg_vectf(L, index);
}

vect_i          *L_argptr_vecti(lua_State *L, int index, vect_i *store);
vect_f          *L_argptr_vectf(lua_State *L, int index, vect_f *store);
BB              *L_argptr_BB(lua_State *L, int index, BB *store);

/* Push values onto stack. */
void             L_push_vecti(lua_State *L, vect_i v);
void             L_push_vectf(lua_State *L, vect_f v);
void             L_push_BB(lua_State *L, BB bb);
void             L_push_boolpair(lua_State *L, int first, int second);
void             L_push_color(lua_State *L, uint32_t color);

/* Get complex values from stack. */
uint32_t         L_getstk_color(lua_State *L, int index);
vect_i           L_getstk_vecti(lua_State *L, int index);
float            L_getstk_float(lua_State *L, int index);
void             L_getstk_boolpair(lua_State *L, int index, int *first, int *second);
void             L_getstk_intpair(lua_State *L, int index, int *first, int *second);

#endif  /* ENABLE_LUA */

#endif /* GAME2D_UTIL_LUA_H */
