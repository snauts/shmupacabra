#include "common.h"

#if !PLATFORM_IOS

#include <SDL.h>

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>     /* Required for chdir(). */

#include "config.h"
#include "eapi_Lua.h"
#include "gameloop.h"
#include "init.h"
#include "log.h"
#include "OpenGL_include.h"
#include "texture.h"
#include "audio.h"
#include "misc.h"

#if defined(__APPLE__) && !defined(NO_BUNDLE)
#include "bundle_path.h"
#endif

void    bind_framebuffer(void);
void    draw_framebuffer(void);
void    cleanup_framebuffer(void);

#if ENABLE_LUA

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

int     eapi_index;     /* "eapi" namespace table stack location. */
int     errfunc_index;  /* Error handler stack location. */
int     callfunc_index; /* eapi.__CallFunc. */

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
#endif  /* ENABLE_LUA */

int
main(int argc, char *argv[])
{
        /* Open log and seed PRNG. */
        log_open(NULL);
        srandom(0);
        
#if ENABLE_LUA
        /* Start Lua. */
        lua_State *L = luaL_newstate();
        luaL_openlibs(L);
        
        /* Push error handler on stack. */
        lua_pushcfunction(L, error_handler);
        errfunc_index = lua_gettop(L);
#endif
        
#if !defined(__APPLE__) || defined(NO_BUNDLE)
        /* Find user application directory in command line options (-L). */
        for (int arg_i = 1; arg_i < argc; arg_i++) {
                if (strcmp(argv[arg_i], "-L") != 0)
                        continue;
                if (arg_i + 1 == argc)
                        break;
                assert(strlen(argv[arg_i + 1]) < sizeof(config.location));
                strcpy(config.location, argv[arg_i + 1]);
        }
        
        /* Change working dir to what was specified. */
        if (strlen(config.location) > 0 && chdir(config.location) != 0) {
                fatal_error("Could not change working directory to %s: %s",
                            config.location, strerror(errno));
        }
#else
        change_dir_to_bundle();
#endif
        
        /*
         * Read configuration file, then parse command line options because they
         * take precedence (they will overwrite values read from config).
         */
#if ENABLE_LUA
        cfg_read("config.lua");
#endif
#if ENABLE_SQLITE
        cfg_read("Config");
#endif
        parse_cmd_opt(argc, argv);
        
        /* Print game version. */
        cfg_get_cstr("version", config.version, sizeof(config.version));
        log_msg("Game version: %s", config.version);
        
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
        uint32_t flags = SDL_INIT_TIMER;
#if ENABLE_SDL_VIDEO
        flags |= SDL_INIT_VIDEO;
#endif
#if ENABLE_AUDIO
        flags |= SDL_INIT_AUDIO;
#endif
#if ENABLE_JOYSTICK
        flags |= SDL_INIT_JOYSTICK;
#endif
        if (SDL_Init(flags) < 0)
                fatal_error("[SDL_Init] %s.", SDL_GetError());
        
        /* Allocate memory for pools & set atexit() which will free them. */
        setup_memory();
        atexit(cleanup);
#if ENABLE_AUDIO
        audio_init();
#endif
        /* Create window and set up OpenGL context. */
        SDL_Window *win = create_window();
#if !ENABLE_SDL2
        UNUSED(win);
#endif
        /* Print OpenGL extension string. */
        if (cfg_get_bool("printExtensions"))
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
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glDisable(GL_TEXTURE_2D);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        texture_bind(NULL);
        
        /* Framebuffer clear color is opaque black. */
        glClearColor(0.0, 0.0, 0.0, 1.0);

        /* Set up main framebuffer. */
        init_main_framebuffer();
        
#if ENABLE_LUA
        /* Register "API" functions with Lua. */
	eapi_register(L);
	
	/*
         * Leave eapi.__CallFunc function on stack since we use it often.
         */
	lua_getfield(L, eapi_index, "__CallFunc");
	callfunc_index = lua_gettop(L);
        
        /* Execute user script. */
	if ((luaL_loadfile(L, "script/first.lua") ||
             lua_pcall(L, 0, 0, errfunc_index))) {
		fatal_error("[Lua] %s", lua_tostring(L, -1));
        }
#else
        // Run C script here.
        abort();
#endif
        for (;;) {
                bind_main_framebuffer();
                run_game(L);
                draw_main_framebuffer();
#if ENABLE_SDL2
                SDL_GL_SwapWindow(win);
#else
                SDL_GL_SwapBuffers();
#endif  /* ENABLE_SDL2 */
        }
        abort();
}

#endif  /* !PLATFORM_IOS */
