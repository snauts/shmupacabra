#include "common.h"
#include <assert.h>
#include <SDL.h>
#include "config.h"
#include "mem.h"
#include "misc.h"
#include "eapi_C.h"
#include "init.h"
#include "texture.h"
#include "OpenGL_include.h"
#include "audio.h"

/* Memory pools. */
mem_pool mp_body, mp_camera, mp_group, mp_shape;
mem_pool mp_sprite, mp_texture, mp_tile, mp_timer, mp_gridcell;
mem_pool mp_world, mp_property, mp_collision;
#if TRACE_MAX
mem_pool mp_bodytrace, mp_tiletrace, mp_shapetrace;
#endif

#if ENABLE_TOUCH
mem_pool mp_touch;

#if !ENABLE_SDL_VIDEO
mem_pool mp_hackevent;
#endif
#endif  /* ENABLE_TOUCH */

#if ENABLE_SQLITE
/* Global database handle. */
sqlite3 *db;
#endif

#if ENABLE_AUDIO
mem_pool mp_sound;
#endif

/*
 * Perform cleanup.
 */
void
cleanup(void)
{
#if ENABLE_AUDIO
        audio_close();  /* Close audio if it was opened. */
#endif
        SDL_Quit();     /* Finally, kill SDL. */
}

void
setup_memory(void)
{
        struct poolsize_t *ps = &config.poolsize;
        mem_pool_init(&mp_body, sizeof(Body), ps->body, "Body");
        mem_pool_init(&mp_camera, sizeof(Camera), ps->camera, "Camera");
        mem_pool_init(&mp_group, sizeof(Group), ps->group,
                      "Shape collision group");
        mem_pool_init(&mp_shape, sizeof(Shape), ps->shape, "Shape");
        mem_pool_init(&mp_sprite, sizeof(SpriteList), ps->spritelist,
                      "SpriteList");
        mem_pool_init(&mp_texture, sizeof(Texture), ps->texture, "Texture");
        mem_pool_init(&mp_tile, sizeof(Tile), ps->tile, "Tile");
        mem_pool_init(&mp_timer, sizeof(Timer), ps->timer, "Timer");
        mem_pool_init(&mp_gridcell, sizeof(GridCell), ps->gridcell, "GridCell");
        mem_pool_init(&mp_world, sizeof(World), ps->world, "World");
        mem_pool_init(&mp_property, sizeof(Property), ps->property, "Property");
        mem_pool_init(&mp_collision, sizeof(Collision), ps->collision,
                      "Collision");        
#if ENABLE_TOUCH
        mem_pool_init(&mp_touch, sizeof(Touch), ps->touch, "Touch");
#endif
#if !ENABLE_SDL_VIDEO
        mem_pool_init(&mp_hackevent, sizeof(HackEvent), ps->hackevent,
                      "HackEvent");
#endif
#if TRACE_MAX
        mem_pool_init(&mp_bodytrace, sizeof(BodyState) * TRACE_MAX,
                      ps->bodytrace, "Body Trace");
        mem_pool_init(&mp_tiletrace, sizeof(TileState) * TRACE_MAX,
                      ps->tiletrace, "Tile Trace");
        mem_pool_init(&mp_shapetrace, sizeof(ShapeState) * TRACE_MAX,
                      ps->shapetrace, "Body Trace");
#endif
}

#if PLATFORM_IOS
#include <sys/sysctl.h>

/*
 * iPhone Simulator == i386
 * iPhone == iPhone1,1
 * 3G iPhone == iPhone1,2
 * 3GS iPhone == iPhone2,1
 * 4 iPhone == iPhone3,1
 * 1st Gen iPod == iPod1,1
 * 2nd Gen iPod == iPod2,1
 * 3rd Gen iPod == iPod3,1
 */
const char *
platform_name(void)
{
        static char buf[20] = "";
        if (*buf != '\0')
                return buf;     /* Return pointer to static platform string. */
        
        /* Find out platform and return a pointer to its name. */
        size_t bufsize = sizeof(buf);
        RCCHECK(sysctlbyname("hw.machine", buf, &bufsize, NULL, 0), 0);
        return buf;
}
#endif  /* PLATFORM_IOS */

#if !PLATFORM_IOS

GLuint main_framebuffer;
GLuint mainfb_texture_id;
float  mainfb_texture_s, mainfb_texture_t;

void
init_main_framebuffer(void)
{
        /* Do nothing if framebuffer extensions not present. */
        if (glGenFramebuffers == NULL)
                return;
                
        uint fb_texture_w = nearest_pow2(config.screen_width);
        uint fb_texture_h = nearest_pow2(config.screen_height);
        mainfb_texture_s = (float)config.screen_width / fb_texture_w;
        mainfb_texture_t = (float)config.screen_height / fb_texture_h;
        
        /* Generate main framebuffer texture. */
        glGenTextures(1, &mainfb_texture_id);
        texture_bind_id(mainfb_texture_id);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fb_texture_w, fb_texture_h,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        
        /* Generate and bind main framebuffer object. */
        glGenFramebuffers(1, &main_framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER_EXT, main_framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                               GL_TEXTURE_2D, mainfb_texture_id, 0);
        
        /* Reset back to default framebuffer. */
        glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
}

void
bind_main_framebuffer(void)
{
        /* Do nothing if framebuffer extensions not present. */
        if (glGenFramebuffers == NULL)
                return;
                
        glBindFramebuffer(GL_FRAMEBUFFER_EXT, main_framebuffer);
}

void
draw_main_framebuffer(void)
{
        /* Do nothing if framebuffer extensions not present. */
        if (glGenFramebuffers == NULL)
                return;

        /* Go back to default framebuffer and clear it. */
        glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
        glClear(GL_COLOR_BUFFER_BIT);

        /* Bind main framebuffer texture (the one we were drawing to). */
        texture_bind_id(mainfb_texture_id);
        
        /* Reset texture matrix. */
        glMatrixMode(GL_TEXTURE);
        glLoadIdentity();
        
        /* Set viewport. */
        glViewport(0, 0, config.window_width, config.window_height);
        
        /* Set up orthographic projection. */
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0, 1.0, 0.0, 1.0, 0.0, 1.0);
        
        /* Reset modelview matrix. */
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        /* Draw quad with default color mask and without blending. */
        glDisable(GL_BLEND);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDisableClientState(GL_COLOR_ARRAY);
        {
                glColor4f(1.0, 1.0, 1.0, 1.0);
                float w = (config.w_r - config.w_l) / 2.0;
                float h = (config.w_t - config.w_b) / 2.0;
                GLfloat vertex_array[] = {
                        0.5 - w, 0.5 - h,
                        0.5 + w, 0.5 - h,
                        0.5 - w, 0.5 + h,
                        0.5 + w, 0.5 + h
                };
                GLfloat texcoord_array[] = {
                        0.0,              0.0,
                        mainfb_texture_s, 0.0,
                        0.0,              mainfb_texture_t,
                        mainfb_texture_s, mainfb_texture_t
                };
                glVertexPointer(2, GL_FLOAT, 0, vertex_array);
                glTexCoordPointer(2, GL_FLOAT, 0, texcoord_array);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
        glEnableClientState(GL_COLOR_ARRAY);
        glEnable(GL_BLEND);
}

static void
calculate_screen_dimensions(void)
{
        float screen_aspect = (float)config.screen_width / config.screen_height;
        float window_aspect = (float)config.window_width / config.window_height;
        
        if (screen_aspect > window_aspect) {
                float offset = 0.5 * (1.0 - window_aspect / screen_aspect);
                config.w_b = offset;
                config.w_t = 1.0 - offset;
                config.w_l = 0.0;
                config.w_r = 1.0;
        } else {
                float offset = 0.5 * (1.0 - screen_aspect / window_aspect);
                config.w_b = 0.0;
                config.w_t = 1.0;
                config.w_l = offset;
                config.w_r = 1.0 - offset;
        }
}

#if !ENABLE_SDL2

static SDL_Surface *
set_video_mode(int w, int h, int fullscreen)
{
        Uint32 video_flags = (SDL_OPENGL | SDL_DOUBLEBUF | SDL_HWSURFACE);
        video_flags |= fullscreen ? SDL_FULLSCREEN : 0;
        log_msg("Attempting to set %dx%d fullscreen=%d", w, h, fullscreen);
        SDL_Surface *scr = SDL_SetVideoMode(w, h, 0, video_flags);
        if (scr != NULL) {
                config.window_width = w;
                config.window_height = h;
                calculate_screen_dimensions();
                config.fullscreen = fullscreen;
        } else {
                log_err("SDL_SetVideoMode() failed: %s", SDL_GetError());
        }
        return scr;
}

#endif  /* !ENABLE_SDL2 */

/*
 * Return true if we have framebuffer->texture rendering support.
 */
static int
set_gl_extensions(void)
{
        int r, g, b, a;
        SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &r);
        SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &g);
        SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &b);
        SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &a);
        log_msg("OpenGL platform: %s, %s, %s", glGetString(GL_RENDERER),
                glGetString(GL_VENDOR), glGetString(GL_VERSION));
        log_msg("Frambuffer component sizes (RGBA): %d %d %d %d", r, g, b, a);
        if (a == 0)
                log_warn("Missing framebuffer alpha.");

        /* Get extension function addresses. */
        glGenerateMipmap = (__typeof__(glGenerateMipmap))
            SDL_GL_GetProcAddress("glGenerateMipmapEXT");
        glGenFramebuffers = (__typeof__(glGenFramebuffers))
            SDL_GL_GetProcAddress("glGenFramebuffersEXT");
        glBindFramebuffer = (__typeof__(glBindFramebuffer))
            SDL_GL_GetProcAddress("glBindFramebufferEXT");
        glFramebufferTexture2D = (__typeof__(glFramebufferTexture2D))
            SDL_GL_GetProcAddress("glFramebufferTexture2DEXT");
        glDeleteFramebuffers = (__typeof__(glDeleteFramebuffers))
            SDL_GL_GetProcAddress("glDeleteFramebuffersEXT");
        assert((glGenFramebuffers && glBindFramebuffer &&
                glFramebufferTexture2D && glDeleteFramebuffers) ||
               (!glGenFramebuffers && !glBindFramebuffer &&
                !glFramebufferTexture2D && !glDeleteFramebuffers));
        return (glGenFramebuffers != NULL);
}

SDL_Window *
create_window(void)
{
#if ENABLE_SDL2
        /* Set OpenGL attributes. */
        RCCHECK(SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1), 0);
        //SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        //SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
        
        /* Get desktop width & height. */
        SDL_DisplayMode display;
        RCCHECK(SDL_GetDesktopDisplayMode(0, &display), 0);
        if (config.fullscreen && config.use_desktop) {
                /* Use native video mode while in fullscreen. */
                config.window_width = display.w;
                config.window_height = display.h;
        }
        calculate_screen_dimensions();
        
        /* Create window. */
        uint32_t flags = SDL_WINDOW_OPENGL;
        if (config.fullscreen) {
                flags |= SDL_WINDOW_FULLSCREEN;
                SDL_ShowCursor(SDL_DISABLE);
        }
        SDL_Window *win = SDL_CreateWindow(config.name, SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED,
                                           config.window_width,
                                           config.window_height, flags);
        if (win == NULL)
                fatal_error("SDL_CreateWindow() failed: %s.", SDL_GetError());
        SDL_ShowWindow(win);
        SDL_DisableScreenSaver();
        
        /* Create OpenGL context. */
        SDL_GLContext context = SDL_GL_CreateContext(win);
        if (context == NULL) {
                fatal_error("Could not create OpenGL context: %s",
                            SDL_GetError());
        }
        RCCHECK(SDL_GL_MakeCurrent(win, context), 0);
        set_gl_extensions();
#else
        int double_buf = 1;
        int swap_control = 1;

        /* Set OpenGL attributes. */
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, double_buf);
	/* SDL API documentation says that 
	   the attribute SDL_GL_SWAP_CONTROL is deprecated in 1.3.0 */
        //SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, swap_control);
        /*SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);*/

        /* Get desktop width & height. */
        const SDL_VideoInfo *vinfo = SDL_GetVideoInfo();
        assert(vinfo != NULL);

        /* Create game window. */
        int w = config.window_width;
        int h = config.window_height;
        SDL_WM_SetCaption(config.name, config.name);        
        SDL_Surface *scr = NULL;
        if (config.fullscreen && config.use_desktop)
                scr = set_video_mode(vinfo->current_w, vinfo->current_h, 1);
        else
                scr = set_video_mode(w, h, config.fullscreen);
        int fbuf_support = set_gl_extensions();
        if (!fbuf_support) {
                /* Missing framebuffer extensions for fullscreen scaling. */
                if (config.fullscreen && config.use_desktop)
                        scr = set_video_mode(w, h, 1);
                else if (scr == NULL)
                        scr = set_video_mode(w, h, !config.fullscreen);
		
		extern int height_offset;
		height_offset = (h - (int) config.screen_height) / 2;
        } else if (scr == NULL) {
                /*
                 * One last attempt: try windowed if fullscreen or fullscreen
                 * if windowed.
                 */
                scr = set_video_mode(w, h, !config.fullscreen);
        }
        if (scr == NULL)
                fatal_error("No more video modes to try.");
        
        int value;
        SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER, &value);
        if (value != double_buf)
                log_warn("Double buffer attribute: %i", value);
        SDL_GL_GetAttribute(SDL_GL_SWAP_CONTROL, &value);
        if (value != swap_control)
                log_warn("Swap control attribute: %i", value);
        
        SDL_Window *win = NULL;
#endif  /* !ENABLE_SDL2 */
        SDL_ShowCursor(SDL_DISABLE);
        return win;
}

int     getopt_bsd(int argc, char * const argv[], const char *optstring);

/*
 * Parse command line options.
 */
void
parse_cmd_opt(int argc, char *argv[])
{
        int opt;
        extern int opterr;
        extern char *optarg;
        
        opterr = 0;     /* Disable getopt_bsd() error reporting. */
        while ((opt = getopt_bsd(argc, argv, "fwL:")) != -1) {
                switch (opt) {
                case 'f':
                        config.fullscreen = 1;
                        break;
                case 'w':
                        config.fullscreen = 0;
                        break;
                case 'L':
                        assert(strlen(optarg) < sizeof(config.location));
                        strcpy(config.location, optarg);
                        break;
#ifndef __APPLE__
                default:
                        log_msg("Usage: %s [-f] [-w] [-L app_location]",
                                argv[0]);
                        log_msg("\t-w\tRun in windowed mode.");
                        log_msg("\t-f\tRun in fullscreen mode.");
                        log_msg("\t-L\tPath to application directory.");
                        exit(EXIT_FAILURE);
#endif
                }
        }
}
#endif

