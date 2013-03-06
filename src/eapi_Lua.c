#include "common.h"

#if ENABLE_LUA

#include <assert.h>
#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <math.h>
#include "eapi_C.h"
#include "audio.h"
#include "config.h"
#include "console.h"
#include "event.h"
#include "log.h"
#include "misc.h"
#include "texture.h"
#include "tile.h"
#include "world.h"
#include "util_lua.h"
#include "utlist.h"
#include "utstring.h"
#include "assert_lua.h"
#include "eapi_Lua.h"
#include "render.h"
#include "OpenGL_include.h"
#include "stepfunc.h"

#ifndef NDEBUG

#define objtype_error(L, obj)                                   \
do {                                                            \
        log_msg("Assertion failed in " SOURCE_LOC);             \
        luaL_error(L, "Unexpected object type: %s.",            \
                   object_name(obj));                           \
        abort();                                                \
} while (0)

#define valid_world(L, x) \
do { \
        info_assert_va(L, ((x) && ((World *)(x))->objtype == OBJTYPE_WORLD && \
                       ((World *)(x))->step_ms > 0 && !((World *)(x))->killme), \
                        "Invalid World object; looks like `%s` (if it is a " \
                        "World, then there is something else wrong).", object_name(x)); \
} while (0)

#define valid_body(L, x) \
do { \
        info_assert_va(L, ((x) && ((Body *)(x))->objtype == OBJTYPE_BODY), \
                       "Invalid Body object; looks more like `%s`.", object_name(x)); \
        valid_world(L, ((Body *)(x))->world); \
} while (0)

#define valid_camera(L, x) \
do { \
        info_assert_va(L, ((x) && ((Camera *)(x))->objtype == OBJTYPE_CAMERA), \
                       "Invalid Camera object; looks more like `%s`.", object_name(x)); \
        valid_body(L, &((Camera *)(x))->body); \
} while (0)

#define valid_shape(L, x) \
do { \
        info_assert_va(L, ((x) && ((Shape *)(x))->objtype == OBJTYPE_SHAPE && \
                (((Shape *)(x))->shape_type == SHAPE_RECTANGLE || \
                ((Shape *)(x))->shape_type == SHAPE_CIRCLE) && ((Shape *)x)->group != NULL), \
                "Invalid Shape object; looks more like `%s`.", object_name(x)); \
        valid_body(L, ((Shape *)(x))->body); \
} while (0)

#define valid_spritelist(L, x) \
do { \
        info_assert_va(L, ((x) && ((SpriteList *)(x))->objtype == OBJTYPE_SPRITELIST && \
                ((SpriteList *)(x))->tex && \
                ((SpriteList *)(x))->frames && ((SpriteList *)(x))->num_frames > 0), \
                "Invalid SpriteList object; looks more like `%s`", object_name(x)); \
} while (0)

#define valid_tile(L, x) \
do { \
        info_assert_va(L, ((x) && ((Tile *)(x))->objtype == OBJTYPE_TILE), \
                       "Invalid Tile object; looks more like `%s`.", object_name(x)); \
        valid_body(L, ((Tile *)(x))->body); \
        if (((Tile *)(x))->sprite_list != NULL) \
                valid_spritelist(L, ((Tile *)(x))->sprite_list); \
} while (0)
        
#else   /* NDEBUG */

#define objtype_error(...)      abort()
#define valid_world(...)        ((void)0)
#define valid_camera(...)       ((void)0)
#define valid_body(...)         ((void)0)
#define valid_tile(...)         ((void)0)
#define valid_shape(...)        ((void)0)
#define valid_spritelist(...)   ((void)0)

#endif  /* !NDEBUG */

static Body *
get_body(lua_State *L, void *obj)
{
        UNUSED(L);
        switch (*(int *)obj) {
        case OBJTYPE_BODY: {
                valid_body(L, obj);
                return obj;
        }
        case OBJTYPE_WORLD:  {
                valid_world(L, obj);
                return &((World *)obj)->static_body;
        }
        case OBJTYPE_CAMERA: {
                valid_camera(L, obj);
                return &((Camera *)obj)->body;
        }
        default:
                objtype_error(L, obj);
        }
        abort();
}

#if ENABLE_KEYS

/*
 * BindKeyboard(funcID, argID)
 *
 * Set keypress handling function.
 */
static int
LUA_BindKeyboard(lua_State *L)
{
        L_numarg_range(L, 2, 2);
        unsigned funcID = L_arg_uint(L, 1);
        intptr_t argID = L_arg_int(L, 2);
        
        extern EventFunc key_bind;
        key_bind.type = EVFUNC_TYPE_LUA;
        key_bind.func.lua_func_id = funcID;
        key_bind.callback_data = argID;
        
        return 0;
}

static int
LUA_GetKeyName(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        unsigned scancode = L_arg_uint(L, 1);
        
        const char *name = SDL_GetScancodeName(scancode);
        if (*name != '\0')
                lua_pushstring(L, name);
        else
                lua_pushnil(L);
        return 1;
}

static int
LUA_GetKeyFromName(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        const char *name = L_arg_cstr(L, 1);
        SDL_Scancode scancode = SDL_GetScancodeFromName(name);
        if (scancode != SDL_SCANCODE_UNKNOWN)
                lua_pushinteger(L, scancode);
        else
                lua_pushnil(L);
        return 1;
}

#endif  /* ENABLE_KEYS */

#if ENABLE_JOYSTICK

/*
 * BindJoystickButton(funcID, argID)
 *
 * Register function that handles joystick buttons.
 */
static int
LUA_BindJoystickButton(lua_State *L)
{
        open_joysticks();
        
        L_numarg_range(L, 2, 2);
        unsigned funcID = L_arg_uint(L, 1);
        intptr_t argID = L_arg_int(L, 2);
        
        extern EventFunc joybutton_bind;
        joybutton_bind.type = EVFUNC_TYPE_LUA;
        joybutton_bind.func.lua_func_id = funcID;
        joybutton_bind.callback_data = argID;
        
        return 0;
}

/*
 * BindJoystickAxis(funcID, argID)
 *
 * Register function that handles joystick axis movement.
 */
static int
LUA_BindJoystickAxis(lua_State *L)
{
        open_joysticks();
        
        L_numarg_range(L, 2, 2);
        unsigned funcID = L_arg_uint(L, 1);
        intptr_t argID = L_arg_int(L, 2);
        
        extern EventFunc joyaxis_bind;
        joyaxis_bind.type = EVFUNC_TYPE_LUA;
        joyaxis_bind.func.lua_func_id = funcID;
        joyaxis_bind.callback_data = argID;
        
        return 0;
}

#endif  /* ENABLE_JOYSTICK */

#if ENABLE_MOUSE

/*
 * BindMouseClick(cam, funcID, argID)
 *
 * Register function that handles mouse button clicks.
 */
static int
LUA_BindMouseClick(lua_State *L)
{
        L_numarg_range(L, 3, 3);
        Camera *cam = L_arg_userdata(L, 1);
        valid_camera(L, cam);
        unsigned func_id = L_arg_uint(L, 2);
        intptr_t arg_id = L_arg_int(L, 3);
        
        EventFunc *bind = &cam->mouseclick_bind;
        bind->type = EVFUNC_TYPE_LUA;
        bind->func.lua_func_id = func_id;
        bind->callback_data = arg_id;
        
        return 0;
}

/*
 * BindMouseMove(cam, funcID, argID)
 *
 * Register function that handles mouse motion events.
 */
static int
LUA_BindMouseMove(lua_State *L)
{
        L_numarg_range(L, 3, 3);
        Camera *cam = L_arg_userdata(L, 1);
        valid_camera(L, cam);
        unsigned func_id = L_arg_uint(L, 2);
        intptr_t arg_id = L_arg_int(L, 3);
        
        EventFunc *bind = &cam->mousemove_bind;
        bind->type = EVFUNC_TYPE_LUA;
        bind->func.lua_func_id = func_id;
        bind->callback_data = arg_id;
        
        return 0;
}

/*
 * MousePos(cam) -> {x=?, y=?}
 *
 * Mouse position within world that the given camera belongs to.
 */
static int
LUA_MousePos(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        Camera *cam = L_arg_userdata(L, 1);
        valid_camera(L, cam);
        
        L_push_vectf(L, mouse_pos(cam));
        return 1;
}

#endif  /* ENABLE_MOUSE */

/*
 * Function that does nothing.
 */
static int
LUA_Dummy(lua_State *L)
{
        UNUSED(L);
        return 0;
}

/*
 * What(obj) -> string
 *
 * Return a string describing what an object is ("Shape", "World", "Tile", etc).
 */
static int
LUA_What(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        void *obj = L_arg_userdata(L, 1);
        lua_pushstring(L, object_name(obj));
        return 1;
}

/*
 * NewWorld(name, step, grid_area, grid_cellsz, trace_skip=0) -> world
 *
 * Create a new world and return its pointer. World is the topmost data
 * structure (see world.h).
 *
 * name                 String parameter: choose a name for this world (useful
 *                      for debugging only).
 * step                 Duration of each world step in milliseconds.
 * grid_area            Partitioned space area. No objects may exist outside
 *                      this rectangular space, and an error will be raised if
 *                      something leaves it.
 * grid_cellsz          Size of grid cells. Each cell is a square. Make the
 *                      cells somewhat larger than the most abundant object in
 *                      the world.
 * trace_skip           When recording body state for the purpose of rewinding
 *                      time, this option allows to skip steps so as to save
 *                      memory space. Leaving `trace_skip` at zero will cause
 *                      each and every step to be recorded.
 */
static int
LUA_NewWorld(lua_State *L)
{
        L_numarg_range(L, 4, 5);
        const char *name = L_arg_cstr(L, 1);
        unsigned step = L_arg_uint(L, 2);
        BB area = L_arg_BB(L, 3);
        info_assert(L, area.l < area.r && area.b < area.t, "Invalid area box.");
        unsigned cellsz = L_arg_uint(L, 4);
        unsigned trace_skip = L_argdef_uint(L, 5, 0);
        
        World *w = world_new(name, step, area, cellsz, trace_skip);
        lua_pushlightuserdata(L, w);
        return 1;
}

/*
 * NewBody(parent, pos, sleepy=false) -> userdata
 *
 * Create a new body relative to parent.
 *
 * parent       If this is a world object, then body is created relative to
 *              world origin (attached to its static body).
 *              If `parent` is a Body object, then the new body is created
 *              relative to that.
 * pos          Relative offset from parent.
 * nocturnal    If true, the body never sleeps, even if outside camera
 *              vicinity.
 *
 * Body represents a physical object with position and (optionally) also
 * velocity and acceleration.
 * A body may own a list of Shapes, and a list of Tiles (tile = drawing area +
 * sprite list). Anything that has either a physical or graphical representation
 * in the game world is bound to a body object; even cameras have their own body
 * objects.
 */
static int
LUA_NewBody(lua_State *L)
{
        L_numarg_range(L, 2, 3);
        void *parent = L_arg_userdata(L, 1);
        vect_f pos = L_arg_vectf(L, 2);
        
        unsigned flags = 0;
#if !ALL_NOCTURNAL
        int nocturnal = L_argdef_bool(L, 3, 0);
        if (nocturnal)
                flags |= BODY_NOCTURNAL;
#endif
        lua_pushlightuserdata(L, body_new(get_body(L, parent), pos, flags));
        return 1;
}

/*
 * NewCamera(world, pos={0, 0}, size=screenSize, viewport=wholeScreen,
 *           sort=0) -> userdata
 *
 * Create a camera that sees the world it is bound to and renders its view to a
 * viewport.
 *
 * world        World as returned by NewWorld().
 * pos          Position vector.
 * size         Size vector: size of the area that camera is able to see.
 *              Passing zero as one of the two dimensions makes the engine
 *              calculate the other one based on viewport size (same aspect
 *              ratio for both). If `nil`, size is the same as viewport size.
 * viewport     Area of the window that camera draws to. Specified as bounding
 *              box: {l=?,r=?,b=?,t=?}, assuming coordinates are in pixels, with
 *              top left corner being (0,0). If `nil`, viewport is calculated
 *              from size. If size is missing too, then we draw to the whole
 *              screen.
 * sort         Determines camera sort order. Those with smaller `sort` values
 *              will be rendered first.
 */
static int
LUA_NewCamera(lua_State *L)
{
        L_numarg_range(L, 1, 5);
        World *world = L_arg_userdata(L, 1);
        vect_f pos = L_argdef_vectf(L, 2, (vect_f){0.0, 0.0});
        vect_i size_arg_lua;
        vect_i *size_arg = L_argptr_vecti(L, 3, &size_arg_lua);
        BB viewport_arg_lua;
        BB *viewport_arg = L_argptr_BB(L, 4, &viewport_arg_lua);
        int sort = L_argdef_int(L, 5, 0);
        
        valid_world(L, world);
        
        /* Init viewport box. */
        BB viewport;
        if (viewport_arg != NULL) {
                viewport = (BB){
                        .l=viewport_arg->l / config.pixel_scale,
                        .r=viewport_arg->r / config.pixel_scale,
                        .b=viewport_arg->b / config.pixel_scale,
                        .t=viewport_arg->t / config.pixel_scale
                };
        } else {
                if (size_arg != NULL) {
                        assert(size_arg->x > 0 && size_arg->y > 0);
                        viewport = (BB){
                                .l=0, .r=size_arg->x / config.pixel_scale,
                                .b=size_arg->y / config.pixel_scale, .t=0
                        };
                } else {
                        viewport = (BB){
                                .l=0, .r=config.screen_width,
                                .b=config.screen_height, .t=0
                        };
                }                
        }
        assert(viewport.r > viewport.l && viewport.b > viewport.t);
        
        vect_i size = {0, 0};
        if (size_arg != NULL) {
                size = *size_arg;
        } else {
                size.x = (viewport.r - viewport.l) * config.pixel_scale;
                size.y = (viewport.b - viewport.t) * config.pixel_scale;
        }
        assert(size.x >= 0 && size.y >= 0);
        assert(size.x > 0 || size.y > 0);
        
        /*
         * If one of the size components is zero, compute the other one using
         * viewport aspect ratio.
         */
        if (size.x == 0)
                size.x = (int)lroundf(size.y * (viewport.r - viewport.l) /
                                      (viewport.b - viewport.t));
        if (size.y == 0)
                size.y = (int)lroundf(size.x*(viewport.b - viewport.t) /
                                      (viewport.r - viewport.l));
        
        /* Create camera and position its body. */
        Camera *cam = cam_new(world, size, viewport);
        body_set_pos(&cam->body, pos);
        
        /*
         * Prepend to global camera list.
         * We must re-sort the list so that proper sort order is maintained.
         */
        extern Camera *cam_list;
        cam->sort = sort;               /* Don't forget to set "sort". */
        DL_PREPEND(cam_list, cam);
        DL_SORT(cam_list, cmp_cameras);
        
        lua_pushlightuserdata(L, cam);
        return 1;
}

/*
 * Parse texture specification argument. It can either be a string:
 *      "path/to/image"
 * or a table containing texture file name string and (optional) filter
 * attribute:
 *      {"path/to/image", filter=true/false}
 *
 * As an example, these are all valid texture specifications:
 *      {"image/player.png"}
 *      {"image/clouds.jpg", filter=true}
 *      "menu.png"
 */
static void
get_texture_spec(lua_State *L, int index, char *name, unsigned bufsize,
                 unsigned *flags)
{
        switch (lua_type(L, index)) {
        case LUA_TSTRING: {
                const char *filename = lua_tostring(L, index);
                RCLESS(snprintf(name, bufsize, "%s", filename), (int)bufsize);
                *flags = 0;
                return;
        }
        case LUA_TTABLE: {
                L_get_intfield(L, index, 1);
                const char *filename = lua_tostring(L, -1);
                info_assert(L, filename, "Missing image filename. Make sure "
                            "it's given correctly like this: "
                            "{\"path/to/image\", filter=true/false}.");
                RCLESS(snprintf(name, bufsize, "%s", filename), (int)bufsize);
                
                /* Set flags. */
                *flags = 0;
                L_get_strfield(L, index, "filter");
                info_assert(L, lua_isnil(L, -1) || lua_isboolean(L, -1),
                            "`filter` flag should be a boolean value.");
                SETFLAG(*flags, TEXFLAG_FILTER, lua_toboolean(L, -1));
                L_get_strfield(L, index, "intensity");
                info_assert(L, lua_isnil(L, -1) || lua_isboolean(L, -1),
                            "`intensity` flag should be a boolean value.");
                SETFLAG(*flags, TEXFLAG_INTENSITY, lua_toboolean(L, -1));
                return;
        }
        }
        luaL_error(L, "Texture name should either be a filename string "
                   "(e.g., \"path/to/file.png\") or a table with both filename "
                   "and filter setting (e.g., {\"path/to/file.png\", "
                   "filter=true}.");
}

/*
 * Get texture specification from user argument and find corresponding texture
 * struct. Texture is loaded into memory if not already there.
 */
static Texture *
load_texture(lua_State *L, int index)
{
        unsigned flags;
        char name[128];
        get_texture_spec(L, index, name, sizeof(name), &flags);

        Texture *tex = texture_load(name, flags);
        info_assert_va(L, tex != NULL, "Unable to load texture image `%s` "
                       "(not found).", name);
        return tex;
}

/*
 * NewSpriteList(texture, subimage, ..) -> userdata
 *
 * Create a sprite list; pointer to the C structure is returned for later
 * reference.
 *
 * texture      Texture file name (e.g., "image/hello.png"). To do linear
 *              filtering on this texture, pass in a table in the following
 *              format instead: {filename, filter=1}.
 * subimage, .. A list of rectangular sub-image specifications.
 *              Each subimage specification can have one of two forms: vector
 *              and bounding box.
 *              Vector form looks like this:
 *                      {{s, t}, {w, h}}
 *              where {s, t} is the position in pixel coordinates and {w, h} is
 *              width and height in pixels.
 *              In bounding box form:
 *                      {l=?, b=?, r=?, t=?}
 *              Left, bottom, right, top are the pixel coordinates of all four
 *              extremes.
 *
 * As a special case, if no sub-images are given (second argument is not given),
 * then a sprite-list containing a single frame is returned. This frame will be
 * the whole texture image.
 */
static int
LUA_NewSpriteList(lua_State *L)
{
        L_numarg_range(L, 1, 999);
        int n = lua_gettop(L);
        
        /* Read image from file and create OpenGL texture. */
        Texture *tex = load_texture(L, 1);
        
        /*
         * If no sub-image arguments are present, create a single frame that
         * contains the whole texture.
         */
        if (n == 1) {
                TexFrag tf = {
                        .l = 0,
                        .r = tex->w,
                        .b = tex->h,
                        .t = 0
                };
                assert(tf.r > tf.l && tf.b > tf.t);
                
                /* Create sprite-list and return its pointer. */
                lua_pushlightuserdata(L, spritelist_new(tex, &tf, 1));
                return 1;
        }
        
        /*
         * For each subimage spec figure out what form it is in. Then add the
         * four texture coordinates to temporary texture fragment array.
         */
        unsigned num_frames = 0;
        TexFrag tmp_frames[100];
        for (int i = 2; i <= n; i++) {
                luaL_checktype(L, i, LUA_TTABLE);
                lua_getfield(L, i, "l");
                
                /* Add a TexFrag to the frame list. */
                assert(num_frames < ARRAYSZ(tmp_frames));
                TexFrag *tf = &tmp_frames[num_frames++];
                
                /* Determine format and store into `tf`. */
                if (lua_isnil(L, -1)) {
                        info_assert(L, lua_objlen(L, i) == 2,
                                    "Expected {{s, t}, {w, h}}.");
                        L_get_intfield(L, i, 1); /* .. {s, t} */
                        L_get_intfield(L, i, 2); /* .. {s, t} {w, h} */
                        vect_i ST, WH;
                        L_getstk_intpair(L, -2, &ST.x, &ST.y);
                        L_getstk_intpair(L, -1, &WH.x, &WH.y);
                        lua_pop(L, 3);          /* ... */
                        tf->l = ST.x;
                        tf->r = ST.x + WH.x;
                        tf->b = ST.y + WH.y;
                        tf->t = ST.y;
                } else {
                        *tf = L_arg_TexFrag(L, i);
                        lua_pop(L, 1);                  /* ... */
                }
                assert(tf->r > tf->l && tf->b > tf->t);
        }
        
        /* Create sprite-list and return its pointer. */
        lua_pushlightuserdata(L, spritelist_new(tex, tmp_frames, num_frames));  
        return 1;
}

/*
 * ChopImage(image, size, first=0, total=all, skip=0, gap={0,0}) -> SpriteList
 *
 * Create a sprite-list by chopping up a texture into pieces.
 *
 * image        Image filename -- see NewSpriteList() for details.
 * size         Size of each individual sprite {widht, height} or
 *              {x=width, y=height}.
 * first        The first frame index used (zero is the very first one).
 * total        Total number of frames put into resulting sprite-list. `nil` or
 *              zero means take as many frames as possible.
 * skip         Number of frames to skip over in each step. Zero means that all
 *              frames will be consecutive.
 *
 * Chop texture into frames to produce a sprite-list. In the source image
 * sprites are assumed to be ordered left-to-right and top-to-bottom.
 */
static int
LUA_ChopImage(lua_State *L)
{
        L_numarg_range(L, 2, 6);
        vect_i size = L_arg_vecti(L, 2);
        unsigned first = L_argdef_uint(L, 3, 0);
        unsigned total = L_argdef_uint(L, 4, 0);
        unsigned skip = L_argdef_uint(L, 5, 0);
        vect_i gap = L_argdef_vecti(L, 6, (vect_i){0, 0});
        
        /* Read image from file and create OpenGL texture. */
        Texture *tex = load_texture(L, 1);
        
        assert(size.x > 0 && size.y > 0);
#if PLATFORM_IOS
        int factor = (config.screen_width > 500) ? 2 : 1;
        unsigned num_cols = tex->w / (size.x * factor); /* Number of columns. */
        unsigned num_rows = tex->h / (size.y * factor); /* Number of rows. */
#else        
        unsigned num_cols = tex->w / (size.x + gap.x);  /* Number of columns. */
        unsigned num_rows = tex->h / (size.y + gap.y);  /* Number of rows. */
#endif
        /* Create sprite-list frames. */
        unsigned num_frames = 0;
        TexFrag tmp_frames[1024];
        unsigned sentinel = (total > 0) ? first + (total * (skip+1)) :
                                          num_rows * num_cols;
        info_assert(L, sentinel <= num_rows * num_cols && first < sentinel,
                    "Image is missing frames.");
        for (unsigned i = first; i < sentinel; i += (1 + skip)) {
                assert(num_frames < ARRAYSZ(tmp_frames));
                TexFrag *tf = &tmp_frames[num_frames++];
                
                unsigned r = i / num_cols;
                unsigned c = i - r * num_cols;
#if PLATFORM_IOS
                tf->l = (c  ) * size.x * factor;
                tf->r = (c+1) * size.x * factor;
                tf->b = (r+1) * size.y * factor;
                tf->t = (r  ) * size.y * factor;
#else
                tf->l = c * (size.x + gap.x);
                tf->r = c * (size.x + gap.x) + size.x;
                tf->b = r * (size.y + gap.y) + size.y;
                tf->t = r * (size.y + gap.y);
#endif
        }
        assert(total == 0 || total == num_frames);
        lua_pushlightuserdata(L, spritelist_new(tex, tmp_frames, num_frames));
        return 1;
}

/*
 * NewShape(object, offset={0,0}, shapeTbl, groupName) -> shape
 *
 * Create a new shape.
 *
 * parent       Object to attach this shape to. Accepted are either Body objects
 *              or objects that contain bodies (Camera, World). If parent is
 *              a World object, then its static body is used as the actual
 *              parent.
 * offset       Position of shape relative to its body's position.
 * rect         Shape rectangle.
 * groupName    Name of collision group this shape will belong to.
 *
 * Note: Presently only rectangular shapes are supported, but there are some
 * distant plans to support circle shapes too.
 *
 * Circle:  {{centerX, centerY}, radius}
 * Rectangle: {l=?, r=?, b=?, t=?}
 */
static int
LUA_NewShape(lua_State *L)
{
        L_numarg_range(L, 4, 4);
        void *parent = L_arg_userdata(L, 1);
        vect_f offset = L_argdef_vectf(L, 2, (vect_f){0,0});
        BB rect = L_arg_BB(L, 3);
        info_assert(L, bb_valid(rect), "Shape rectangle invalid.");
        const char *groupName = L_arg_cstr(L, 4);
        
        /* Extract body pointer. */
        Body *body = get_body(L, parent);
        World *world = body->world;    /* Shorthand for world pointer. */
        
        /* Extract collision group name and find corresponding group. */
        assert(groupName && strlen(groupName) > 0);
        Group *group;
        HASH_FIND_STR(world->groups, groupName, group);
        
        /* Create group if it doesn't exist yet. */
        if (group == NULL) {
                extern mem_pool mp_group;
                group = mp_alloc(&mp_group);
                info_assert_va(L, strlen(groupName) < sizeof(group->name),
                            "Shape group name length exceeds maximum value "
                            "(%d).", sizeof(group->name));
                
                /* Set group name, ID, and add it to hash. */
                strcpy(group->name, groupName);
                group->index = world->next_group_id++;
                assert(group->index < SHAPEGROUPS_MAX);
                HASH_ADD_STR(world->groups, name, group);
        }
        
        /* Shape definition. */
        ShapeDef def = {
                .rect = {
                        .l=rect.l + offset.x,
                        .r=rect.r + offset.x,
                        .b=rect.b + offset.y,
                        .t=rect.t + offset.y
                }
        };
        
        /* Create shape. */
        Shape *s = shape_new(body, group, SHAPE_RECTANGLE, def);
        s->color = config.defaultShapeColor;
                        
        lua_pushlightuserdata(L, s);
        return 1;
}

static void
flip_sprites(Tile *t, int flipx, int flipy)
{
        /* Check if we need to flip. */
        if (!flipx && !flipy)
                return;
        
        SpriteList *sl = t->sprite_list;
        unsigned num_frames = sl->num_frames;
        TexFrag frames[num_frames];
        for (unsigned i = 0; i < num_frames; i++) {
                TexFrag *f = &sl->frames[i];
                if (!flipx) {
                        frames[i].l = f->l;
                        frames[i].r = f->r;
                } else {
                        frames[i].r = f->l;
                        frames[i].l = f->r;
                }
                if (!flipy) {
                        frames[i].b = f->b;
                        frames[i].t = f->t;
                } else {
                        frames[i].t = f->b;
                        frames[i].b = f->t;
                }
        }
        
        t->sprite_list = spritelist_new(sl->tex, frames, num_frames);
}

/*
 * Assign sprite-list to a tile.
 *
 * tile         Tile pointer as returned by NewTile().
 * spriteList   Sprite-list pointer as returend by NewSpriteList() or `nil` if
 *              current sprite-list should be removed from tile.
 */
static int
LUA_SetSpriteList(lua_State *L)
{
        L_numarg_range(L, 2, 2);
        Tile *t = L_arg_userdata(L, 1);
        SpriteList *sprite_list = L_arg_userdata(L, 2);
        
        /*
         * If explicit tile size was not previously set, calculate it from
         * either current or previous sprite-list.
         */
        valid_tile(L, t);
        if (tile_size(t).x <= 0) {
                SpriteList *sz_sprite_list = sprite_list != NULL ? sprite_list
                                                               : t->sprite_list;
                valid_spritelist(L, sz_sprite_list);
                
                /* Get size that accomodates all frames. */
                vect_i size = texfrag_maxsize(sz_sprite_list->frames,
                                              sz_sprite_list->num_frames);
                
                /*
                 * If sprite-list is set, store this calculated size as
                 * negative so drawing code knows individual sprite-list frame
                 * size must be used instead. This negative value will then only
                 * be used when updating the grid.
                 */
                if (sprite_list != NULL) {
                        size.x = -size.x;
                        size.y = -size.y;
                }
                tile_set_size(t, (vect_f){size.x, size.y});
        }
        
        /* Assign sprite list and reset frame index. */
        t->sprite_list = sprite_list;
        tile_set_frame(t, 0);
        
        /* Flip original sprite if necessary. */
        if (sprite_list != NULL) {
                flip_sprites(t, t->flags & TILE_FLIP_X, t->flags & TILE_FLIP_Y);
        }
        return 0;
}

/*
 * NewTile(parent, offset={0,0}, size, sprite_list=nil, depth=0.0)
 *
 * Create a new tile.
 *
 * parent       Object the tile will be attached to. Body, Camera, and World
 *              objects are accepted. If World, then its static body is used as
 *              parent.
 * offset       Tile offset (lower left corner) relative to object's position.
 * size         Tile size. If `nil` or {0,0}, then sprite (current frame) size
 *              is used.
 * sprite_list  Sprite list pointer as returned by NewSpriteList(). If `nil`,
 *              tile has no texture (color only). In this case size must be
 *              given.
 * depth        Depth determines drawing order. Into the screen is negative.
 */
static int
LUA_NewTile(lua_State *L)
{
        L_numarg_range(L, 3, 5);
        void *parent = L_arg_userdata(L, 1);
        vect_f offset = L_argdef_vectf(L, 2, (vect_f){0,0});
        vect_f size_arg_store;
        vect_f *size_arg = L_argptr_vectf(L, 3, &size_arg_store);
        SpriteList *sprite_list = L_argdef_userdata(L, 4, NULL);
        float depth = L_argdef_float(L, 5, 0.0);
        
#ifndef NDEBUG
        if (sprite_list == NULL) {
                info_assert(L, size_arg, "Missing size argument. If "
                            "sprite-list is not given, then size must be.");
        }
        if (size_arg != NULL) {
                info_assert_va(L, size_arg->x > 0 && size_arg->y > 0,
                               "Negative size value(s): (%.2f, %.2f).",
                               size_arg->x, size_arg->y);
        }
#endif
        /* Get parent Body object. */
        Body *body = get_body(L, parent);
        
        vect_f size = {-10, -10};
        if (size_arg != NULL) {
                size = *size_arg;
                assert(size.x > 0.0 && size.y > 0.0);
        }
        
        /* Create tile. */
        int grid_store = (*(int *)parent != OBJTYPE_CAMERA);
        Tile *t = tile_new(body, offset, size, depth, grid_store);
        
        /* Call SetSpriteList(). */
        if (sprite_list != NULL) {
                lua_pushcfunction(L, LUA_SetSpriteList);
                lua_pushlightuserdata(L, t);
                lua_pushlightuserdata(L, sprite_list);
                lua_call(L, 2, 0);
        }
        
        /* Return tile. */
        lua_pushlightuserdata(L, t);
        return 1;
}

/*
 * Same as NewTile() except `offset` is interpreted as the desired center point
 * of created tile. The actual tile offset will still remain at its lower left
 * corner, so calling GetPos() on the new tile will not return the `offset`
 * argument to NewTileCentered() but instead the offset of its lower left
 * corner.
 */
static int
LUA_NewTileCentered(lua_State *L)
{
        /* Call NewTile(). */
        int n = lua_gettop(L);
        lua_pushcfunction(L, LUA_NewTile);
        for (int i = 1; i <= n; i++)
                lua_pushvalue(L, i);
        lua_call(L, n, 1);
        
        /* Center tile. */
        Tile *t = lua_touserdata(L, n + 1);
        vect_f final_size = GetSize(t);
        vect_f pos = GetPos(t);
        SetPos(t, (vect_f){pos.x - final_size.x/2, pos.y - final_size.y/2});
        
        return 1;
}

/*
 * Clone(obj) -> cloned_obj
 *
 * Produce an exact copy of given object.
 */
static int
LUA_Clone(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        void *obj = L_arg_userdata(L, 1);
        
        switch (*(int *)obj) {
        case OBJTYPE_TILE: {
                Tile *t = tile_clone(((Tile *)obj)->body, obj);
                lua_pushlightuserdata(L, t);
                return 1;
        }
        case OBJTYPE_SHAPE: {
                Shape *s = shape_clone(((Shape *)obj)->body, obj);
                lua_pushlightuserdata(L, s);
                return 1;
        }
        case OBJTYPE_BODY: {
                Body *b = body_clone(obj);
                lua_pushlightuserdata(L, b);
                return 1;
        }
        }
        objtype_error(L, obj);
}

/*
 * Get the world that object belongs to. If object is a world itself, return it.
 */
static int
LUA_GetWorld(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        void *obj = L_arg_userdata(L, 1);
        
        switch (*(int *)obj) {
        case OBJTYPE_WORLD: {
                valid_world(L, obj);
                lua_pushlightuserdata(L, obj);
                return 1;
        }
        case OBJTYPE_BODY: {
                valid_body(L, obj);
                lua_pushlightuserdata(L, ((Body *)obj)->world);
                return 1;
        }
        case OBJTYPE_SHAPE: {
                valid_shape(L, obj);
                lua_pushlightuserdata(L, ((Shape *)obj)->body->world);
                return 1;
        }
        case OBJTYPE_CAMERA: {
                valid_camera(L, obj);
                lua_pushlightuserdata(L, ((Camera *)obj)->body.world);
                return 1;
        }
        case OBJTYPE_TILE: {
                valid_tile(L, obj);
                lua_pushlightuserdata(L, obj);
                return 1;
        }
        }
        objtype_error(L, obj);
}

/*
 * Get the body that object belongs to. If object is a body itself, return it.
 * If object is a world, return its static body.
 */
static int
LUA_GetBody(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        void *obj = L_arg_userdata(L, 1);
        
        switch (*(int *)obj) {
        case OBJTYPE_WORLD: {
                valid_world(L, obj);
                lua_pushlightuserdata(L, &((World *)obj)->static_body);
                return 1;
        }
        case OBJTYPE_TILE: {
                valid_tile(L, obj);
                lua_pushlightuserdata(L, ((Tile *)obj)->body);
                return 1;
        }
        case OBJTYPE_SHAPE: {
                valid_shape(L, obj);
                lua_pushlightuserdata(L, ((Shape *)obj)->body);
                return 1;
        }
        case OBJTYPE_BODY: {
                valid_body(L, obj);
                lua_pushlightuserdata(L, obj);
                return 1;
        }
        case OBJTYPE_CAMERA: {
                valid_camera(L, obj);
                lua_pushlightuserdata(L, &((Camera *)obj)->body);
                return 1;
        }
        }
        objtype_error(L, obj);
}

/*
 * GetTiles(body) -> {tiles}
 *
 * body         Body, Camera, or World.
 */
static int
LUA_GetTiles(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        void *obj = L_arg_userdata(L, 1);
        
        Body *body = get_body(L, obj);
        lua_newtable(L);
        Tile *t;
        int num_tiles = 0;
        DL_FOREACH(body->tiles, t) {
                lua_pushinteger(L, ++num_tiles);
                lua_pushlightuserdata(L, t);
                lua_rawset(L, -3);
        }
        return 1;
}

/*
 * Get shape definition in world coordinate space.
 */
static int
LUA_GetShape(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        Shape *s = L_arg_userdata(L, 1);
        valid_shape(L, s);
        
        /* Get absolute position of shape's parent body. */
        vect_f pos = GetAbsolutePos(s->body);
 
        assert(s->shape_type == SHAPE_RECTANGLE);
        ShapeDef def = shape_def(s);
        def.rect.l += pos.x;
        def.rect.r += pos.x;
        def.rect.b += pos.y;
        def.rect.t += pos.y;
        
        L_push_BB(L, def.rect);
        return 1;
}

/*
 * GetImageSize(image) -> {x=?, y=?}
 *
 * Return image size in pixels. The image (texture) is laoded into memory if not
 * already there. Note that if texture is later used filtered, then the `filter`
 * argument (see NewSpriteList) must be present to avoid loading the image
 * twice.
 */
static int
LUA_GetImageSize(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        
        /* Read image from file and create OpenGL texture. */
        Texture *tex = load_texture(L, 1);
#if PLATFORM_IOS
        if (config.screen_width > 500) {
                L_push_vecti(L, (vect_i){tex->w / 2, tex->h / 2});
                return 1;
        }
#endif
        L_push_vecti(L, (vect_i){tex->w, tex->h});
        return 1;
}

/*
 * GetTime(obj) -> time, stepSec
 *
 * Each body has its own time. The function returns two values: seconds since
 * body creation, and the length of one step in seconds.
 *
 * object       An object with its own timeline: Body, Camera, World.
 */
static int
LUA_GetTime(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        void *obj = L_arg_userdata(L, 1);
        
        /* Now = (current step number) * (step duration in seconds) */
        Body *b = get_body(L, obj);
        float step_sec = b->world->step_sec;
        lua_pushnumber(L, b->step * step_sec);
        lua_pushnumber(L, step_sec);
        return 2;
}

/*
 * GetSpriteList(tile) -> spriteList
 *
 * Return sprite-list that was previously assigned to tile or `nil` if tile does
 * not have a sprite-list.
 */
static int
LUA_GetSpriteList(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        Tile *t = L_arg_userdata(L, 1);
        valid_tile(L, t);
        
        if (t->sprite_list != NULL)
                lua_pushlightuserdata(L, t->sprite_list);
        else
                lua_pushnil(L);
        return 1;
}

/*
 * Set camera zoom value.
 *
 * camera       Camera object as returned by NewCamera().
 * zoom         Zoom = 1 means no zoom. More than one zooms in, less than one
 *              zooms out.
 */
static int
LUA_SetZoom(lua_State *L)
{
        L_numarg_range(L, 2, 2);
        Camera *cam = L_arg_userdata(L, 1);
        float zoom = L_arg_float(L, 2);
        
        valid_camera(L, cam);
        info_assert_va(L, zoom > 0.0, "Zoom value (%f) must be positive.", zoom);
        if (zoom < 0.1)
                zoom = 0.1;
        if (zoom > 100.9)
                zoom = 100.9;
        
        cam->zoom = zoom;
        return 0;
}

/*
 * SetStepC(obj, C_Function, argID)
 *
 * Set the step function of an object (Body, Camera, World). The function must
 * be a function pointer.
 */
static int
LUA_SetStepC(lua_State *L)
{
        L_numarg_range(L, 2, 3);
        void *obj = L_arg_userdata(L, 1);
        StepFunction sf = L_arg_userdata(L, 2);
        intptr_t arg_id = L_arg_int(L, 3);
        
        /* Set step function. */
        Body *body = get_body(L, obj);
        body->flags |= BODY_STEP_C;
        body->step_func = (intptr_t)sf;
        body->step_cb_data = arg_id;
        return 0;
}

/*
 * SetStep(object, stepFuncID, argID)
 *
 * Set the step function of an object. Setting the step function of a World
 * object sets the step function of its static body.
 *
 * object               Accepted objects: Body, Camera, World.
 * stepFuncID           Step function ID.
 * argID                User argument ID.
 */
static int
LUA_SetStep(lua_State *L)
{
        L_numarg_range(L, 3, 3);
        void *obj = L_arg_userdata(L, 1);
        int sf = L_argdef_int(L, 2, 0);
        intptr_t argID = L_arg_int(L, 3);
        info_assert(L, sf != 0 || argID == 0, "Step-function missing but user "
                    "arguments present.");
                
        /* Set step function. */
        Body *body = get_body(L, obj);
        body->flags &= ~BODY_STEP_C;
        body->step_func = sf;
        body->step_cb_data = argID;
        return 0;
}

/*
 * SetAfterStep(object, afterstepFuncID, argID)
 *
 * Set the after-step function of an object. Setting the after-step function of
 * a World object sets the after-step function of its static body.
 *
 * object               Accepted objects: Body, Camera, World.
 * afterstepFuncID      After-step function ID.
 * argID                User argument ID.
 */
static int
LUA_SetAfterStep(lua_State *L)
{
        L_numarg_range(L, 3, 3);
        void *obj = L_arg_userdata(L, 1);
        int sf = L_argdef_int(L, 2, 0);
        intptr_t argID = L_arg_int(L, 3);
        info_assert(L, sf != 0 || argID == 0, "Step-function missing but user "
                    "arguments present.");
                
        /* Set step function. */
        Body *body = get_body(L, obj);
        body->flags &= ~BODY_AFTERSTEP_C;
        body->afterstep_func = sf;
        body->afterstep_cb_data = argID;
        return 0;
}

/*
 * GetStep(obj) -> stepFunctionID, argID
 *
 * obj          Body, Camera, or World.
 */
static int
LUA_GetStep(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        void *obj = L_arg_userdata(L, 1);
        
        Body *body = get_body(L, obj);
        if (!body->step_func) {
                lua_pushnil(L);
                lua_pushinteger(L, 0);
                info_assert(L, body->step_cb_data == 0, "Step-function missing "
                            "but user arguments present.");
                return 2;
        }
        
        /*
         * Push function pointer as userdata if step function is a C function,
         * or Lua function index otherwise.
         */
        if (body->flags & BODY_STEP_C)
                lua_pushlightuserdata(L, (void *)body->step_func);
        else
                lua_pushinteger(L, body->step_func);
        
        /* Push user argument index. */
        lua_pushinteger(L, body->step_cb_data);
        return 2;
}

/*
 * GetAfterStep(obj) -> stepFunctionID, argID
 *
 * obj          Body, Camera, or World.
 */
static int
LUA_GetAfterStep(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        void *obj = L_arg_userdata(L, 1);
        
        Body *body = get_body(L, obj);
        if (!body->afterstep_func) {
                lua_pushnil(L);
                lua_pushinteger(L, 0);
                info_assert(L, body->step_cb_data == 0, "Step-function missing "
                            "but user arguments present.");
                return 2;
        }
        lua_pushinteger(L, body->afterstep_func);
        lua_pushinteger(L, body->afterstep_cb_data);
        return 2;
}

/*
 * ShowCursor()
 *
 * Show default mouse cursor.
 */
static int
LUA_ShowCursor(lua_State *L)
{
        UNUSED(L);
        L_numarg_range(L, 0, 0);
        SDL_ShowCursor(SDL_ENABLE);
        return 0;
}

/*
 * HideCursor()
 *
 * Hide default mouse cursor.
 */
static int
LUA_HideCursor(lua_State *L)
{
        UNUSED(L);
        L_numarg_range(L, 0, 0);
        SDL_ShowCursor(SDL_DISABLE);
        return 0;
}

/*
 * NextCamera(cam) -> nextCamera
 *
 * cam          Camera as returned by NewCamera(), or nil to return first camera
 *              from the list.
 *
 * Iterate over cameras.
 */
static int
LUA_NextCamera(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        Camera *cam = L_argdef_userdata(L, 1, NULL);
        valid_camera(L, cam);
        
        /* Return next camera from list if it exists. */
        if (cam->next != NULL) {
                lua_pushlightuserdata(L, cam->next);
                return 1;
        }
        
        /* Return first camera from list if it exists, or nil if it doesn't. */
        extern Camera *cam_list;
        if (cam_list != NULL)
                lua_pushlightuserdata(L, cam_list);
        else
                lua_pushnil(L);
        return 1;
}

/*
 * Pause(world)
 *
 * Pause world (everything becomes frozen).
 *
 * Pause(body)
 *
 * Pause body or camera time.
 */
static int
LUA_Pause(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        void *obj = L_arg_userdata(L, 1);
        
        switch (*(int *)obj) {
        case OBJTYPE_WORLD: {
                valid_world(L, obj);
#if ENABLE_AUDIO
                /* Pause world audio. */
                audio_pause_group((uintptr_t)obj);
#endif
                ((World *)obj)->paused = 1;
                return 0;
        }
        case OBJTYPE_BODY:
        case OBJTYPE_CAMERA: {
                body_pause(get_body(L, obj));
                return 0;
        }
        }
        objtype_error(L, obj);
}

/*
 * Resume(world)
 *
 * Undo world pause.
 *
 * Resume(body)
 *
 * Resume body or camera time.
 */
static int
LUA_Resume(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        void *obj = L_arg_userdata(L, 1);
        
        switch (*(int *)obj) {
        case OBJTYPE_WORLD: {
                valid_world(L, obj);
#if ENABLE_AUDIO
                /* Resume world audio. */
                audio_resume_group((uintptr_t)obj);
#endif
                ((World *)obj)->paused = 0;
                return 0;
        }
        case OBJTYPE_BODY:
        case OBJTYPE_CAMERA: {
                body_resume(get_body(L, obj));
                return 0;
        }
        }
        objtype_error(L, obj);
}

/*
 * Enable(camera)
 */
static int
LUA_Enable(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        void *obj = L_arg_userdata(L, 1);
        
        switch (*(int *)obj) {
        case OBJTYPE_CAMERA: {
                ((Camera *)obj)->disabled = 0;
                return 0;
        }
        }
        objtype_error(L, obj);
}

/*
 * Disable(camera)
 */
static int
LUA_Disable(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        void *obj = L_arg_userdata(L, 1);
        
        switch (*(int *)obj) {
        case OBJTYPE_CAMERA: {
                ((Camera *)obj)->disabled = 1;
                return 0;
        }
        }
        objtype_error(L, obj);
}

#if TRACE_MAX

/*
 * Record(body) -> now
 *
 * Start recording trace. Returns current body time.
 */
static int
LUA_Record(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        void *obj = L_arg_userdata(L, 1);
        
        Body *b = get_body(L, obj);
        info_assert(L, !b->trace, "Body is already being recorded.");
        body_start_recording(b);
        
        lua_pushnumber(L, b->step * b->world->step_sec);
        return 1;
}

/*
 * Snapshot(body) -> now
 *
 * Record a single snapshot of body. Time of recorded snapshot (i.e., body's
 * present time) is returned.
 */
static int
LUA_Snapshot(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        void *obj = L_arg_userdata(L, 1);
        
        float now = body_snapshot(obj);
        lua_pushnumber(L, now);
        return 1;
}

/*
 * Rewind(body, time) -> rewind_time
 *
 * Rewind body to `time` seconds since its creation. The body is automatically
 * paused so that it can be fast-forwarded if necessary.
 *
 * Not all time values correspond directly to a saved state (trace). Thus
 * rewinding is not accurate and closest trace will be chosen. Rewind() returns
 * the time that the body is actually rewound to.
 */
static int
LUA_Rewind(lua_State *L)
{
        L_numarg_range(L, 2, 2);
        void *obj = L_arg_userdata(L, 1);
        float time = L_arg_float(L, 2);
        info_assert(L, time >= 0.0, "Negative time value.");
        
        Body *b = get_body(L, obj);
        info_assert(L, b->trace, "Body cannot be rewound because no "
                    "trace/snapshots exist for it."); 
                    
        float rew_time;
        body_pause(b);
        if (body_rewind(b, time, &rew_time))
                lua_pushnumber(L, rew_time);
        else
                lua_pushnil(L);
        return 1;
}

#endif  /* TRACE_MAX */

/*
 * SetBoundary(camera, boundingBox)
 *
 * camera       Camera object pointer as returned by NewCamera().
 * boundingBox  Bounding box spec: {l=?, r=?, b=?, t=?}. If nil, remove bounds.
 *
 * Restrict camera movement to a bounding box.
 */
static int
LUA_SetBoundary(lua_State *L)
{
        L_numarg_range(L, 2, 2);
        Camera *cam = L_arg_userdata(L, 1);
        valid_camera(L, cam);
        BB bb_store;
        BB *bb = L_argptr_BB(L, 2, &bb_store);
        
        if (bb != NULL) {
                info_assert(L, bb_valid(*bb), "Invalid bounding box.");
                info_assert(L, cam->size.x <= bb->r-bb->l &&
                                cam->size.y <= bb->t-bb->b, "Bounding box must "
                            "be bigger than camera-visible area size.");
        } else {
                cam->box = (BB){0, 0, 0, 0};
        }        
        return 0;
}

/*
 * Log(msg)
 *
 * Output log message.
 */
static int
LUA_Log(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        
        /* Copy string into (next) console line buffer. */
        console.last_line = (console.last_line + 1) % CONSOLE_MAX_LINES;
        snprintf(console.buffer[console.last_line], sizeof(console.buffer[0]),
                 "%s", L_arg_cstr(L, 1));
        
        /* Save log time. */
        extern uint64_t game_time;
        console.log_time[console.last_line] = game_time;
        
        return 0;
}

/*
 * GetFrame(tile) -> frameIndex
 *
 * Get current frame index.
 */
static int
LUA_GetFrame(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        Tile *t = L_arg_userdata(L, 1);
        valid_tile(L, t);
        
        lua_pushnumber(L, tile_frame(t));
        return 1;
}

/*
 * SetFrame(tile, frameNumber)
 *
 * tile         Tile object as returned by NewTile().
 * frameNumber  Index into a tile's sprite list.
 *
 * Select frame manually.
 */
static int
LUA_SetFrame(lua_State *L)
{
        L_numarg_range(L, 2, 2);
        Tile *t = L_arg_userdata(L, 1);
        unsigned frame_index = L_arg_uint(L, 2);
        
        valid_tile(L, t);
        info_assert(L, t->sprite_list, "Cannot set frame because tile has no sprite-list.");
        info_assert_va(L, frame_index < t->sprite_list->num_frames,
                       "Frame index (%d) out of range [%d..%d]", frame_index, 0, t->sprite_list->num_frames);
        tile_set_frame(t, frame_index);
        return 0;
}

/*
 * Animate(obj, animType, FPS, startTime=0)
 *
 * Do frame animation from first to last frame.
 *
 * obj          Tile object.
 * animType     eapi.ANIM_CLAMP, eapi.ANIM_LOOP, etc. See property.h for a full
 *              list.
 * FPS          Animation speed: frames per second. If negative, animation is
 *              assumed to start from last frame and go backwards.
 * startTime    When to assume the animation started, in seconds relative to
 *              current world time.
 */
static int
LUA_Animate(lua_State *L)
{
        L_numarg_range(L, 3, 4);
        Tile *t = L_arg_userdata(L, 1);
        valid_tile(L, t);
        uint8_t anim_type = L_arg_uint(L, 2);
        float FPS = L_arg_float(L, 3);
        float start_time = L_argdef_float(L, 4, 0.0);
        
        valid_spritelist(L, t->sprite_list);
        unsigned num_frames = t->sprite_list->num_frames;
#ifndef NDEBUG
        if (num_frames < 2)
                log_warn("Animating sprite-list with less than 2 frames!");
#endif
        info_assert(L, FPS != 0.0, "Zero frames per second.");
        int first, last;
        if (FPS > 0.0) {
                first = 0;
                last = num_frames - 1;
        } else {
                first = num_frames - 1;
                last = 0;
        }
        float duration = (float)(abs(last-first)+1) / fabs(FPS);
        tile_set_frame(t, first);
        if (start_time > 0.0)
                start_time = fmod(start_time, duration) - duration;
        tile_anim_frame(t, anim_type, last, duration, start_time);
        return 0;
}

/*
 * AnimateFrame(tile, animType, start=0, end=lastFrame, from=current, duration)
 *
 * Do frame animation.
 *
 * tile         Tile as returned by NewTile().
 * animType     eapi.ANIM_CLAMP, eapi.ANIM_LOOP, etc. See property.h for a full
 *              list.
 * start        Frame number that starts the animation.
 * end          Last frame in animation sequence.
 * from         Adjust animation start time so it jumps to this frame.
 * duration     Time it takes for frame number to go from `start` to `end`.
 */
static int
LUA_AnimateFrame(lua_State *L)
{
        L_numarg_range(L, 6, 6);
        Tile *t = L_arg_userdata(L, 1);
        valid_spritelist(L, t->sprite_list);
        int num_frames = t->sprite_list->num_frames;
        uint8_t anim_type = L_arg_uint(L, 2);
        valid_tile(L, t);
        int start = L_argdef_int(L, 3, 0);
        int end = L_argdef_int(L, 4, num_frames - 1);
        int from = L_argdef_int(L, 5, (int)GetFrame(t));
        float duration = L_arg_float(L, 6);
#ifndef NDEBUG
        if (num_frames < 2)
                log_warn("Animating sprite-list with less than 2 frames!");
        info_assert_va(L, end < num_frames, "End frame number (%d) out of "
                       "range. SpriteList has %d frames.", end, num_frames);
#endif
        /* Adjust start_time so animation jumps to `from` frame. */
        float start_time = (float)abs(from-start) * duration / (abs(end-start) + 1);
        tile_set_frame(t, start);
        tile_anim_frame(t, anim_type, end, duration, -start_time);
        return 0;
}

/*
 * AnimatingFrame(tile) -> boolean
 *
 * Return true if tile has ongoing frame animation; false otherwise.
 */
static int
LUA_AnimatingFrame(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        Tile *t = L_arg_userdata(L, 1);
        valid_tile(L, t);
        
        Property *anim = t->frame;
        if (anim == NULL) {
                lua_pushboolean(L, 0);
                return 1;
        }
        lua_pushboolean(L, (anim->anim_type != ANIM_NONE));
        return 1;
}

/*
 * StopAnimation(obj)
 *
 * Stop frame animation at the current frame.
 *
 * obj          Tile object.
 */
static int
LUA_StopAnimation(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        void *obj = L_arg_userdata(L, 1);
        
        SetFrame(obj, GetFrame(obj));
        return 0;
}

/*
 * SetAnimPos(tile, animPos)
 *
 * tile         Tile object as returned by NewTile().
 * animPos      See below.
 *
 * Set tile animation position. animPos is used to calculate the current frame
 * number. As it goes from 0.0 to 1.0, each frame in the animation will be
 * shown. Once it reaches 1.0, however, the animation starts repeating.
 */
static int
LUA_SetAnimPos(lua_State *L)
{
        L_numarg_range(L, 2, 2);
        Tile *t = L_arg_userdata(L, 1);
        valid_tile(L, t);
        
        int num_frames = t->sprite_list->num_frames;        
        float anim_pos = L_arg_float(L, 2);
        anim_pos -= floor(anim_pos);
        tile_set_frame(t, floor(anim_pos * (num_frames - 1)));
        return 0;
}

/*
 * SetAngle(tile, angle, pivot={0,0})
 */
static int
LUA_SetAngle(lua_State *L)
{
        L_numarg_range(L, 2, 3);
        void *obj = L_arg_userdata(L, 1);
        float angle = L_arg_float(L, 2);
        vect_f pivot = L_argdef_vectf(L, 3, (vect_f){0.0, 0.0});
        
        valid_tile(L, obj);
        tile_set_angle(obj, pivot, angle);
        return 0;
}

/*
 * AnimateAngle(obj, type, pivot={0,0}, angle, duration, startTime=0)
 */
static int
LUA_AnimateAngle(lua_State *L)
{
        L_numarg_range(L, 5, 6);
        Tile *t = L_arg_userdata(L, 1);
        valid_tile(L, t);
        uint8_t type = L_arg_uint(L, 2);
        vect_f pivot = L_arg_vectf(L, 3);
        float angle = L_arg_float(L, 4);
        float duration = L_arg_float(L, 5);
        float start_time = L_argdef_float(L, 6, 0.0);

        tile_anim_angle(t, type, pivot, angle, duration, start_time);
        return 0;
}

/*
 * FlipX(tile, flip=nil) -> flip
 *
 * tile         Tile.
 * flip         Boolean value, may be omitted.
 *
 * Set if tile is horizontally flipped. Returns the present horizontal flip
 * state.
 */
static int
LUA_FlipX(lua_State *L)
{
        L_numarg_range(L, 1, 2);
        Tile *t = L_arg_userdata(L, 1);
        valid_tile(L, t);

        if (lua_isnoneornil(L, 2)) {
                lua_pushboolean(L, (t->flags & TILE_FLIP_X));
                return 1;
        }
        
        int was_flipped = (t->flags & TILE_FLIP_X);
        if (L_arg_bool(L, 2)) {
                t->flags |= TILE_FLIP_X;
                if (t->sprite_list != NULL && !was_flipped)
                        flip_sprites(t, 1, 0);
        } else {
                t->flags &= ~TILE_FLIP_X;
                if (t->sprite_list != NULL && was_flipped)
                        flip_sprites(t, 1, 0);
        }
        
        lua_pushboolean(L, (t->flags & TILE_FLIP_X));
        return 1;
}

/*
 * FlipY(tile, flip=nil) -> flip
 *
 * tile         Tile.
 * flip         Boolean value, may be omitted.
 *
 * Set if tile is vertically flipped. Returns the present vertical flip state.
 */
static int
LUA_FlipY(lua_State *L)
{
        L_numarg_range(L, 1, 2);
        Tile *t = L_arg_userdata(L, 1);
        valid_tile(L, t);
        
        if (lua_isnoneornil(L, 2)) {
                lua_pushboolean(L, (t->flags & TILE_FLIP_Y));
                return 1;
        }
        
        int was_flipped = (t->flags & TILE_FLIP_Y);
        if (L_arg_bool(L, 2)) {
                t->flags |= TILE_FLIP_Y;
                if (t->sprite_list != NULL && !was_flipped)
                        flip_sprites(t, 0, 1);
        } else {
                t->flags &= ~TILE_FLIP_Y;
                if (t->sprite_list != NULL && was_flipped)
                        flip_sprites(t, 0, 1);
        }
        
        lua_pushboolean(L, (t->flags & TILE_FLIP_Y));
        return 1;
}

/*
 * GetDepth(tile)
 */
static int
LUA_GetDepth(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        Tile *t = L_arg_userdata(L, 1);
        valid_tile(L, t);
        lua_pushnumber(L, t->depth);
        return 1;
}

/*
 * SetDepth(tile)
 */
static int
LUA_SetDepth(lua_State *L)
{
        L_numarg_range(L, 2, 2);
        Tile *t = L_arg_userdata(L, 1);
        valid_tile(L, t);
        float depth = L_arg_float(L, 2);
        
        t->depth = depth;
        return 0;
}

float fractal(float x, float y, float z, float s, int octaves);
static int
LUA_Fractal(lua_State *L)
{
        L_numarg_range(L, 5, 5);
	float x = L_arg_float(L, 1);
	float y = L_arg_float(L, 2);
	float z = L_arg_float(L, 3);
	float s = L_arg_float(L, 4);
	int octaves = L_arg_int(L, 5);
        lua_pushnumber(L, fractal(x, y, z, s, octaves));
	return 1;
}

/*
 * GetPos(obj, relativeTo=parent) -> {x=?, y=?}
 *
 * Return Body or Camera position relative to its parent. Optionally, the
 * second argument can be used to get position relative to some other Body,
 * Camera, or World.
 *
 *
 * GetPos(tile) -> {x=?, y=?}
 *
 * Get tile offset from parent Body.
 */
static int
LUA_GetPos(lua_State *L)
{
        L_numarg_range(L, 1, 2);
        void *obj = L_arg_userdata(L, 1);
        
        switch (*(int *)obj) {
        case OBJTYPE_BODY:
        case OBJTYPE_CAMERA: {
                void *relto = L_argdef_userdata(L, 2, NULL);
                if (relto == NULL) {
                        L_push_vectf(L, body_pos(get_body(L, obj)));
                        return 1;
                }
                vect_f bpos = body_absolute_pos(get_body(L, obj));
                vect_f other_pos = body_absolute_pos(get_body(L, relto));
                L_push_vectf(L, vect_f_sub(bpos, other_pos));
                return 1;
        }
        case OBJTYPE_TILE: {
                valid_tile(L, obj);
                L_push_vectf(L, tile_pos(obj));
                return 1;
        }
        }
        objtype_error(L, obj);
}

/*
 * Get body position from previous step.
 */
static int
LUA_GetPrevPos(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        void *obj = L_arg_userdata(L, 1);
        
        switch (*(int *)obj) {
        case OBJTYPE_BODY: {
                valid_body(L, obj);
                L_push_vectf(L, ((Body *)obj)->prevstep_pos);
                return 1;
        }
        case OBJTYPE_CAMERA: {
                valid_camera(L, obj);
                L_push_vectf(L, ((Camera *)obj)->body.prevstep_pos);
                return 1;
        }
        }
        objtype_error(L, obj);
}

/*
 * SetPos(object, pos)
 *
 * object       Accepted objects: Body, Camera, Tile, Shape.
 * pos          Position vector.
 *
 * Set position relative to parent.
 *
 * Shapes do not have an explicit position, so instead the given position is
 * added to all the shape's present values.
 */
static int
LUA_SetPos(lua_State *L)
{
        L_numarg_range(L, 2, 2);
        void *obj = L_arg_userdata(L, 1);
        vect_f pos = L_arg_vectf(L, 2);
        
        switch (*(int *)obj) {
        case OBJTYPE_BODY: {
                valid_body(L, obj);
                body_set_pos(obj, pos);
                return 0;
        }
        case OBJTYPE_CAMERA: {
                valid_camera(L, obj);
                cam_set_pos(obj, pos);
                return 0;
        }
        case OBJTYPE_TILE: {
                valid_tile(L, obj);                
                tile_set_pos(obj, pos);
                return 0;
        }
        case OBJTYPE_SHAPE: {
                valid_shape(L, obj);
                vect_i delta = (vect_i){posround(pos.x), posround(pos.y)};
                
                switch (((Shape *)obj)->shape_type) {
                case SHAPE_RECTANGLE: {
                        ShapeDef def = shape_def(obj);
                        def.rect.l += delta.x;
                        def.rect.r += delta.x;
                        def.rect.b += delta.y;
                        def.rect.t += delta.y;
                        shape_set_def(obj, def);
                        break;
                }
                case SHAPE_CIRCLE: {
                        ShapeDef def = shape_def(obj);
                        def.circle.offset.x += delta.x;
                        def.circle.offset.y += delta.y;
                        shape_set_def(obj, def);
                        break;
                }
                }
                return 0;
        }
        }
        objtype_error(L, obj);
}

/*
 * SetPosCentered(object, pos)
 */
static int
LUA_SetPosCentered(lua_State *L)
{
        L_numarg_range(L, 2, 2);
        void *obj = L_arg_userdata(L, 1);
        vect_f pos = L_arg_vectf(L, 2);
        
        SetPosCentered(obj, pos);
        return 0;
}

/*
 * SetPosX(obj, value)
 */
static int
LUA_SetPosX(lua_State *L)
{
        L_numarg_range(L, 2, 2);
        void *obj = L_arg_userdata(L, 1);
        float value = L_arg_float(L, 2);
        
        SetPosX(obj, value);
        return 0;
}

/*
 * SetPosY(obj, value)
 */
static int
LUA_SetPosY(lua_State *L)
{
        L_numarg_range(L, 2, 2);
        void *obj = L_arg_userdata(L, 1);
        float value = L_arg_float(L, 2);
        
        SetPosY(obj, value);
        return 0;
}

/*
 * AnimatePos(obj, type, toValue, duration, startTime=0)
 */
static int
LUA_AnimatePos(lua_State *L)
{
        L_numarg_range(L, 4, 5);
        void *obj = L_arg_userdata(L, 1);
        uint8_t type = L_arg_uint(L, 2);
        vect_f end = L_arg_vectf(L, 3);
        float duration = L_arg_float(L, 4);
        float start_time = L_argdef_float(L, 5, 0.0);
        
        switch (*(int *)obj) {
        case OBJTYPE_TILE: {
                valid_tile(L, obj);
                tile_anim_pos(obj, type, end, duration, start_time);
                return 0;
        }
        case OBJTYPE_CAMERA: {
                valid_camera(L, obj);
                body_anim_pos(&((Camera *)obj)->body, type, end, duration, start_time);
                return 0;
        }
        case OBJTYPE_BODY: {
                valid_body(L, obj);
                body_anim_pos(obj, type, end, duration, start_time);
                return 0;
        }
        }
        objtype_error(L, obj);
}

/*
 * Return the velocity of an object. Supported objects: Body, Camera.
 */
static int
LUA_GetVel(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        void *obj = L_arg_userdata(L, 1);
        
        switch (*(int *)obj) {
        case OBJTYPE_BODY: {
                valid_body(L, obj);
                L_push_vectf(L, ((Body *)obj)->vel);
                return 1;
        }
        case OBJTYPE_CAMERA: {
                valid_camera(L, obj);
                L_push_vectf(L, ((Camera *)obj)->body.vel);
                return 1;
        }
        }
        objtype_error(L, obj);
}

/*
 * SetVel(obj, vel)
 *
 * Set an object's velocity.
 *
 * obj          Body or Camera object.
 * vel          Floating point velocity vector.
 */
static int
LUA_SetVel(lua_State *L)
{
        L_numarg_range(L, 2, 2);
        void *obj = L_arg_userdata(L, 1);
        vect_f vel = L_arg_vectf(L, 2);
        
        switch (*(int *)obj) {
        case OBJTYPE_BODY: {
                Body *body = obj;
                valid_body(L, body);
                body->vel = vel;
                
                /* Set standard step function if not already set. */
                if (body->step_func == 0) {
                        body->step_func = (intptr_t)stepfunc_std;
                        body->flags |= BODY_STEP_C;
                }
                return 0;
        }
        case OBJTYPE_CAMERA: {
                Camera *cam = obj;
                valid_camera(L, cam);
                cam->body.vel = vel;
                
                /* Set standard step function if not already set. */
                if (cam->body.step_func == 0) {
                        cam->body.step_func = (intptr_t)stepfunc_std;
                        cam->body.flags |= BODY_STEP_C;
                }
                return 0;
        }
        }
        objtype_error(L, obj);
}

static int
LUA_SetVelX(lua_State *L)
{
        L_numarg_range(L, 2, 2);
        void *obj = L_arg_userdata(L, 1);
        float value = L_arg_float(L, 2);
        
        SetVel(obj, (vect_f){value, GetVel(obj).y});
        return 0;
}

static int
LUA_SetVelY(lua_State *L)
{
        L_numarg_range(L, 2, 2);
        void *obj = L_arg_userdata(L, 1);
        float value = L_arg_float(L, 2);
        
        SetVel(obj, (vect_f){GetVel(obj).x, value});
        return 0;
}

/*
 * Set an object's acceleration.
 *
 * obj          Body or Camera object pointer.
 * acc          Floating point acceleration vector.
 */
static int
LUA_SetAcc(lua_State *L)
{
        L_numarg_range(L, 2, 2);
        void *obj = L_arg_userdata(L, 1);
        vect_f acc = L_arg_vectf(L, 2);
        
        switch (*(int *)obj) {
        case OBJTYPE_BODY: {
                Body *body = obj;
                valid_body(L, body);
                body->acc = acc;
                
                /* Set standard step function if not already set. */
                if (body->step_func == 0) {
                        body->step_func = (intptr_t)stepfunc_std;
                        body->flags |= BODY_STEP_C;
                }
                return 0;
        }
        case OBJTYPE_CAMERA: {
                Camera *cam = obj;
                valid_camera(L, cam);
                cam->body.acc = acc;
                
                /* Set standard step function if not already set. */
                if (cam->body.step_func == 0) {
                        cam->body.step_func = (intptr_t)stepfunc_std;
                        cam->body.flags |= BODY_STEP_C;
                }
                return 0;
        }
        }
        objtype_error(L, obj);
}

/*
 * GetDeltaPos(object) -> {x=?, y=?}
 *
 * object       Body object pointer.
 *
 * Return the position delta that object moved from previous step to the current
 * step.
 */
static int
LUA_GetDeltaPos(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        void *obj = L_arg_userdata(L, 1);
        
        vect_f delta;
        switch (*(int *)obj) {
        case OBJTYPE_BODY: {
                Body *body = obj;
                vect_f bpos = body_pos(body);
#if !ALL_SMOOTH
                if (body->flags & BODY_SMOOTH_POS) {
                        delta = vect_f_sub(bpos, body->prevstep_pos);
                } else {
                        delta.x = posround(bpos.x) - posround(body->prevstep_pos.x);
                        delta.y = posround(bpos.y) - posround(body->prevstep_pos.y);
                }
#else
                delta = vect_f_sub(bpos, body->prevstep_pos);
#endif  /* ALL_SMOOTH */
                break;
        }
        default:
                objtype_error(L, obj);
        }
        L_push_vectf(L, delta);
        return 1;
}

/*
 * GetSize(something) -> {x=?, y=?}
 *
 * Get the size of something. To get the size of a texture, pass in the same
 * kind of argument you would give to NewSpriteList().
 */
static int
LUA_GetSize(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        void *obj = L_arg_userdata(L, 1);
        
        switch (*(int *)obj) {
        case OBJTYPE_CAMERA: {
                Camera *cam = obj;
                valid_camera(L, cam);
                
                L_push_vecti(L, (vect_i){
                        .x=(int)lroundf(cam->size.x / cam->zoom),
                        .y=(int)lroundf(cam->size.y / cam->zoom)
                });
                return 1;
        }
        case OBJTYPE_TILE: {
                Tile *t = obj;
                valid_tile(L, t);
                
                vect_f size = tile_size(t);
                if (size.x < 0) {
                        size.x = -size.x;
                        size.y = -size.y;
                }
                assert(size.x > 0 && size.y > 0);
                L_push_vectf(L, size);
                return 1;
        }
        case OBJTYPE_SPRITELIST: {
                SpriteList *sprite_list = obj;
                valid_spritelist(L, sprite_list);
                vect_i size = texfrag_maxsize(sprite_list->frames,
                                              sprite_list->num_frames);
                L_push_vectf(L, (vect_f){size.x, size.y});
                return 1;
        }
        }
        objtype_error(L, obj);
}

/*
 * SetSize(obj, {x=?, y=?})
 *
 * Change size of an object (Tile).
 */
static int
LUA_SetSize(lua_State *L)
{
        L_numarg_range(L, 2, 2);
        void *obj = L_arg_userdata(L, 1);
        vect_f size = L_arg_vectf(L, 2);
        
        switch (*(int *)obj) {
        case OBJTYPE_TILE: {
                valid_tile(L, obj);
                tile_set_size(obj, size);
                return 0;
        }
        }
        objtype_error(L, obj);
}

/*
 * AnimateSize(tile, type, targetSize, duration, startTime)
 */
static int
LUA_AnimateSize(lua_State *L)
{
        L_numarg_range(L, 4, 5);
        Tile *t = L_arg_userdata(L, 1);
        valid_tile(L, t);
        uint8_t type = L_arg_uint(L, 2);
        vect_f end = L_arg_vectf(L, 3);
        float duration = L_arg_float(L, 4);
        float start_time = L_argdef_float(L, 5, 0.0);
        
        tile_anim_size(t, type, end, duration, start_time);
        return 0;
}

/*
 * GetColor(tile)
 */
static int
LUA_GetColor(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        Tile *t = L_arg_userdata(L, 1);
        
        uint32_t c = tile_color(t);
        L_push_color(L, c);
        
        return 1;
}

/*
 * SetColor(obj, color)
 *
 * Change tile color, or the background color of camera view.
 *
 * obj          Tile or Camera object.
 * c            {r=?, g=?, b=?, a=?}. Alpha is optional and defaults to 1.
 */
static int
LUA_SetColor(lua_State *L)
{
        L_numarg_range(L, 2, 2);
        void *obj = L_arg_userdata(L, 1);
        uint32_t c = L_arg_color(L, 2);
        
        switch (*(int *)obj) {
        case OBJTYPE_TILE: {
                valid_tile(L, obj);
                tile_set_color(obj, c);
                return 0;
        }
        case OBJTYPE_CAMERA: {
                valid_camera(L, obj);
                cam_set_color(obj, c);
                return 0;
        }
        }
        objtype_error(L, obj);
}

/*
 * AnimateColor(obj, type, endValue, duration, startTime=0)
 */
static int
LUA_AnimateColor(lua_State *L)
{
        L_numarg_range(L, 4, 5);
        void *obj = L_arg_userdata(L, 1);
        uint8_t type = L_arg_uint(L, 2);
        uint32_t end = L_arg_color(L, 3);
        float duration = L_arg_float(L, 4);
        float start_time = L_argdef_float(L, 5, 0.0);

        switch (*(int *)obj) {
        case OBJTYPE_TILE: {
                valid_tile(L, obj);
                tile_anim_color(obj, type, end, duration, start_time);
                return 0;
        }
        case OBJTYPE_CAMERA: {
                valid_camera(L, obj);
                cam_anim_color(obj, type, end, duration, start_time);
                return 0;
        }
        }
        objtype_error(L, obj);
}

/*
 * AnimatingColor(obj)
 *
 * Return true if there is an ongoing color animation.
 */
static int
LUA_AnimatingColor(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        void *obj = L_arg_userdata(L, 1);
        
        switch (*(int *)obj) {
        case OBJTYPE_TILE: {
                valid_tile(L, obj);
                Tile *tile = obj;
                Property *prop = tile->color;
                lua_pushboolean(L, prop != NULL && prop->anim_type != ANIM_NONE);
                return 1;
        }
        }
        objtype_error(L, obj);
}

/*
 * Blend(tile, mode)
 *
 * tile         Tile as returned by NewTile() whose blending mode will be
 *              affected. If `false` then the default blending mode of all
 *              tiles will be changed.
 * mode         One of blending mode names:
 *                      "default"       (whatever is set as present default)
 *                      "source"        (standard blending -- use source alpha)
 *                      "multiply"
 *                      "mask"          (color affects destination alpha)
 *                      "destination"   (use destination alpha)
 *                      "add"           (source color added to destination)
 *                      "alpha"         (put alpha as source color)
 */
static int
LUA_Blend(lua_State *L)
{
        L_numarg_range(L, 2, 2);
        const char *modename = L_arg_cstr(L, 2);
        
        unsigned mode;
        if (strcmp(modename, "default") == 0)
                mode = TILE_BLEND_DEFAULT;
        else if (strcmp(modename, "source") == 0)
                mode = TILE_BLEND_SOURCE;
        else if (strcmp(modename, "multiply") == 0)
                mode = TILE_BLEND_MULTIPLY;
        else if (strcmp(modename, "mask") == 0)
                mode = TILE_BLEND_MASK;
        else if (strcmp(modename, "destination") == 0)
                mode = TILE_BLEND_DESTINATION;
        else if (strcmp(modename, "add") == 0)
                mode = TILE_BLEND_ADD;
        else if (strcmp(modename, "alpha") == 0)
                mode = TILE_BLEND_ALPHA;
        else
                luaL_error(L, "Invalid blending mode name: `%s`.", modename);
        
        /* Check if we have to change the default blending mode. */
        if (lua_isboolean(L, 1)) {
                extern unsigned blend_mode_default;
                info_assert(L, mode != TILE_BLEND_DEFAULT, "'default' is not a "
                            "valid default blending mode.");
                blend_mode_default = mode;
                return 0;
        }
        
        /* Set tile blending mode. */
        Tile *t = L_arg_userdata(L, 1);
        valid_tile(L, t);
        t->flags = (t->flags & ~TILE_BLEND) | mode;
        return 0;
}

/*
 * Link(child, parent)
 *
 * Attach child body to parent body.
 *
 * child, parent        Body objects as returned by NewBody().
 */
static int
LUA_Link(lua_State *L)
{
        L_numarg_range(L, 2, 2);
        Body *child = L_arg_userdata(L, 1);
        Body *parent = L_arg_userdata(L, 2);

        valid_body(L, child);
        valid_body(L, parent);
        if (child->parent == parent)
                return 0;       /* Already linked. */
        
        /* Update child position so that it is relative to parent now. */
        vect_f parent_pos = body_absolute_pos(parent);
        vect_f child_pos = body_absolute_pos(child);
        vect_f diff = vect_f_sub(child_pos, parent_pos);
        SetPos(child, diff);
        
        /* Remove child from current parent. */
        if (child->parent != NULL)
                DL_DELETE(child->parent->children, child);
                
        child->parent = parent;                 /* Set new parent. */
        DL_PREPEND(parent->children, child);    /* Add as new parent's child. */
        return 0;
}

/*
 * Unlink(body)
 *
 * Detach body from its parent and add it to the world's static body.
 */
static int
LUA_Unlink(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        Body *child = L_arg_userdata(L, 1);
        valid_body(L, child);
        
        Body *parent = child->parent;
        Body *static_body = &child->world->static_body;
        info_assert(L, parent, "Body has no parent.");
        if (parent == static_body) {
                log_warn("Unlink() has no effect: body already attached to "
                         "world directly.");
                return 0;
        }
        
        vect_f abs_pos = body_absolute_pos(child); /* Position within world. */
        DL_DELETE(parent->children, child);        /* Remove from parent. */
        
        /* Add as child to static body. */
        child->parent = static_body;
        DL_PREPEND(static_body->children, child);
        SetPos(child, abs_pos);
        return 0;
}

/*
 * GetChildren(object) -> object
 *
 * Get linked children. The return value is an array (table with numeric
 * indices) of pointers to bodies that have been previously linked using the
 * Link() function.
 */
static int
LUA_GetChildren(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        Body *body = L_arg_userdata(L, 1);

        /* Create and return an array of lightuserdata pointers to children. */
        valid_body(L, body);
        lua_newtable(L);
        int key = 1;
        Body *child;
        DL_FOREACH(body->children, child) {
                lua_pushinteger(L, key++);
                lua_pushlightuserdata(L, child);
                lua_rawset(L, -3);
        }
        return 1;
}

static lua_State *L_state;

static void
clear_timer_state(Timer *t)
{
        lua_State *L = L_state;
        
        lua_getglobal(L, "eapi");
        L_get_strfield(L, -1, "idToObjectMap");
        
        /* Unset timer function reference. */
        info_assert(L, t->func != 0, "No function.");
        lua_pushnumber(L, t->func);
        lua_pushnil(L);
        lua_rawset(L, -3);
        
        /* Unset user argument reference. */
        if (t->data != 0) {
                lua_pushnumber(L, t->data);
                lua_pushnil(L);
                lua_rawset(L, -3);
        }
        
        lua_pop(L, 2);
}

/*
 * AddTimer(obj, when, funcID, argID)
 *
 * obj          Accepted objects: World, Camera, Body.
 * when         When to execute the timer (seconds since present time).
 * functionID   Function index into `eapi.idToObjectMap`.
 * argID        User argument index into `eapi.idToObjectMap`.
 *
 * Create a timer that is bound to given body. If that body is paused, then the
 * timer will not be executed until the body is resumed.
 */
static int
LUA_AddTimer(lua_State *L)
{
        L_numarg_range(L, 4, 4);
        void *obj = L_arg_userdata(L, 1);
        float when = L_arg_float(L, 2);
        intptr_t func = L_arg_int(L, 3);
        intptr_t argID = L_arg_int(L, 4);
        
        /* Add timer to body. */
        info_assert(L, when >= 0.0, "Negative time offset.");
        Body *b = get_body(L, obj);
        Timer *tmr = body_add_timer(b, obj, when, OBJTYPE_TIMER_LUA, func,
                                    argID);
        
        /* Set cleanup function to avoid memory leaks in script. */
        tmr->clearfunc = clear_timer_state;
        
        /* Set timer pointer. */
        lua_createtable(L, 4, 0);
        lua_pushinteger(L, 1);
        lua_pushlightuserdata(L, tmr);
        lua_rawset(L, -3);
        
        /* Set timer ID. */
        lua_pushinteger(L, 2);
        lua_pushinteger(L, tmr->timer_id);
        lua_rawset(L, -3);

        return 1;
}

/*
 * CancelTimer(timer)
 *
 * Stop timer from being executed.
 *
 * timer        Timer table as returned by AddTimer(). Silently ignores `nil`
 *              for script convenience.
 */
static int
LUA_CancelTimer(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        
        /* Ignore if argument is nil. */
        if (lua_isnil(L, 1))
                return 0;
        
        /* Get timer pointer. */
        L_get_intfield(L, 1, 1);
        Timer *timer = L_arg_userdata(L, 2);
        
        /* Get timer ID. */
        L_get_intfield(L, 1, 2);
        unsigned timer_id = L_arg_uint(L, 3);
        
        /* Ignore if timer was destroyed (check if mem is reused). */
        void *owner = timer->owner;
        if (owner == NULL || timer->timer_id != timer_id)
                return 0;
        
        switch (*(int *)owner) {
        case OBJTYPE_BODY: {
                valid_body(L, owner);
                body_cancel_timer(owner, timer);
                break;
        }
        case OBJTYPE_WORLD: {
                World *world = owner;
                valid_world(L, world);
                body_cancel_timer(&world->static_body, timer);
                break;
        }
        case OBJTYPE_CAMERA: {
                Camera *cam = owner;
                valid_camera(L, cam);
                body_cancel_timer(&cam->body, timer);
                break;
        }
        default:
                objtype_error(L, owner);
        }
        return 0;
}

/*
 * Dump(shape) -> Lua code
 *
 * Produce Lua code that creates the argument shape.
 */
static int
LUA_Dump(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        Shape *s = L_arg_userdata(L, 1);
        valid_shape(L, s);
        Body *body = s->body;
        
        UT_string dump, tmp;
        utstring_init(&dump);
        utstring_init(&tmp);
        utstring_printf(&dump, "eapi.NewShape(");
        utstring_printf(&dump, body == &body->world->static_body ? "staticBody," : "?,");
        utstring_printf(&dump, "nil,");
        ShapeDef def = shape_def(s);
        switch (s->shape_type) {
        case SHAPE_CIRCLE: {
                vect_i v = def.circle.offset;
                utstring_printf(&tmp, "{{%d, %d}, %d},", v.x, v.y, def.circle.radius);
                utstring_concat(&dump, &tmp);
                break;
        }
        case SHAPE_RECTANGLE: {
                BB bb = def.rect;
                utstring_printf(&tmp, "{l=%d,r=%d,b=%d,t=%d},", bb.l, bb.r, bb.b, bb.t);
                utstring_concat(&dump, &tmp);
                break;
        }
        default:
                luaL_error(L, "Unknown shape type: %d.", s->shape_type);
        }
        
        /* Append group name. */
        utstring_printf(&dump, "\"");
        utstring_printf(&dump, s->group->name);
        utstring_printf(&dump, "\")");

        lua_pushstring(L, utstring_body(&dump));
        utstring_done(&dump);
        utstring_done(&tmp);
        return 1;
}

/*
 * SelectShape(world, point, groupName=nil) -> shape
 *
 * world        Game world as returned by NewWorld().
 * point        World position vector.
 * group        Ignore all shapes that do not belong to this group.
 *
 * Select the first shape that covers provided point. If none do, return nil.
 */
static int
LUA_SelectShape(lua_State *L)
{
        L_numarg_range(L, 2, 3);
        World *world = L_arg_userdata(L, 1);
        vect_i point = L_arg_vecti(L, 2);
        const char *group_name = L_argdef_cstr(L, 3, NULL);
        
        Shape *s = NULL;
        SelectShapes(world, point, group_name, &s, 1);
        
        if (s == NULL)
                lua_pushnil(L);
        else
                lua_pushlightuserdata(L, s);
        return 1;
}

#if ENABLE_TILE_GRID

/*
 * SelectTiles(world, area, body=all)
 */
static int
LUA_SelectTiles(lua_State *L)
{
        L_numarg_range(L, 2, 3);
        World *world = L_arg_userdata(L, 1);
        valid_world(L, world);
        BB area = L_arg_BB(L, 2);
        Body *body = L_argdef_userdata(L, 3, NULL);
                
        /* Look up nearby tiles from grid. */
        void *nearby[400];
        unsigned num_nearby = grid_lookup(&world->grid, area, nearby,
                                          ARRAYSZ(nearby), tile_filter);
        /*
         * Go over the lookup tiles. If we find one which really falls inside
         * given area (and belongs to requested body), push it as a result.
         */
        lua_newtable(L);
        int num_tiles = 0;
        for (unsigned i = 0; i < num_nearby; i++) {
                Tile *t = nearby[i];
                valid_tile(L, t);
                if (body != NULL && t->body != body)
                        continue;       /* Does not belong to requested body. */
                
                /* Add to result list if there's overlap with given area. */
                vect_f sz = GetSize(t);
                vect_f pos = GetPos(t);
                BB tile_bb = {
                        .l=roundf(pos.x), .r=roundf(pos.x + sz.x),
                        .b=roundf(pos.y), .t=roundf(pos.y + sz.y)
                };
                if (bb_overlap(&tile_bb, &area)) {
                        lua_pushnumber(L, ++num_tiles);
                        lua_pushlightuserdata(L, t);
                        lua_rawset(L, -3);
                }
        }
        return 1;
}

#endif  /* ENABLE_TILE_GRID */

/*
 * Render to named texture.
 *
 * name         Image name. This follows the same format as NewSpriteList()
 *              `image` argument.
 */
static int
LUA_DrawToTexture(lua_State *L)
{
        L_numarg_range(L, 1, 1);

        /* Get texture name and filter setting. */
        unsigned flags;
        char name[128];
        get_texture_spec(L, 1, name, sizeof(name), &flags);
        
        /* Set texture size = screen size. */
        Texture *tex = texture_load_blank(name, flags);
        texture_set_size(tex, config.screen_width, config.screen_height);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tex->pow_w, tex->pow_h, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, NULL);
        if (glGenerateMipmap != NULL && (flags & TEXFLAG_FILTER))
                glGenerateMipmap(GL_TEXTURE_2D);
                
        /* If texture->framebuffer support, use it. Otherwise glReadPixels(). */
        if (glGenFramebuffers != NULL) {
                /* Create and bind framebuffer. */
                GLuint framebuf;
                glGenFramebuffers(1, &framebuf);
                glBindFramebuffer(GL_FRAMEBUFFER_EXT, framebuf);
                assert(tex->id);
                glFramebufferTexture2D(GL_FRAMEBUFFER_EXT,
                                       GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D,
                                       tex->id, 0);
                
                /* Draw everything as usual. */
                extern Camera *cam_list;
                glClear(GL_COLOR_BUFFER_BIT);
                for (Camera *cam = cam_list; cam != NULL; cam = cam->next) {
                        if (!cam->disabled)
                                render_to_framebuffer(cam);
                }
                
                /* Dispose of framebuffer. */
                glDeleteFramebuffers(1, &framebuf);

                /* Restore main framebuffer. */
                extern GLuint main_framebuffer;
                glBindFramebuffer(GL_FRAMEBUFFER_EXT, main_framebuffer);
                return 0;
        }
        
        /* Read framebuffer into buffer then write into texture. */
        static unsigned char *pixels, *flipped;
        unsigned rowsz = tex->w * 3;
        if (pixels == NULL) {
                pixels = mem_alloc(rowsz * tex->h, "glReadPixels buffer");
                flipped = mem_alloc(rowsz * tex->h, "glReadPixels flipped");
        }
        glReadPixels(0, 0, tex->w, tex->h, GL_RGB, GL_UNSIGNED_BYTE, pixels);
        
        /* Flip texture vertically. */
        unsigned char *ptr = pixels;
        for (int i = tex->h - 1; i >= 0; --i) {
                memcpy(flipped + i * rowsz, ptr, rowsz);
                ptr += rowsz;
        }
        
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tex->w, tex->h, GL_RGB,
                        GL_UNSIGNED_BYTE, flipped);
        return 0;
}

/*
 * Collide(world, groupNameA, groupNameB, funcID, update=false, priority=0, argID=0)
 *
 * Register a collision handler function.
 *
 * world        World as returned by NewWorld().
 * groupname_a  groupname_a and groupname_a name the two shape collision groups
 *              that, after invoking this function with their names, are
 *              going to be considered for collisions.
 * groupname_b  see groupname_a.
 * cf           Collision function pointer. If NULL, previously set collision
 *              handler is removed.
 *              There can be only one collision handler per each pair of groups,
 *              so if a handler has already been registered for a particular
 *              pair, it is going to be overwritten with the new one.
 * update       If true, collision events are received continuously (each step).
 *              If false, collision function is called when collision between
 *              two shapes starts (shapes touch after being separate), and then
 *              when it ends ("separation" callback).
 * priority     Priority determines the order in which collision handlers are
 *              called for a particular pair of shapes.
 *              So let's say we have registered handlers for these two pairs:
 *                      "Player" vs "Ground"
 *                      "Player" vs "Exit".
 *              Now a shape belonging to group "Player" can be simultaneously
 *              colliding with both a "Ground" shape and an "Exit" shape. If
 *              priorities are the same for both handlers, then the order in
 *              which they will be executed is undetermined. We can, however,
 *              set the priority of "Player" vs "Ground" handler to be higher,
 *              so that it would always be executed first.
 * argID        User argument index.
 *
 * Collision function C prototype:
 *      int (*CollisionFunc)(Shape *A, Shape *B, int new_collision, BB *resolve, intptr_t data);
 *
 *      A, B                    Shapes involved in collision. For separation
 *                              callback these pointers may be NULL in case a
 *                              shape has been destroyed.
 *      new_collision           True if this is the first time shapes touch
 *                              after being separate.
 *      resolve                 Pointer to four values indicating distance in
 *                              each of the four directions (left, right,
 *                              bottom, top) that shape A can be moved so that
 *                              it would not intersect B.
 *                              If this is a separation callback, then resolve
 *                              is NULL.
 *      data                    User callback data.
 *
 *      If user callback returns nonzero value, then further callbacks for this
 *      particular pair of shapes are not invoked anymore until they separate
 *      and touch again.
 */
static int
LUA_Collide(lua_State *L)
{
        L_numarg_range(L, 4, 7);
        World *w = L_arg_userdata(L, 1);
        const char *groupname_a = L_arg_cstr(L, 2);
        const char *groupname_b = L_arg_cstr(L, 3);
        unsigned cf = L_argdef_uint(L, 4, 0);
        int update = L_argdef_bool(L, 5, 0);
        int priority = L_argdef_int(L, 6, 0);
        intptr_t argID = L_argdef_int(L, 7, 0);
        assert(cf || (!update && !priority));
        
        /* Create group A if it doesn't exist. */
        Group *groupA;
        HASH_FIND_STR(w->groups, groupname_a, groupA);
        if (groupA == NULL) {
                extern mem_pool mp_group;
                groupA = mp_alloc(&mp_group);
                
                info_assert_va(L, strlen(groupname_a) < sizeof(groupA->name),
                               "Shape group name length exceeds maximum value "
                               "(%d).", sizeof(groupA->name));
                strcpy(groupA->name, groupname_a);
                
                groupA->index = w->next_group_id++;
                assert(groupA->index < SHAPEGROUPS_MAX);
                HASH_ADD_STR(w->groups, name, groupA);
        }
        
        /* Create group B if it doesn't exist. */
        Group *groupB;
        HASH_FIND_STR(w->groups, groupname_b, groupB);
        if (groupB == NULL) {
                extern mem_pool mp_group;
                groupB = mp_alloc(&mp_group);
                
                info_assert_va(L, strlen(groupname_b) < sizeof(groupB->name),
                               "Shape group name length exceeds maximum value "
                               "(%d).", sizeof(groupB->name));
                strcpy(groupB->name, groupname_b);
                
                groupB->index = w->next_group_id++;
                assert(groupB->index < SHAPEGROUPS_MAX);
                HASH_ADD_STR(w->groups, name, groupB);
        }
        
        /* Get handler structure pointer from `handler_map` array. */
        Handler *handler = &w->handler_map[groupA->index][groupB->index];
        
        /* Each group remembers how many handlers we have registered for it. */
        if (handler->func == 0 && cf != 0)
                groupA->num_handlers++;
        else if (handler->func != 0 && cf == 0)
                groupA->num_handlers--;
        
        /* Set handler data. */
        handler->type = HANDLER_LUA;
        handler->func = cf;
        handler->update = update;
        handler->priority = priority;
        handler->data = argID;
        
        return 0;
}

/*
 * GetGroup(shape) -> groupName
 */
static int
LUA_GetGroup(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        Shape *s = L_arg_userdata(L, 1);
        valid_shape(L, s);
        lua_pushstring(L, s->group->name);
        return 1;
}

/*
 * Destroy(obj)
 *
 * obj          Accepted objects: Body, Shape, Tile, World.
 *
 * Free any resources owned by objects. Passing an object into API routines
 * after it has been destroyed will result in assertion failures saying it is of
 * incorrect type. It is also possible (though unlikely) that its memory has
 * been reused by some other object in which case the other object will be
 * affected.
 */
static int
LUA_Destroy(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        void *obj = L_arg_userdata(L, 1);
        
        switch (*(int *)obj) {
        case OBJTYPE_BODY: {
                valid_body(L, obj);
                body_free(obj);
                return 0;
        }
        case OBJTYPE_SHAPE: {
                valid_shape(L, obj);
                shape_free(obj);
                return 0;
        }
        case OBJTYPE_TILE: {
                valid_tile(L, obj);
                tile_free(obj);
                return 0;
        }
        case OBJTYPE_WORLD: {
                valid_world(L, obj);
#if ENABLE_AUDIO
                /* Fade out all sounds bound to this world. */
                audio_fadeout_group((uintptr_t)obj, 1000);
#endif
                /*
                 * Destroy resources owned by world and schedule for complete
                 * destruction.
                 */
                world_kill(obj);
                return 0;
        }
        }
        objtype_error(L, obj);
        return 0;
}

/*
 * Clear()
 *
 * Clear engine state.
 */
static int
LUA_Clear(lua_State *L)
{
        UNUSED(L);
        L_numarg_range(L, 0, 0);
        Clear();
        return 0;
}

/*
 * Quit()
 *
 * Terminate program.
 */
static int
LUA_Quit(lua_State *L)
{
        UNUSED(L);
        L_numarg_range(L, 0, 0);
        log_msg("User script called Quit()");
        exit(EXIT_SUCCESS);
}

#if ENABLE_AUDIO

/*
 * PlaySound(world, filename, repeat=0, volume=1, fadeInTime=0) -> sound
 *
 * world        World as returned by NewWorld(). This is necessary because when
 *              a world is paused, we also must pause all sounds bound to it. If
 *              false is used here, then the sound does not belong to any world.
 * filename     Sound filename (e.g., "script/click.ogg").
 * repeat       How many extra times the sound should be played. Zero (or `nil`)
 *              means the sound will be played only once. -1 will play it
 *              forever (until some other condition stops it).
 * volume       Number in the range [0..1].
 * fadeInTime   The time it takes for sound to go from zero volume to full
 *              volume.
 *
 * Play a sound. Returns a sound handle which can be used to stop the sound or
 * change its volume, add effects, etc.
 */
static int
LUA_PlaySound(lua_State *L)
{
        L_numarg_range(L, 2, 5);
        const char *filename = L_arg_cstr(L, 2);
        int loops = L_argdef_int(L, 3, 0);
        info_assert_va(L, loops >= -1, "Invalid number of loops (%d).", loops);
        int volume = round(L_argdef_float(L, 4, 1.0) * MIX_MAX_VOLUME);
        info_assert_va(L, volume >= 0 && volume <= MIX_MAX_VOLUME,
                       "Volume out of range.", volume);
        int fade_in = round(L_argdef_float(L, 5, 0.0) * 1000.0);
        info_assert(L, fade_in >= 0, "Invalid fade-in time.");
        
        /* World or no world. */
        World *world = NULL;
        if (lua_islightuserdata(L, 1)) {
                world = lua_touserdata(L, 1);
                valid_world(L, world);
        } else {
                info_assert(L, lua_isboolean(L, 1) && !lua_toboolean(L, 1),
                            "First argument should be either either world or "
                            "false.");
        }
                        
        /* Start playing. Use world pointer as group number. */
        int channel;
        unsigned sound_id;
        audio_play(filename, (uintptr_t)world, volume, loops, fade_in,
                   &sound_id, &channel);
        
        /* Return sound handle = {soundID=sound_id, channel=ch}. */
        lua_createtable(L, 0, 2);
        lua_pushstring(L, "soundID");
        lua_pushnumber(L, sound_id);
        lua_rawset(L, -3);
        lua_pushstring(L, "channel");
        lua_pushnumber(L, channel);
        lua_rawset(L, -3);
        return 1;
}

static void
extract_from_sound_handle(lua_State *L, int index, unsigned *sound_id,
                          int *channel)
{
        /* Extract sound ID and channel. */
        L_get_strfield(L, index, "soundID");
        *sound_id = lua_tonumber(L, -1);
        L_get_strfield(L, index, "channel");
        *channel = lua_tonumber(L, -1);
        info_assert_va(L, *sound_id > 0, "Invalid sound ID (%d).", *sound_id);
        info_assert_va(L, *channel >= 0, "Invalid channel (%d).", *channel);
}

/*
 * FadeSound(sound, time)
 *
 * sound        Sound handle as returned by PlaySound().
 *              You may also pass a world here instead of a particual sound. In
 *              this case all sounds belonging to that world will be faded out.
 * time         Fade out time in seconds.
 */
static int
LUA_FadeSound(lua_State *L)
{
        L_numarg_range(L, 2, 2);
        
        /* Extract fade-out time and convert to milliseconds. */
        int fade_time = round(L_arg_float(L, 2) * 1000.0);
        info_assert(L, fade_time >= 0, "Fade out time must not be negative.");

        switch (lua_type(L, 1)) {
        case LUA_TTABLE: {
                unsigned sound_id;
                int channel;
                extract_from_sound_handle(L, 1, &sound_id, &channel);
                if (fade_time > 0)
                        audio_fadeout(channel, sound_id, fade_time);
                else
                        audio_stop(channel, sound_id);
                return 0;
        }
        case LUA_TLIGHTUSERDATA: {
                World *world = L_arg_userdata(L, 1);
                valid_world(L, world);
                
                if (fade_time > 0)
                        audio_fadeout_group((uintptr_t)world, fade_time);
                else
                        audio_stop_group((uintptr_t)world);
                return 0;
        }
        }
        return luaL_error(L, "Invalid argument type (%s). Either sound "
                          "handle or world expected.",
                          lua_typename(L, lua_type(L, 1)));
}

/*
 * SetVolume(sound, volume)
 *
 * sound        Sound handle as returned by PlaySound().
 *              You may also pass a world here instead of a particual sound. In
 *              this case all sounds belonging to that world will be stopped.
 * volume       Volume must be in range [0..1].
 */
static int
LUA_SetVolume(lua_State *L)
{
        L_numarg_range(L, 2, 2);
        
        int volume = (int)lroundf(L_arg_float(L, 2) * MIX_MAX_VOLUME);
        info_assert(L, volume >= 0 && volume <= MIX_MAX_VOLUME, "Volume out of "
            "range.");
        
        switch (lua_type(L, 1)) {
        case LUA_TTABLE: {
                unsigned sound_id;
                int channel;
                extract_from_sound_handle(L, 1, &sound_id, &channel);
                audio_set_volume(channel, sound_id, volume);
                return 0;
        }
        case LUA_TLIGHTUSERDATA: {
                World *world = L_arg_userdata(L, 1);
                valid_world(L, world);
                audio_set_group_volume((uintptr_t)world, volume);
                return 0;
        }
        }
        return luaL_error(L, "Invalid argument type (%s). Either sound "
                          "handle or world expected.",
                          lua_typename(L, lua_type(L, 1)));
}

/*
 * BindVolume(sound, source, listener, distMaxVolume, distSilence)
 *
 * sound                Sound handle as returned by PlaySound().
 * source               Object that is producing the sound.
 *                      Accepted types: Body and Camera.
 * listener             Object that "hears" the sound.
 *                      Accepted types: Body and Camera.
 * distMaxVolume        When listener body gets this close to source (or
 *                      closer), then sound is played at max volume.
 * distSilence          When listener body is this far from source (or further),
 *                      then sound volume drops off to zero.
 */
static int
LUA_BindVolume(lua_State *L)
{
        L_numarg_range(L, 5, 5);
        luaL_checktype(L, 1, LUA_TTABLE);
        luaL_checktype(L, 2, LUA_TLIGHTUSERDATA);
        luaL_checktype(L, 3, LUA_TLIGHTUSERDATA);
        luaL_checktype(L, 4, LUA_TNUMBER);
        luaL_checktype(L, 5, LUA_TNUMBER);
        
        /* Extract source body. */
        Body *source;
        void *obj = L_arg_userdata(L, 2);
        switch (*(int *)obj) {
        case OBJTYPE_BODY:
                source = (Body *)obj;
                break;
        case OBJTYPE_CAMERA:
                source = &((Camera *)obj)->body;
                break;
        default:
                objtype_error(L, obj);
        }
        valid_body(L, source);
        
        /* Extract listener body. */
        Body *listener;
        obj = L_arg_userdata(L, 3);
        switch (*(int *)obj) {
        case OBJTYPE_BODY:
                listener = (Body *)obj;
                break;
        case OBJTYPE_CAMERA:
                listener = &((Camera *)obj)->body;
                break;
        default:
                objtype_error(L, obj);
        }
        valid_body(L, listener);
        
        /* Extract and verify distances. */
        float dist_maxvol = lua_tonumber(L, 4);
        float dist_silence = lua_tonumber(L, 5);
        info_assert(L, dist_maxvol >= 0.0, "Max volume distance must not be "
                 "negative.");
        info_assert(L, dist_silence > dist_maxvol, "Silence distance must be "
                 "greater than max volume distance");
        
        unsigned sound_id;
        int channel;
        extract_from_sound_handle(L, 1, &sound_id, &channel);
        audio_bind_volume(channel, sound_id, source, listener, dist_maxvol,
                          dist_silence);
        return 0;
}

/*
 * PlayMusic(filename, loops=nil, volume=1, fadeInTime=0, position=0)
 *
 * filename     Music filename (e.g., "script/morning.mp3").
 * loops        How many times the music should be played. `nil` means it
 *              will loop forever (until some other condition stops it).
 * volume       Number in the range [0..1].
 * fadeInTime   The time it takes for sound to go from zero volume to full
 *              volume.
 * position     Jump to `position` seconds from beginning of song.
 *
 * Play music.
 */
static int
LUA_PlayMusic(lua_State *L)
{
        L_numarg_range(L, 1, 5);
        const char *filename = L_arg_cstr(L, 1);
        int loops = L_argdef_uint(L, 2, 0);
        float volume = L_argdef_float(L, 3, 1.0);
        info_assert_va(L, volume >= 0.0 && volume <= 1.0,
                       "Volume (%f) must fall within range [0..1]", volume);
        float fadeInTime = L_argdef_float(L, 4, 0.0);
        info_assert_va(L, fadeInTime >= 0.0, "Fade-in time (%f) must be positive.", fadeInTime);
        float position = L_argdef_float(L, 5, 0.0);

        int ivol = round(volume * MIX_MAX_VOLUME);
        int fade_in = round(fadeInTime * 1000.0);
        audio_music_play(filename, ivol, loops, fade_in, position);
        return 0;
}

/*
 * FadeMusic(seconds)
 *
 * Stop music by fading it out.
 */
static int
LUA_FadeMusic(lua_State *L)
{
        L_numarg_range(L, 1, 1);
        
        /* Extract fade-out time and convert to milliseconds. */
        int fade_time = round(L_arg_float(L, 1) * 1000.0);
        info_assert(L, fade_time >= 0, "Fade out time must not be negative.");
        
        audio_music_fadeout(fade_time);
        return 0;
}

/*
 * SetMusicVolume(volume, transitionTime=0, delay=0)
 */
static int
LUA_SetMusicVolume(lua_State *L)
{
        L_numarg_range(L, 1, 3);        
        int volume = round(L_arg_float(L, 1) * MIX_MAX_VOLUME);
        float time = L_argdef_float(L, 2, 0.0);
        float delay = L_argdef_float(L, 3, 0.0);
        
        info_assert(L, volume >= 0 && volume <= MIX_MAX_VOLUME, "Volume out of range.");
        int time_ms = round(time * 1000.0);
        int delay_ms = round(delay * 1000.0);
                
        audio_music_set_volume(volume, time_ms, delay_ms);
        return 0;
}

static int
LUA_PauseMusic(lua_State *L)
{
        UNUSED(L);
        L_numarg_range(L, 0, 0);
        audio_music_pause();
        return 0;
}

static int
LUA_ResumeMusic(lua_State *L)
{
        UNUSED(L);
        L_numarg_range(L, 0, 0);
        audio_music_resume();
        return 0;
}

#endif

static long (*random_func)(void) = rand_eglibc;

/*
 * RandomSeed(seed, system)
 *
 * seed         Unsigned integer that seeds the random number generator.
 * system       If true, standard C library random() function is used as random
 *              number generator.
 *
 * Seed random number generator (see Random function).
 */
static int
LUA_RandomSeed(lua_State *L)
{
        L_numarg_range(L, 1, 2);
        unsigned seed = L_arg_uint(L, 1);
        int system = L_argdef_bool(L, 2, 0);
        
        if (system) {
                random_func = random;
                srandom(seed);
        } else {
                random_func = rand_eglibc;
                srand_eglibc(seed);
        }
        return 0;
}

/*
 * Random() -> number
 *
 * Returns a random number. Use this to have portable randomness (standard
 * library random function implementations differ among platforms).
 */
static int
LUA_Random(lua_State *L)
{
        L_numarg_range(L, 0, 0);
        lua_pushnumber(L, random_func());
        return 1;
}

#ifndef EAPI_MACROS
#define EAPI_MACROS

/* Add function [f] to "eapi" namespace table. */
#define EAPI_SET_FUNC(name, f) \
do { \
        lua_pushstring(L, (name)); \
        lua_pushcfunction(L, (f)); \
        lua_rawset((L), eapi_index); \
} while (0)

/* Add integer to "eapi" namespace table. */
#define EAPI_SET_INT(name, c) \
do { \
        lua_pushstring(L, (name)); \
        lua_pushinteger(L, (c)); \
        lua_rawset(L, eapi_index); \
} while (0)

#define EAPI_SET_USERDATA(name, u) \
do { \
        lua_pushstring(L, (name)); \
        lua_pushlightuserdata(L, (u)); \
        lua_rawset(L, eapi_index); \
} while (0) \

#endif /* EAPI_SET */

/*
 * Register engine "API" routines with Lua.
 */
void
eapi_register(lua_State *L)
{
        /* Create a namespace "eapi" for all API routines. */
        lua_newtable(L);                        /* ... {} */
        lua_setglobal(L, "eapi");               /* ... */

        /* Leave "eapi" table on the stack and remember its index. */
        extern int eapi_index;
        lua_getglobal(L, "eapi");               /* ... eapi */
        eapi_index = lua_gettop(L);
        L_state = L;
        
        /* Input API. */
#if ENABLE_KEYS
        EAPI_SET_FUNC("__BindKeyboard",  LUA_BindKeyboard);
        EAPI_SET_FUNC("GetKeyName",      LUA_GetKeyName);
        EAPI_SET_FUNC("GetKeyFromName",  LUA_GetKeyFromName);
#endif
#if ENABLE_TOUCH
        EAPI_SET_FUNC("BindTouchDown",   LUA_BindTouchDown);
        EAPI_SET_FUNC("BindTouchMove",   LUA_BindTouchMove);
        EAPI_SET_FUNC("BindTouchUp",     LUA_BindTouchUp);
        EAPI_SET_FUNC("UnbindTouches",   LUA_UnbindTouches);
#endif
#if ENABLE_MOUSE
        EAPI_SET_FUNC("__BindMouseClick", LUA_BindMouseClick);
        EAPI_SET_FUNC("__BindMouseMove",  LUA_BindMouseMove);
        EAPI_SET_FUNC("MousePos",         LUA_MousePos);
#endif
#if ENABLE_JOYSTICK
        EAPI_SET_FUNC("__BindJoystickButton", LUA_BindJoystickButton);
        EAPI_SET_FUNC("__BindJoystickAxis",   LUA_BindJoystickAxis);
#endif
        /* Create objects. */
        EAPI_SET_FUNC("NewWorld",        LUA_NewWorld);
        EAPI_SET_FUNC("NewBody",         LUA_NewBody);
        EAPI_SET_FUNC("NewCamera",       LUA_NewCamera);
        EAPI_SET_FUNC("NewShape",        LUA_NewShape);
        EAPI_SET_FUNC("NewTile",         LUA_NewTile);
        EAPI_SET_FUNC("NewTileCentered", LUA_NewTileCentered);
        EAPI_SET_FUNC("NewSpriteList",   LUA_NewSpriteList);
        EAPI_SET_FUNC("ChopImage",       LUA_ChopImage);
        EAPI_SET_FUNC("Clone",           LUA_Clone);
        
        /* Destroy objects. */
        EAPI_SET_FUNC("__Clear",         LUA_Clear);
        EAPI_SET_FUNC("Destroy",         LUA_Destroy);
        EAPI_SET_FUNC("Quit",            LUA_Quit);
        
        /* Timers. */
        EAPI_SET_FUNC("__AddTimer",      LUA_AddTimer);
        EAPI_SET_FUNC("CancelTimer",     LUA_CancelTimer);
        
        /* Step/after-step functions. */
        EAPI_SET_FUNC("__SetStep",       LUA_SetStep);
        EAPI_SET_FUNC("__SetAfterStep",  LUA_SetAfterStep);
        EAPI_SET_FUNC("__GetStep",       LUA_GetStep);
        EAPI_SET_FUNC("__GetAfterStep",  LUA_GetAfterStep);
        EAPI_SET_FUNC("__SetStepC",      LUA_SetStepC);
        //EAPI_SET_FUNC("SetAfterStepC", LUA_SetAfterStepC);
        
        /* Link/unlink bodies. */
        EAPI_SET_FUNC("Link",            LUA_Link);
        EAPI_SET_FUNC("Unlink",          LUA_Unlink);
        
        /* Collisions and selection. */
        EAPI_SET_FUNC("__Collide",       LUA_Collide);
        EAPI_SET_FUNC("GetGroup",        LUA_GetGroup);
        EAPI_SET_FUNC("SelectShape",     LUA_SelectShape);
#if ENABLE_TILE_GRID
        EAPI_SET_FUNC("SelectTiles",     LUA_SelectTiles);
#endif
        /* Random number generator. */
        EAPI_SET_FUNC("Random",          LUA_Random);
        EAPI_SET_FUNC("RandomSeed",      LUA_RandomSeed);
                        
        /* Position. */
        EAPI_SET_FUNC("SetPos",          LUA_SetPos);
        EAPI_SET_FUNC("SetPosX",         LUA_SetPosX);
        EAPI_SET_FUNC("SetPosY",         LUA_SetPosY);
        EAPI_SET_FUNC("SetPosCentered",  LUA_SetPosCentered);
        EAPI_SET_FUNC("GetPos",          LUA_GetPos);
        EAPI_SET_FUNC("GetPrevPos",      LUA_GetPrevPos);
        EAPI_SET_FUNC("GetDeltaPos",     LUA_GetDeltaPos);
        EAPI_SET_FUNC("AnimatePos",      LUA_AnimatePos);
        
        /* Velocity. */
        EAPI_SET_FUNC("GetVel",          LUA_GetVel);
        EAPI_SET_FUNC("SetVel",          LUA_SetVel);
        EAPI_SET_FUNC("SetVelX",         LUA_SetVelX);
        EAPI_SET_FUNC("SetVelY",         LUA_SetVelY);
        
        /* Acceleration. */
        EAPI_SET_FUNC("SetAcc",          LUA_SetAcc);
        
        /* Tile flip, depth, and blending mode. */
        EAPI_SET_FUNC("Blend",           LUA_Blend);
        EAPI_SET_FUNC("FlipX",           LUA_FlipX);
        EAPI_SET_FUNC("FlipY",           LUA_FlipY);
        EAPI_SET_FUNC("GetDepth",        LUA_GetDepth);
        EAPI_SET_FUNC("SetDepth",        LUA_SetDepth);
        
        /* Color. */
        EAPI_SET_FUNC("GetColor",       LUA_GetColor);
        EAPI_SET_FUNC("SetColor",       LUA_SetColor);
        EAPI_SET_FUNC("AnimateColor",   LUA_AnimateColor);
        EAPI_SET_FUNC("AnimatingColor", LUA_AnimatingColor);
        
        /* Size. */
        EAPI_SET_FUNC("GetSize",        LUA_GetSize);
        EAPI_SET_FUNC("SetSize",        LUA_SetSize);
        EAPI_SET_FUNC("AnimateSize",    LUA_AnimateSize);
        
        /* Angle. */
        //EAPI_SET_FUNC("GetAngle",     LUA_GetAngle);
        EAPI_SET_FUNC("SetAngle",       LUA_SetAngle);
        EAPI_SET_FUNC("AnimateAngle",   LUA_AnimateAngle);
                        
        /* Frame animation. */
        EAPI_SET_FUNC("GetFrame",       LUA_GetFrame);
        EAPI_SET_FUNC("SetFrame",       LUA_SetFrame);
        EAPI_SET_FUNC("Animate",        LUA_Animate);
        EAPI_SET_FUNC("AnimateFrame",   LUA_AnimateFrame);
        EAPI_SET_FUNC("AnimatingFrame", LUA_AnimatingFrame);
        EAPI_SET_FUNC("StopAnimation",  LUA_StopAnimation);
        EAPI_SET_FUNC("SetAnimPos",     LUA_SetAnimPos);
        
        /* Get values. */
        EAPI_SET_FUNC("GetWorld",       LUA_GetWorld);
        EAPI_SET_FUNC("GetBody",        LUA_GetBody);
        EAPI_SET_FUNC("GetShape",       LUA_GetShape);
        EAPI_SET_FUNC("GetTiles",       LUA_GetTiles);
        EAPI_SET_FUNC("GetTime",        LUA_GetTime);
        EAPI_SET_FUNC("GetSpriteList",  LUA_GetSpriteList);
        EAPI_SET_FUNC("SetSpriteList",  LUA_SetSpriteList);
      
        /* Recording/rewinding time. */
#if TRACE_MAX
        EAPI_SET_FUNC("Pause",          LUA_Pause);
        EAPI_SET_FUNC("Resume",         LUA_Resume);
        EAPI_SET_FUNC("Record",         LUA_Record);
        EAPI_SET_FUNC("Snapshot",       LUA_Snapshot);
        EAPI_SET_FUNC("Rewind",         LUA_Rewind);
#else
        EAPI_SET_FUNC("Pause",          LUA_Pause);
        EAPI_SET_FUNC("Resume",         LUA_Resume);
        EAPI_SET_FUNC("Record",         LUA_Dummy);
        EAPI_SET_FUNC("Snapshot",       LUA_Dummy);
        EAPI_SET_FUNC("Rewind",         LUA_Dummy);
#endif
        /* Misc. */
        EAPI_SET_FUNC("ShowCursor",     LUA_ShowCursor);
        EAPI_SET_FUNC("HideCursor",     LUA_HideCursor);
        EAPI_SET_FUNC("Enable",         LUA_Enable);
        EAPI_SET_FUNC("Disable",        LUA_Disable);
        EAPI_SET_FUNC("DrawToTexture",  LUA_DrawToTexture);
        EAPI_SET_FUNC("NextCamera",     LUA_NextCamera);
        EAPI_SET_FUNC("SetBoundary",    LUA_SetBoundary);
        EAPI_SET_FUNC("What",           LUA_What);
        EAPI_SET_FUNC("Log",            LUA_Log);

        EAPI_SET_FUNC("Fractal",	LUA_Fractal);

        /* Audio functions. */
#if ENABLE_AUDIO
        if (audio_enabled()) {
                EAPI_SET_FUNC("PlaySound",       LUA_PlaySound);
                EAPI_SET_FUNC("FadeSound",       LUA_FadeSound);
                EAPI_SET_FUNC("SetVolume",       LUA_SetVolume);
                EAPI_SET_FUNC("BindVolume",      LUA_BindVolume);
                
                EAPI_SET_FUNC("PlayMusic",       LUA_PlayMusic);
                EAPI_SET_FUNC("FadeMusic",       LUA_FadeMusic);
                EAPI_SET_FUNC("SetMusicVolume",  LUA_SetMusicVolume);
                EAPI_SET_FUNC("PauseMusic",      LUA_PauseMusic);
                EAPI_SET_FUNC("ResumeMusic",     LUA_ResumeMusic);
        } else
#endif
        {
                EAPI_SET_FUNC("PlaySound",       LUA_Dummy);
                EAPI_SET_FUNC("FadeSound",       LUA_Dummy);
                EAPI_SET_FUNC("SetVolume",       LUA_Dummy);
                EAPI_SET_FUNC("BindVolume",      LUA_Dummy);
                
                EAPI_SET_FUNC("PlayMusic",       LUA_Dummy);
                EAPI_SET_FUNC("FadeMusic",       LUA_Dummy);
                EAPI_SET_FUNC("SetMusicVolume",  LUA_Dummy);
                EAPI_SET_FUNC("PauseMusic",      LUA_Dummy);
                EAPI_SET_FUNC("ResumeMusic",     LUA_Dummy);
        }
        
        /* Animation types. */
        EAPI_SET_INT("ANIM_LOOP",               ANIM_LOOP);
        EAPI_SET_INT("ANIM_CLAMP",              ANIM_CLAMP);
        EAPI_SET_INT("ANIM_CLAMP_EASEIN",       ANIM_CLAMP_EASEIN);
        EAPI_SET_INT("ANIM_CLAMP_EASEOUT",      ANIM_CLAMP_EASEOUT);
        EAPI_SET_INT("ANIM_CLAMP_EASEINOUT",    ANIM_CLAMP_EASEINOUT);
        EAPI_SET_INT("ANIM_REVERSE_CLAMP",      ANIM_REVERSE_CLAMP);
        EAPI_SET_INT("ANIM_REVERSE_LOOP",       ANIM_REVERSE_LOOP);
        
        /* Step functions. */
        EAPI_SET_USERDATA("STEPFUNC_STD",       stepfunc_std);
        EAPI_SET_USERDATA("STEPFUNC_ROT",       stepfunc_rot);
        
        /* Load the part of eapi interface that lives in eapi.lua. */
        extern int errfunc_index;
        if ((luaL_loadfile(L, "eapi.lua") || lua_pcall(L, 0, 0, errfunc_index)))
                fatal_error("[Lua] %s", lua_tostring(L, -1));
}

#endif  /* ENABLE_LUA */
