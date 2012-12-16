#include "common.h"

#if ENABLE_LUA

#include <assert.h>
#include <stdlib.h>
#include <lua.h>
#include <lualib.h>
#include "config.h"
#include "log.h"
#include "misc.h"
#include "util_lua.h"
#include "assert_lua.h"

static lua_State *cfg_L;        /* Lua state used by cfg_ functions. */
static int cfg_index;           /* Configuration table stack index. */

static int
error_handler(lua_State *L)
{
        RCCHECK(luaL_dostring(L, "io.stderr:write(debug.traceback(\"error_handler():\",3) .. '\\n')"), 0);
        lua_pushstring(L, lua_tostring(L, 1));
        return 1;
}

#ifndef NDEBUG

static int
panic_handler(lua_State *L)
{
        log_err("Failure while reading configuration data.");
        log_err("%s", lua_tostring(L, -1));
        return 1;
}

#endif  /* !NDEBUG */

static int
get_poolsize(lua_State *L, int index, const char *poolname)
{
        lua_getfield(L, index, poolname);
        if (!lua_isnumber(L, -1))
                log_warn("config.lua: missing `%s` pool size.", poolname);
        lua_Integer sz = lua_tointeger(L, -1);
        if (sz <= 0)
                fatal_error("config.lua: negative `%s` pool size.", poolname);
        lua_pop(L, 1);
        return (int)sz;
}

#define SET_POOLSIZE(poolname) \
        config.poolsize.poolname = get_poolsize(cfg_L, -1, #poolname);

int
cfg_read(const char *filename)
{
        /* Config routines use a separate Lua state. */
        cfg_L = luaL_newstate();
        luaL_openlibs(cfg_L);
        assert(cfg_L != NULL);
        
        /* Push error handler on stack. */
        lua_pushcfunction(cfg_L, error_handler);
        int cfg_errfunc_index = lua_gettop(cfg_L);
        
        /* Let Lua parse the configuration file. */
        if ((luaL_loadfile(cfg_L, filename) ||
             lua_pcall(cfg_L, 0, 0, cfg_errfunc_index)))
                fatal_error("[Lua] %s", lua_tostring(cfg_L, -1));
#ifndef NDEBUG
        /* Set panic function. */
        lua_atpanic(cfg_L, panic_handler);
#endif
        /* Leave "Cfg" table on the stack and remember its index. */
        lua_getglobal(cfg_L, "Cfg");            /* ... cfg */
        cfg_index = lua_gettop(cfg_L);
        
        /*
         * Cache some configuration.
         */
        config.FPSUpdateInterval = cfg_get_int("FPSUpdateInterval");
        config.fullscreen = cfg_get_bool("fullscreen");
        config.debug = cfg_get_bool("debug");
        config.pixel_scale = 1;
        config.use_desktop = cfg_get_bool("useDesktop");
        config.gameSpeed = cfg_get_int("gameSpeed");
        config.defaultShapeColor = cfg_get_color("defaultShapeColor");
        config.screen_width = cfg_get_int("screenWidth");
        config.screen_height = cfg_get_int("screenHeight");
        config.window_width = cfg_get_int("windowWidth");
        config.window_height = cfg_get_int("windowHeight");
        
        config.collision_dist = cfg_get_int("collision_dist");
        config.cam_vicinity_factor = cfg_get_float("cam_vicinity_factor");
        
        /* Read pool sizes. */
        lua_getfield(cfg_L, cfg_index, "poolsize");
        if (!lua_istable(cfg_L, -1))
                fatal_error("config.lua: missing 'poolsize' table.");
        SET_POOLSIZE(world);
        SET_POOLSIZE(body);
        SET_POOLSIZE(tile);
        SET_POOLSIZE(shape);
        SET_POOLSIZE(group);
        SET_POOLSIZE(camera);
        SET_POOLSIZE(texture);
        SET_POOLSIZE(spritelist);
        SET_POOLSIZE(timer);
        SET_POOLSIZE(gridcell);
        SET_POOLSIZE(property);
        SET_POOLSIZE(collision);
#if ENABLE_TOUCH
        SET_POOLSIZE(touch);
        SET_POOLSIZE(hackevent);
#endif
#if TRACE_MAX
        SET_POOLSIZE(bodytrace);
        SET_POOLSIZE(tiletrace);
        SET_POOLSIZE(shapetrace);
#endif
#if ENABLE_AUDIO
        SET_POOLSIZE(sound);
        SET_POOLSIZE(music);
#endif
        lua_pop(cfg_L, 1);  /* Pop `poolsize` table. */
        
        config.grid_info = 0;
        config.grid_expand = 0;
        config.grid_many = 10;
        
        return 1;
}

void
cfg_close()
{
        lua_close(cfg_L);
}

int
cfg_has_key(const char *key)
{
        assert(cfg_L != NULL && key != NULL);
        lua_getfield(cfg_L, cfg_index, key);
        int has = !lua_isnil(cfg_L, -1);
        lua_pop(cfg_L, 1);
        return has;
}

float
cfg_get_float(const char *key)
{
        assert(cfg_L && key);
        lua_pushstring(cfg_L, key);     /* ... key */
        lua_rawget(cfg_L, cfg_index);   /* ... value */
        float result = L_getstk_float(cfg_L, -1);
        lua_pop(cfg_L, 1);              /* ... */
        return result;
}

void
cfg_get_cstr(const char *key, char *buf, unsigned bufsize)
{       
        assert(cfg_L && key && buf && bufsize > 0);
        lua_pushstring(cfg_L, key);             /* + key */
        lua_rawget(cfg_L, cfg_index);           /* str? */
        info_assert(cfg_L, lua_isstring(cfg_L, -1), "String expected.");
        const char *result = lua_tostring(cfg_L, -1);
        RCLESS(snprintf(buf, bufsize, "%s", result), (int)bufsize);
        lua_pop(cfg_L, 1);                      /* ... */
}

uint32_t
cfg_get_color(const char *key)
{
        assert(cfg_L && key);
        lua_pushstring(cfg_L, key);             /* ... key */
        lua_rawget(cfg_L, cfg_index);           /* ... tbl? */
        uint32_t c = L_getstk_color(cfg_L, -1);
        lua_pop(cfg_L, 1);
        
        return c;
}

int
cfg_get_int(const char *key)
{
        assert(cfg_L && key);
        lua_pushstring(cfg_L, key);
        lua_rawget(cfg_L, cfg_index);
        info_assert(cfg_L, lua_isnumber(cfg_L, -1), "Integer expected");
        int result = (int)lua_tointeger(cfg_L, -1);
        lua_pop(cfg_L, 1);                      /* ... */
        return result;
}

int
cfg_get_bool(const char *key)
{
        assert(cfg_L && key);
        lua_pushstring(cfg_L, key);
        lua_rawget(cfg_L, cfg_index);
        info_assert_va(cfg_L, lua_isboolean(cfg_L, -1),
                       "Boolean value expected; key = `%s`.", key);
        int result = lua_toboolean(cfg_L, -1);
        lua_pop(cfg_L, 1);                      /* ... */
        return result;
}

#endif  /* ENABLE_LUA */
