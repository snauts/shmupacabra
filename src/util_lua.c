#include "common.h"

#if ENABLE_LUA

#include <assert.h>
#include <math.h>
#include <lua.h>
#include <SDL.h>
#include "log.h"
#include "util_lua.h"
#include "geometry.h"
#include "config.h"
#include "spritelist.h"
#include "utstring.h"
#include "assert_lua.h"

/*
 * Lua utility routines.
 */

/*
 * Print Lua stack (for debugging purposes).
 */
void
L_printstk(lua_State *L, const char *prefix)
{
        int n = lua_gettop(L);

        UT_string s, msg;
        utstring_init(&s);
        utstring_init(&msg);
        utstring_printf(&msg, "%s: ", prefix);
        for (int i = 1; i <= n; i++) {
                utstring_printf(&s, "%s(#=%d) ", lua_typename(L, lua_type(L, i)),
                                lua_objlen(L, i));
                utstring_concat(&msg, &s);
        }
        log_msg("%s", utstring_body(&msg));
        utstring_done(&s);
        utstring_done(&msg);
}

void
L_get_strfield(lua_State *L, int index, const char *name)
{
        if (index < 0)
                index--;
        lua_pushstring(L, name);
        lua_rawget(L, index);
}

void
L_get_intfield(lua_State *L, int index, int key)
{
        if (index < 0)
                index--;
        lua_pushinteger(L, key);
        lua_rawget(L, index);
}

const char *
L_arg_cstr(lua_State *L, int index)
{
        info_assert_va(L, lua_isstring(L, index),
                       "Argument %d: expected string, got `%s`.",
                       index, lua_typename(L, lua_type(L, index)));
        return lua_tostring(L, index);
}

void *
L_arg_userdata(lua_State *L, int index)
{
        info_assert_va(L, lua_islightuserdata(L, index),
                       "Argument %d: expected userdata, got `%s`.",
                       index, lua_typename(L, lua_type(L, index)));
        return lua_touserdata(L, index);
}

int
L_arg_int(lua_State *L, int index)
{
        info_assert_va(L, lua_isnumber(L, index),
                       "Argument %d: expected number, got `%s`.",
                       index, lua_typename(L, lua_type(L, index)));
        lua_Integer value = lua_tointeger(L, index);
        return (int)value;
}

unsigned
L_arg_uint(lua_State *L, int index)
{
        info_assert_va(L, lua_isnumber(L, index),
                       "Argument %d: expected number, got `%s`.",
                       index, lua_typename(L, lua_type(L, index)));
        lua_Integer value = lua_tointeger(L, index);
        info_assert_va(L, lua_isnumber(L, index) && value >= 0,
                       "Argument %d: expected unsigned integer, got %d.",
                       index, value);
        return (unsigned)value;
}

int
L_arg_bool(lua_State *L, int index)
{
        info_assert_va(L, lua_isboolean(L, index),
                       "Argument %d: expected boolean, got `%s`.",
                       index, lua_typename(L, lua_type(L, index)));
        return lua_toboolean(L, index);
}

float
L_arg_float(lua_State *L, int index)
{
        info_assert_va(L, lua_isnumber(L, index),
                       "Argument %d: expected number, got `%s`.",
                       index, lua_typename(L, lua_type(L, index)));
        float f = lua_tonumber(L, index);
        info_assert_va(L, isfinite(f), "Invalid number value: %.2f", f);
        return f;
}

/*
 * Get vect_f, as represented by Lua table {x, y}, or {x=?, y=?}, from the
 * stack.
 */
vect_f
L_arg_vectf(lua_State *L, int index)
{
        info_assert_va(L, lua_istable(L, index), "Argument %d: expected vector "
                       "table -- {x, y} or {x=?, y=?}, got `%s`.", index,
                       lua_typename(L, lua_type(L, index)));
        lua_pushnumber(L, 1);
        lua_rawget(L, index);
        if (lua_isnumber(L, -1)) {
                lua_pushnumber(L, 2);
                lua_rawget(L, index);
        } else {
                lua_pop(L, 1);
                lua_pushstring(L, "x");
                lua_rawget(L, index);
                lua_pushstring(L, "y");
                lua_rawget(L, index);
        }
        info_assert_va(L, lua_isnumber(L, -1) && lua_isnumber(L, -2),
                       "Argument %d: doesn't look like a vector {%s, %s}.",
                       index, lua_typename(L, lua_type(L, -2)), 
                       lua_typename(L, lua_type(L, -1)));
        vect_f v = {
                lua_tonumber(L, -2),
                lua_tonumber(L, -1)
        };
        info_assert_va(L, isfinite(v.x) && isfinite(v.y), "Argument %d: "
                       "Invalid floating point value {x=%f, y=%f}.", index, v.x,
                       v.y);
        lua_pop(L, 2);
        return v;
}

/*
 * Get vect_i, as represented by Lua table {x, y}, or {x=?, y=?}, from the
 * stack.
 */
vect_i
L_arg_vecti(lua_State *L, int index)
{
        info_assert_va(L, lua_istable(L, index),
                       "Argument %d: expected vector table -- {x, y} or "
                       "{x=?, y=?}, got `%s`.", index,
                       lua_typename(L, lua_type(L, index)));
        lua_pushnumber(L, 1);
        lua_rawget(L, index);
        if (lua_isnumber(L, -1)) {
                lua_pushnumber(L, 2);
                lua_rawget(L, index);
        } else {
                lua_pop(L, 1);
                lua_pushstring(L, "x");
                lua_rawget(L, index);
                lua_pushstring(L, "y");
                lua_rawget(L, index);
        }
        info_assert(L, !lua_isnil(L, -1) && !lua_isnil(L, -2),
                    "Doesn't look like a vector.");
        vect_i v = {
                (int)lua_tointeger(L, -2),
                (int)lua_tointeger(L, -1)
        };
        info_assert_va(L, lua_tonumber(L, -2) == (float)v.x &&
                       lua_tonumber(L, -1) == (float)v.y,
                       "Integer vector was expected, got this: {x=%f, y=%f}.",
                       lua_tonumber(L, -2), lua_tonumber(L, -1));
        lua_pop(L, 2);
        return v;
}

uint32_t
L_arg_color(lua_State *L, int i)
{
        info_assert_va(L, lua_istable(L, i), "Argument %d: expected color "
                       "table -- {r=?, g=?, b=?, a=?} -- color components "
                       "default to 1.0, got `%s`.", i,
                       lua_typename(L, lua_type(L, i)));
        lua_pushstring(L, "r");
        lua_rawget(L, i);
        info_assert_va(L, lua_isnil(L, -1) || lua_isnumber(L, -1),
                       "Argument %d: invalid RED value.", i);
        lua_pushstring(L, "g");
        lua_rawget(L, i);
        info_assert_va(L, lua_isnil(L, -1) || lua_isnumber(L, -1),
                       "Argument %d: invalid GREEN value.", i);
        lua_pushstring(L, "b");
        lua_rawget(L, i);
        info_assert_va(L, lua_isnil(L, -1) || lua_isnumber(L, -1),
                       "Argument %d: invalid BLUE value.", i);
        lua_pushstring(L, "a");
        lua_rawget(L, i);
        info_assert_va(L, lua_isnil(L, -1) || lua_isnumber(L, -1),
                       "Argument %d: invalid ALPHA value.", i);
        
        float r = lua_isnil(L, -4) ? 1.0 : lua_tonumber(L, -4);
        float g = lua_isnil(L, -3) ? 1.0 : lua_tonumber(L, -3);
        float b = lua_isnil(L, -2) ? 1.0 : lua_tonumber(L, -2);
        float a = lua_isnil(L, -1) ? 1.0 : lua_tonumber(L, -1);
        info_assert_va(L,
                       isfinite(r) && isfinite(g) && isfinite(b) && isfinite(a),
                       "Argument %d: invalid floating point color component "
                       "value {r=%f, g=%f, b=%f, a=%f}.", i, r, g, b, a);
        uint32_t c = color_32bit(r, g, b, a);
        lua_pop(L, 4);
        return c;
}

vect_i *
L_argptr_vecti(lua_State *L, int index, vect_i *store)
{
        if (lua_isnoneornil(L, index))
                return NULL;
        *store = L_arg_vecti(L, index);
        return store;
}

vect_f *
L_argptr_vectf(lua_State *L, int index, vect_f *store)
{
        if (lua_isnoneornil(L, index))
                return NULL;
        *store = L_arg_vectf(L, index);
        return store;
}

BB
L_arg_BB(lua_State *LS, int index)
{
        info_assert(LS, lua_istable(LS, index), "Expected bounding box in the "
                    "form {l=?, r=?, b=?, t=?}.");
        lua_pushstring(LS, "l");
        lua_rawget(LS, index);
        lua_pushstring(LS, "r");
        lua_rawget(LS, index);
        lua_pushstring(LS, "b");
        lua_rawget(LS, index);
        lua_pushstring(LS, "t");
        lua_rawget(LS, index);
        info_assert(LS, lua_isnumber(LS, -1) && lua_isnumber(LS, -2) &&
                    lua_isnumber(LS, -3) && lua_isnumber(LS, -4),
                    "Expected bounding box in the form {l=?, r=?, b=?, t=?}.");
        
        /* Get numbers. */
        lua_Number l = lua_tonumber(LS, -4);
        lua_Number r = lua_tonumber(LS, -3);
        lua_Number b = lua_tonumber(LS, -2);
        lua_Number t = lua_tonumber(LS, -1);
        info_assert_va(LS, isfinite(l) && isfinite(r) && isfinite(b) &&
                    isfinite(t), "Invalid value: {l=%.2f, r=%.2f, b=%.2f, "
                    "t=%.2f}", l, r, b, t);
        lua_pop(LS, 4);
        return (BB){l, r, b, t};
}

BB *
L_argptr_BB(lua_State *L, int index, BB *store)
{
        if (lua_isnoneornil(L, index))
                return NULL;
        *store = L_arg_BB(L, index);
        return store;
}

TexFrag
L_arg_TexFrag(lua_State *LS, int index)
{
        info_assert(LS, lua_istable(LS, index),
                    "Expected texture fragment {l=?, r=?, b=?, t=?}.");
        lua_pushstring(LS, "l");
        lua_rawget(LS, index);
        lua_pushstring(LS, "r");
        lua_rawget(LS, index);
        lua_pushstring(LS, "b");
        lua_rawget(LS, index);
        lua_pushstring(LS, "t");
        lua_rawget(LS, index);
        info_assert(LS, lua_isnumber(LS, -1) && lua_isnumber(LS, -2) &&
                    lua_isnumber(LS, -3) && lua_isnumber(LS, -4),
                    "Expected texture fragment {l=?, r=?, b=?, t=?}.");
        /* Get numbers. */
        lua_Number l = lua_tonumber(LS, -4);
        lua_Number r = lua_tonumber(LS, -3);
        lua_Number b = lua_tonumber(LS, -2);
        lua_Number t = lua_tonumber(LS, -1);
        info_assert_va(LS, isfinite(l) && isfinite(r) && isfinite(b) &&
                       isfinite(t), "Invalid value: {l=%.2f, r=%.2f, b=%.2f, "
                       "t=%.2f}", l, r, b, t);
        lua_pop(LS, 4);
        return (TexFrag){l, r, b, t};
}

/*
 * Read a table in the form {first, second} from stack and put the boolean
 * values into [first] and [second].
 */
void
L_getstk_boolpair(lua_State *L, int index, int *first, int *second)
{
        if (index < 0)
                index += lua_gettop(L) + 1;
        
        info_assert(L, lua_istable(L, index),
                    "Table (pair of boolean values) expected.");
        lua_pushnumber(L, 1);           /* ... pair ... 1 */
        lua_gettable(L, index);         /* ... pair ... first */
        lua_pushnumber(L, 2);           /* ... pair ... first 2 */
        lua_gettable(L, index);         /* ... pair ... first second */
        info_assert(L, lua_isboolean(L, -2) && lua_isboolean(L, -1),
                    "Expected two boolean values.");
        *first = lua_toboolean(L, -2);
        *second = lua_toboolean(L, -1);
        lua_pop(L, 2);                  /* ... pair ... */
}

void
L_getstk_intpair(lua_State *L, int index, int *first, int *second)
{
        if (index < 0)
                index += lua_gettop(L) + 1;
        
        info_assert(L, lua_istable(L, index),
                    "Table (pair of integer values) expected.");
        lua_pushnumber(L, 1);           /* ... pair ... 1 */
        lua_gettable(L, index);         /* ... pair ... first */
        lua_pushnumber(L, 2);           /* ... pair ... first 2 */
        lua_gettable(L, index);         /* ... pair ... first second */
        info_assert(L, lua_isnumber(L, -2) && lua_isnumber(L, -1),
                    "Expected two integer values.");
        *first = (int)lua_tointeger(L, -2);
        *second = (int)lua_tointeger(L, -1);
        lua_pop(L, 2);                  /* ... pair ... */
}

float
L_getstk_float(lua_State *L, int index)
{
        if (index < 0)
                index += lua_gettop(L) + 1;
        return L_arg_float(L, index);
}

uint32_t
L_getstk_color(lua_State *L, int index)
{
        if (index < 0)
                index += lua_gettop(L) + 1;
        return L_arg_color(L, index);
}

vect_i
L_getstk_vecti(lua_State *L, int index)
{
        if (index < 0)
                index += lua_gettop(L) + 1;
        return L_arg_vecti(L, index);
}

void
L_push_vectf(lua_State *L, vect_f v)
{
        lua_createtable(L, 0, 2);       /* ... {} */
        
        lua_pushstring(L, "x");
        lua_pushnumber(L, v.x);
        lua_rawset(L, -3);
        lua_pushstring(L, "y");
        lua_pushnumber(L, v.y);
        lua_rawset(L, -3);
}

void
L_push_vecti(lua_State *L, vect_i v)
{
        lua_createtable(L, 0, 2);       /* ... {} */
        
        lua_pushstring(L, "x");
        lua_pushnumber(L, v.x);
        lua_rawset(L, -3);
        lua_pushstring(L, "y");
        lua_pushnumber(L, v.y);
        lua_rawset(L, -3);
}

void
L_push_BB(lua_State *L, BB bb)
{
        lua_createtable(L, 0, 4);
        
        lua_pushstring(L, "l");
        lua_pushnumber(L, bb.l);
        lua_rawset(L, -3);
        
        lua_pushstring(L, "r");
        lua_pushnumber(L, bb.r);
        lua_rawset(L, -3);

        lua_pushstring(L, "b");
        lua_pushnumber(L, bb.b);
        lua_rawset(L, -3);

        lua_pushstring(L, "t");
        lua_pushnumber(L, bb.t);
        lua_rawset(L, -3);
}

void
L_push_color(lua_State *L, uint32_t color)
{
        lua_createtable(L, 0, 4);
        
        lua_pushstring(L, "r");
        lua_pushnumber(L, COLOR_RED(color));
        lua_rawset(L, -3);
        
        lua_pushstring(L, "g");
        lua_pushnumber(L, COLOR_GREEN(color));
        lua_rawset(L, -3);

        lua_pushstring(L, "b");
        lua_pushnumber(L, COLOR_BLUE(color));
        lua_rawset(L, -3);
        
        lua_pushstring(L, "a");
        lua_pushnumber(L, COLOR_ALPHA(color));
        lua_rawset(L, -3);
}

/* Push {boolean, boolean} */
void
L_push_boolpair(lua_State *L, int first, int second)
{
        lua_createtable(L, 2, 0);
        lua_pushnumber(L, 1);
        lua_pushboolean(L, first);
        lua_settable(L, -3);
        lua_pushnumber(L, 2);
        lua_pushboolean(L, second);
        lua_settable(L, -3);
}

#endif

