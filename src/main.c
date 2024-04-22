#include "common.h"

#include <SDL.h>
#include <emscripten.h>

#include <assert.h>
#include <stdlib.h>

#include "OpenGL_include.h"
#include "config.h"
#include "eapi_Lua.h"
#include "gameloop.h"
#include "init.h"
#include "log.h"
#include "texture.h"
#include "audio.h"
#include "misc.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

static          lua_State *L;
int             eapi_index;     /* "eapi" namespace table stack location. */
int             errfunc_index;  /* Error handler stack location. */
int             callfunc_index; /* eapi.__CallFunc. */
static          SDL_Window *win;

/*
 * This function is pushed onto Lua stack, and its index is passed into
 * lua_pcall() as the errfunc parameter. This means that whenever there's an
 * error, Lua will call this function.
 *
 * The point of this is so we get to print the call stack (traceback). Without
 * such an error handler, when lua_pcall() returns, the interesting part of the
 * stack has already been unwound and is no longer accessible.
 */
static int
error_handler(lua_State *L)
{
        RCCHECK(luaL_dostring(L, "io.stderr:write(debug.traceback(\"error_handler():\",3) .. '\\n')"), 0);
        lua_pushstring(L, lua_tostring(L, 1));
        return 1;
}

void main_loop() {
        bind_main_framebuffer();
        run_game(L);
        draw_main_framebuffer();
#if ENABLE_SDL2
        SDL_GL_SwapWindow(win);
#else
        SDL_GL_SwapBuffers();
#endif
}

EMSCRIPTEN_KEEPALIVE
void startup() {
        /* Execute user script. */
        if ((luaL_loadfile(L, "script/first.lua") ||
             lua_pcall(L, 0, 0, errfunc_index))) {
                fatal_error("[Lua] %s", lua_tostring(L, -1));
        }

        emscripten_set_main_loop(main_loop, 0, 1);
}

int main()
{
        /* Crack open the log file. */
        log_open(NULL);

        /* Print SDL version. */
        SDL_version sdl_version;
#if ENABLE_SDL2
        SDL_GetVersion(&sdl_version);
#else
        SDL_VERSION(&sdl_version);
#endif
        log_msg("SDL version: %u.%u.%u", sdl_version.major, sdl_version.minor,
                sdl_version.patch);

        /* Initialize necessary SDL subsystems. */
        uint32_t flags = SDL_INIT_TIMER | SDL_INIT_VIDEO;
#if ENABLE_AUDIO
        flags |= SDL_INIT_AUDIO;
#endif
#if ENABLE_JOYSTICK
        flags |= SDL_INIT_JOYSTICK;
#endif
        if (SDL_Init(flags) < 0)
                fatal_error("[SDL_Init] %s.", SDL_GetError());

        /* Start Lua. */
        L = luaL_newstate();
        luaL_openlibs(L);

        /* Push error handler on stack. */
        lua_pushcfunction(L, error_handler);
        errfunc_index = lua_gettop(L);

        /* Read configuration file. */
        cfg_read("config.lua");

        /* Allocate memory for pools & set atexit() which will free them. */
        setup_memory();
        atexit(cleanup);
#if ENABLE_AUDIO
        audio_init();
#endif
        /* Create window and set up OpenGL context. */
        win = create_window();

        /* Print OpenGL extension string. */
        log_msg("OpenGL extensions: %s", glGetString(GL_EXTENSIONS));

        /* Disable unnecessary OpenGL features. */
        glDisable(GL_DITHER);
        glDisable(GL_MULTISAMPLE);

        /* Classic blending. */
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        /* We expect to be using vertex and color arrays while drawing. */
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);

        /* No fancy alignment: we want our bytes packed tight. */
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        /* Texturing will be enabled as soon as a tile requires texturing. */
        glDisable(GL_TEXTURE_2D);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        texture_bind(NULL);

        /* Framebuffer clear color is opaque black. */
        glClearColor(0.0, 0.0, 0.0, 1.0);

        /* Set up main framebuffer. */
        init_main_framebuffer();

        /* Register "API" functions with Lua. */
        eapi_register(L);

        /*
         * Leave eapi.__CallFunc function on stack since we use it often.
         */
        lua_getfield(L, eapi_index, "__CallFunc");
        callfunc_index = lua_gettop(L);

        /*
         * Synchronize from IndexedDB to Emscripten MEMFS ("saavgaam" file location).
         * Then callback into C to start the game.
         */
        EM_ASM({
            FS.mkdir("/savedata");
            FS.mount(IDBFS, {}, "/savedata");
            FS.syncfs(true, function (err) {
              if (err) {
                console.error("Error mounting filesystem:", err);
              } else {
                console.log("Filesystem mounted and synchronized!");
                Module['_startup']();
              }
            });
        });
}
