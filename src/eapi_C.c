#include <assert.h>
#include "audio.h"
#include "OpenGL_include.h"
#include "config.h"
#include "eapi_C.h"
#include "log.h"
#include "mem.h"
#include "misc.h"
#include "render.h"
#include "texture.h"
#include "world.h"
#include "uthash_tuned.h"
#include "utlist.h"

#if PLATFORM_IOS
  #include "glue.h"
#endif

#define L (NULL)

#define objtype_error(L, obj)                                                  \
do {                                                                           \
        fatal_error("Unexpected object type: %s.", object_name(obj));          \
        abort();                                                               \
} while (0)

#ifndef NDEBUG

#define info_assert_va(L, cond, fmt, ...)                               \
if (!(cond)) {                                                          \
        log_msg("[C] Assertion failed in " SOURCE_LOC);                 \
        log_err("Assertion (%s) failed: " fmt, #cond, __VA_ARGS__);     \
        abort();                                                        \
}

#define info_assert(L, cond, fmt)					\
if (!(cond)) {                                                          \
        log_msg("[C] Assertion failed in " SOURCE_LOC);                 \
        log_err("Assertion (%s) failed: " fmt, #cond);                  \
        abort();                                                        \
}

#define valid_world(L, x) \
do { \
        info_assert_va(L, ((x) && ((World *)(x))->objtype == OBJTYPE_WORLD && \
                       ((World *)(x))->step_ms > 0 && !((World *)(x))->killme), \
                        "Invalid World object; looks more like `%s`.", object_name(x)); \
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

#define valid_timerptr(x)                                               \
        ((x) && ((TimerPtr *)(x))->objtype == OBJTYPE_TIMERPTR)

#else   /* NDEBUG */

#define info_assert(...)        ((void)0)
#define info_assert_va(...)     ((void)0)
#define valid_camera(...)       ((void)0)
#define valid_tile(...)         ((void)0)
#define valid_body(...)         ((void)0)
#define valid_shape(...)        ((void)0)
#define valid_world(...)        ((void)0)
#define valid_spritelist(...)   ((void)0)

#endif  /* !NDEBUG */

static Body *
get_body(void *unused, void *obj)
{
        UNUSED(unused);
        switch (*(int *)obj) {
        case OBJTYPE_BODY: {
                valid_body(unused, obj);
                return obj;
        }
        case OBJTYPE_WORLD:  {
                valid_world(unused, obj);
                return &((World *)obj)->static_body;
        }
        case OBJTYPE_CAMERA: {
                valid_camera(unused, obj);
                return &((Camera *)obj)->body;
        }
        default:
                objtype_error(unused, obj);
        }
        abort();
}

#if ENABLE_KEYS

/*
 * Set keypress handling function.
 */
void
BindKeyboard(KeyHandler handler, intptr_t data)
{
        extern EventFunc key_bind;
        
        key_bind.func.key_func = handler;
        key_bind.callback_data = data;
}

const char *
GetKeyName(SDL_Scancode scancode)
{
        const char *name = SDL_GetScancodeName(scancode);
        if (*name == '\0')
                return "???";
        return name;
}

SDL_Scancode
GetKeyFromName(const char *name)
{
        SDL_Scancode scancode = SDL_GetScancodeFromName(name);
        info_assert_va(L, scancode != SDL_SCANCODE_UNKNOWN,
                       "Unknown scancode name `%s`.", name);
        return scancode;
}

#endif  /* ENABLE_KEYS */

#if ENABLE_TOUCH

/*
 * Bind a function that will handle all touch-down events.
 *
 * cam          Each camera may have its own touch handler bindings.
 * handler      Touch handling function pointer.
 * data         Callback data pointer that handler will receive.
 *
 * If handler argument is NULL, then the current touch handler function will be
 * unbound.
 */
void
BindTouchDown(Camera *cam, TouchHandler handler, intptr_t data)
{
        valid_camera(L, cam);
        assert(handler);
        
        EventFunc *bind = &cam->touchdown_bind;
        bind->type = (handler == NULL) ? EVFUNC_TYPE_NONE : EVFUNC_TYPE_C;
        bind->func.touch_func = handler;
        bind->callback_data = data;
}

/*
 * Bind a function that will handle all touch-move events.
 *
 * cam          Each camera may have its own touch handler bindings.
 * handler      Touch handling function pointer.
 * data         Callback data pointer that handler will receive.
 *
 * If handler argument is NULL, then the current touch handler function will be
 * unbound.
 */
void
BindTouchMove(Camera *cam, TouchHandler handler, intptr_t data)
{
        valid_camera(L, cam);
        assert(handler);
        
        EventFunc *bind = &cam->touchmove_bind;
        bind->type = (handler == NULL) ? EVFUNC_TYPE_NONE : EVFUNC_TYPE_C;
        bind->func.touch_func = handler;
        bind->callback_data = data;
}

/*
 * Bind a function that will handle all touch-up events.
 *
 * cam          Each camera may have its own touch handler bindings.
 * handler      Touch handling function pointer.
 * data         Callback data pointer that handler will receive.
 *
 * If handler argument is NULL, then the current touch handler function will be
 * unbound.
 */
void
BindTouchUp(Camera *cam, TouchHandler handler, intptr_t data)
{
        valid_camera(L, cam);
        assert(handler);
        
        EventFunc *bind = &cam->touchup_bind;
        bind->type = (handler == NULL) ? EVFUNC_TYPE_NONE : EVFUNC_TYPE_C;
        bind->func.touch_func = handler;
        bind->callback_data = data;
}

/*
 * Unbind all touch handlers that have been set for camera.
 */
void
UnbindTouches(Camera *cam)
{
        valid_camera(L, cam);
        memset(&cam->touchdown_bind, 0, sizeof(EventFunc));
        memset(&cam->touchup_bind, 0, sizeof(EventFunc));
        memset(&cam->touchmove_bind, 0, sizeof(EventFunc));
}

#endif  /* ENABLE_TOUCH */

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
World *
NewWorld(const char *name, unsigned step, BB area, unsigned cellsz,
         unsigned trace_skip)
{
        info_assert(L, area.l < area.r && area.b < area.t, "Invalid area box.");
        return world_new(name, step, area, cellsz, trace_skip);
}

/*
 * NewBody(parent, pos, nocturnal=false) -> userdata
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
Body *
NewBody(void *parent, vect_f pos)
{
        unsigned flags = 0;
        return body_new(get_body(L, parent), pos, flags);
}

/*
 * Create a camera that sees the world it is bound to and renders its view to a
 * viewport.
 *
 * world        World as returned by NewWorld().
 * pos          Position vector.
 * size         Size vector: size of the area that camera is able to see.
 *              Passing zero as one of the two dimensions makes the engine
 *              calculate the other one based on viewport size (same aspect
 *              ratio for both). If NULL, size is the same as viewport size.
 * viewport     Area of the window that camera draws to. Specified as bounding
 *              box: {l=?,r=?,b=?,t=?}, assuming coordinates are in pixels, with
 *              top left corner being (0,0). If NULL, viewport is calculated
 *              from size. If size is missing too, then we draw to the whole
 *              screen.
 * sort         Determines camera sort order. Those with smaller "sort" values
 *              will be rendered first.
 */
Camera *
NewCamera(World *world, vect_f pos, const vect_i *size_arg,
          const BB *viewport_arg, int sort)
{
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
                size.x = (int)lroundf(size.y*(viewport.r-viewport.l)/(viewport.b-viewport.t));
        if (size.y == 0)
                size.y = (int)lroundf(size.x*(viewport.b-viewport.t)/(viewport.r-viewport.l));
        
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
        
        return cam;
}

/*
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
 */
Shape *
NewShape(void *parent, vect_f offset, BB rect, const char *groupName)
{                
        /* Body and World. */
        Body *body = get_body(L, parent);
        World *world = body->world;
        
        /* Extract collision group name and find corresponding group. */
        assert(groupName && strlen(groupName) > 0);
        Group *group;
        HASH_FIND_STR(world->groups, groupName, group);
        
        /* Create group if it doesn't exist yet. */
        if (group == NULL) {
                extern mem_pool mp_group;
                group = mp_alloc(&mp_group);
                assert(strlen(groupName) < sizeof(group->name));
                
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
        
        return s;
}

/*
 * Create a new tile.
 *
 * parent               Object the tile will be attached to. Presently accepted
 *                      objects: Body, Camera, and World. If world, then its
 *                      static body is used as parent.
 * offset               Tile offset (lower left corner) relative to object's
 *                      position.
 * size_arg             Tile size. If NULL or {0,0}, then sprite (current frame)
 *                      dimension are used.
 * spriteList           Sprite list pointer as returned by NewSpriteList().
 *                      If NULL, tile has no texture (color only). In this case
 *                      size must be given.
 * depth                Depth determines drawing order. Into the screen is
 *                      negative.
 */
Tile *
NewTile(void *parent, vect_f offset, const vect_f *size_arg, SpriteList *spriteList, float depth)
{
#ifndef NDEBUG
        if (spriteList == NULL) {
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
        
        /* Create the tile and set its sprite-list. */
        int grid_store = (*(int *)parent != OBJTYPE_CAMERA);
        Tile *t = tile_new(body, offset, size, depth, grid_store);
        SetSpriteList(t, spriteList);
                
        return t;
}

/*
 * Same as NewTile() except `offset` is interpreted as the desired center point
 * of created tile. The actual tile offset will still remain at its lower left
 * corner, so calling GetPos() on the new tile will not return the `offset`
 * argument to NewTileCentered() but instead the offset of its lower left
 * corner.
 */
Tile *
NewTileCentered(void *parent, vect_f offset, const vect_f *size, SpriteList *spriteList, float depth)
{
        Tile *t = NewTile(parent, offset, size, spriteList, depth);
        vect_f final_size = GetSize(t);
        vect_f pos = GetPos(t);
        SetPos(t, (vect_f){pos.x - final_size.x/2, pos.y - final_size.y/2});
        return t;
}

/*
 * Create a sprite list; pointer to the C structure is returned for later
 * reference.
 *
 * img          Image file name (e.g., "image/hello.png").
 * flags        Flags affect texture behavior.
 *              Use TEXFLAG_FILTER to get filtered textures.
 * subimage     A rectangular subimage specification in bounding box form.
 *
 * The function accepts multiple sub-image bounding box pointers. The last
 * argument must always be NULL, otherwise it will not know when to stop reading
 * the list.
 *
 * As a special case, if no bounding boxes are given (second argument is NULL),
 * then a sprite-list containing a single frame is returned. This frame will be
 * the whole texture image.
 */
SpriteList *
NewSpriteList(const char *texname, unsigned flags, const BB *first_subimage, ...)
{
        /*
         * Figure out what the name of the texture is, and try looking it up in
         * the global texture_hash. If not found, create a new texture.
         */
        Texture *tex = texture_load(texname, flags);
        if (tex == NULL)
                return NULL;    /* Could not load image. */
        
        /*
         * If no valid BB pointers are present, create a single frame that
         * contains the whole texture.
         */
        if (first_subimage == NULL) {
                TexFrag tf = {
                        .l = 0,
                        .r = tex->w,
                        .b = tex->h,
                        .t = 0
                };
                assert(tf.r > tf.l && tf.b > tf.t);
                return spritelist_new(tex, &tf, 1);
        }
        
        /*
         * Read vararg subimage specifications until NULL.
         */
        va_list ap;
        va_start(ap, first_subimage);
        unsigned num_frames = 0;
        TexFrag tmp_frames[100];
        for (const BB *subimg = first_subimage; subimg != NULL;
             subimg = va_arg(ap, BB *)) {
                /* Add a TexFrag to the frame list. */
                assert(num_frames < ARRAYSZ(tmp_frames));
                TexFrag *tf = &tmp_frames[num_frames++];
#if PLATFORM_IOS
                int f = (config.screen_width > 500) ? 2 : 1;
                tf->l = subimg->l * f;
                tf->r = subimg->r * f;
                tf->b = subimg->b * f;
                tf->t = subimg->t * f;
#else
                tf->l = subimg->l;
                tf->r = subimg->r;
                tf->b = subimg->b;
                tf->t = subimg->t;
#endif
                assert(tf->r > tf->l && tf->b > tf->t);
        }
        va_end(ap);
        
        return spritelist_new(tex, tmp_frames, num_frames);
}

/*
 * Create a sprite-list by chopping a texture into pieces.
 *
 * texname      Name of texture (see NewSpriteList for details).
 * flags        Texture behavior flags. See NewSpriteList() for details.
 * size         Size of each individual sprite (x=width and y=height).
 * first        The first frame index used (zero is the very first one).
 * total        Total number of frames put into resulting sprite-list. Zero
 *              means take as many frames as possible.
 * skip         Number of frames to skip over in each step. Zero means that all
 *              frames will be consecutive.
 *
 * Chop texture into frames and produce a single SpriteList that contains them
 * all. In the source image sprites are assumed to be ordered left-to-right and
 * top-to-bottom.
 *
 * Returns resulting sprite-list.
 */
SpriteList *
ChopImage(const char *texname, unsigned flags, vect_i size, unsigned first,
          unsigned total, unsigned skip)
{
        /*
         * Figure out what the name of the texture is, and try looking it up in
         * the global texture_hash. If not found, create a new texture.
         */
        Texture *tex = texture_load(texname, flags);
        
        assert(size.x > 0 && size.y > 0);
#if PLATFORM_IOS
        int factor = (config.screen_width > 500) ? 2 : 1;
        unsigned num_cols = tex->w / (size.x * factor); /* Number of columns. */
        unsigned num_rows = tex->h / (size.y * factor); /* Number of rows. */
#else        
        unsigned num_cols = tex->w / size.x;        /* Number of columns. */
        unsigned num_rows = tex->h / size.y;        /* Number of rows. */
#endif
        /* Create sprite-list frames. */
        unsigned num_frames = 0;
        TexFrag tmp_frames[200];
        unsigned sentinel = (total > 0) ? first + (total * (skip+1))
                                        : num_rows * num_cols;
        assert(sentinel <= num_rows * num_cols && first < sentinel);
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
                tf->l = c * (size.x + 0);
                tf->r = c * (size.x + 0) + size.x;
                tf->b = r * (size.y + 0) + size.y;
                tf->t = r * (size.y + 0);
#endif
        }
        assert(total == 0 || total == num_frames);
        return spritelist_new(tex, tmp_frames, num_frames);
}

/*
 * Get the world that object belongs to. If object is a world itself, return it.
 */
World *
GetWorld(void *obj)
{
        switch (*(int *)obj) {
        case OBJTYPE_WORLD: {
                valid_world(L, obj);
                return obj;
        }
        case OBJTYPE_BODY: {
                valid_body(L, obj);
                return ((Body *)obj)->world;
        }
        case OBJTYPE_SHAPE: {
                valid_shape(L, obj);
                return ((Shape *)obj)->body->world;
        }
        case OBJTYPE_CAMERA: {
                valid_camera(L, obj);
                return ((Camera *)obj)->body.world;
        }
        case OBJTYPE_TILE: {
                valid_tile(L, obj);
                return ((Tile *)obj)->body->world;
        }
        }
        objtype_error(L, obj);
}

/*
 * Get the body that object belongs to. If object is a body itself, return it.
 * If object is a world, return its static body.
 */
Body *
GetBody(void *obj)
{
        switch (*(int *)obj) {
        case OBJTYPE_WORLD: {
                valid_world(L, obj);
                return &((World *)obj)->static_body;
        }
        case OBJTYPE_TILE: {
                valid_tile(L, obj);
                return ((Tile *)obj)->body;
        }
        case OBJTYPE_SHAPE: {
                valid_shape(L, obj);
                return ((Shape *)obj)->body;
        }
        case OBJTYPE_BODY: {
                valid_body(L, obj);
                return obj;
        }
        case OBJTYPE_CAMERA: {
                valid_camera(L, obj);
                return &((Camera *)obj)->body;
        }
        }
        objtype_error(L, obj);
}

/*
 * Return image size in pixels. The image (texture) is laoded into memory if not
 * already there. It is for that reason that the `flags` argument must be given:
 * to know in what way the texture should be loaded -- filtered or not.
 *
 * image        Image filename.
 * flags        See NewSpriteList().
 */
vect_i
GetImageSize(const char *image, unsigned flags)
{
        Texture *tex = texture_load(image, flags);
#if PLATFORM_IOS
        if (config.screen_width > 500)
                return (vect_i){tex->w / 2, tex->h / 2};
#endif
        return (vect_i){tex->w, tex->h};
}

/*
 * Each body has its own time. The function returns seconds since body creation,
 * To get absolute world time, pass in World or its static body as the argument.
 */
float
GetTime(void *obj)
{
        /* Now = (current step number) * (step duration in seconds) */
        Body *b = get_body(L, obj);
        return b->step * b->world->step_sec;
}

Tile *
GetFirstTile(void *obj)
{
        return GetBody(obj)->tiles;
}

Tile *
GetNextTile(Tile *t)
{
        return t->next;
}

/*
 * Get collision group by name.
 *
 * world        World as returned by NewWorld().
 * name         Collision group name.
 *
 * Returns NULL if collision group does not exist.
 */
Group *
GetGroup(World *world, const char *name)
{
        assert(name && *name);
        valid_world(L, world);
        
        Group *group;
        HASH_FIND_STR(world->groups, name, group);
        return group;
}

/*
 * Return sprite-list that was previously assigned to tile.
 */
SpriteList *
GetSpriteList(Tile *t)
{
        valid_tile(L, t);
        return t->sprite_list;
}

/*
 * Set camera zoom value.
 *
 * camera       Camera object as returned by NewCamera().
 * zoom         Zoom = 1 means no zoom. More than one zooms in, less than one
 *              zooms out.
 */
void
SetZoom(Camera *cam, float zoom)
{
        valid_camera(L, cam);
        info_assert_va(L, zoom > 0.0, "Zoom value (%f) must be positive.", zoom);
        if (zoom < 0.1)
                zoom = 0.1;
        if (zoom > 100.9)
                zoom = 100.9;
        
        cam->zoom = zoom;
}
        
vect_f
GetSize(void *obj)
{
        switch (*(int *)obj) {
        case OBJTYPE_CAMERA: {
                Camera *cam = obj;
                valid_camera(L, cam);
                
                return (vect_f){
                        .x=cam->size.x / cam->zoom,
                        .y=cam->size.y / cam->zoom
                };
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
                return size;
        }
        case OBJTYPE_SPRITELIST: {
                SpriteList *sprite_list = obj;
                valid_spritelist(L, sprite_list);
                vect_i size = texfrag_maxsize(sprite_list->frames,
                                              sprite_list->num_frames);
                return (vect_f){size.x, size.y};
        }
        }
        objtype_error(L, obj);
}

/*
 * Change tile size.
 */
void
SetSize(void *obj, vect_f size)
{
        switch (*(int *)obj) {
        case OBJTYPE_TILE: {
                valid_tile(L, obj);
                tile_set_size(obj, size);
                return;
        }
        }
        objtype_error(L, obj);
}

void
AnimateSize(void *obj, uint8_t type, vect_f end, float duration, float start_time)
{
        valid_tile(L, obj);
        tile_anim_size(obj, type, end, duration, start_time);
}

/*
 * Animate both size and position so that the tile's center stays in place.
 */
void
AnimateSizeCentered(void *obj, uint8_t type, vect_f end_sz, float duration, float start_time)
{
        valid_tile(L, obj);
        
        /* Animate size. */
        tile_anim_size(obj, type, end_sz, duration, start_time);
        
        /* Animate position. */
        vect_f diff = vect_f_sub(GetSize(obj), end_sz);
        vect_f end_pos = vect_f_add(tile_pos(obj), (vect_f){diff.x/2, diff.y/2});
        tile_anim_pos(obj, type, end_pos, duration, start_time);
}

/*
 * Change tile color, or the background color of camera view.
 *
 * obj          Tile or Camera object.
 * c            32-bit color.
 */
void
SetColor(void *obj, uint32_t c)
{
        switch (*(int *)obj) {
        case OBJTYPE_TILE: {
                valid_tile(L, obj);
                tile_set_color(obj, c);
                return;
        }
        case OBJTYPE_CAMERA: {
                valid_camera(L, obj);
                cam_set_color(obj, c);
                return;
        }
        }
        objtype_error(L, obj);
}

uint32_t
GetColor(void *obj)
{
        valid_tile(L, obj);
        return tile_color(obj);
}

void
AnimateColor(void *obj, uint8_t type, uint32_t end, float duration, float start_time)
{
        switch (*(int *)obj) {
                case OBJTYPE_TILE: {
                        valid_tile(L, obj);
                        tile_anim_color(obj, type, end, duration, start_time);
                        return;
                }
                case OBJTYPE_CAMERA: {
                        valid_camera(L, obj);
                        cam_anim_color(obj, type, end, duration, start_time);
                        return;
                }
        }
        objtype_error(L, obj);;
}

/*
 * Set the position of an object.
 *
 * object       Accepted objects: Body, Camera, Tile, Shape.
 * pos          Position vector.
 *
 * For Bodies and Cameras, the position is relative to the World they belong to.
 * For Tiles, however, only their relative position is altered (with respect
 * to the body that owns the Tile).
 *
 * Shapes do not have an explicit position, so instead the given position is
 * added to all the shape's relative offsets.
 */
void
SetPos(void *obj, vect_f pos)
{
        switch (*(int *)obj) {
        case OBJTYPE_BODY: {
                valid_body(L, obj);
                body_set_pos(obj, pos);
                return;
        }
        case OBJTYPE_CAMERA: {
                valid_camera(L, obj);
                cam_set_pos(obj, pos);
                return;
        }
        case OBJTYPE_TILE: {
                valid_tile(L, obj);                
                tile_set_pos(obj, pos);
                return;
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
                return;
        }
        }
        objtype_error(L, obj);
}

void
SetPosCentered(void *obj, vect_f pos)
{
        vect_f sz = GetSize(obj);
        vect_f cpos = {pos.x-sz.x/2, pos.y-sz.y/2};
        SetPos(obj, cpos);
}

void
SetPosX(void *obj, float value)
{
        SetPos(obj, (vect_f){value, GetPos(obj).y});
}

void
SetPosY(void *obj, float value)
{
        SetPos(obj, (vect_f){GetPos(obj).x, value});
}

void
AnimatePos(void *obj, uint8_t type, vect_f end, float duration, float start_time)
{
        switch (*(int *)obj) {
        case OBJTYPE_TILE: {
                valid_tile(L, obj);
                tile_anim_pos(obj, type, end, duration, start_time);
                return;
        }
        case OBJTYPE_CAMERA: {
                valid_camera(L, obj);
                body_anim_pos(&((Camera *)obj)->body, type, end, duration, start_time);
                return;
        }
        case OBJTYPE_BODY: {
                valid_body(L, obj);
                body_anim_pos(obj, type, end, duration, start_time);
                return;
        }
        }
        objtype_error(L, obj);
}

void
SetAccX(void *obj, float value)
{
        SetAcc(obj, (vect_f){value, GetAcc(obj).y});
}

void
SetAccY(void *obj, float value)
{
        SetAcc(obj, (vect_f){GetAcc(obj).x, value});
}

/*
 * Get object position (relative to its parent). Supported objects: Body, Camera, Tile.
 */
vect_f
GetPos(void *obj)
{
        switch (*(int *)obj) {
        case OBJTYPE_BODY: {
                valid_body(L, obj);
                return body_pos(obj);
        }
        case OBJTYPE_CAMERA: {
                valid_camera(L, obj);
                return body_pos(&((Camera *)obj)->body);
        }
        case OBJTYPE_TILE: {
                valid_tile(L, obj);
                return tile_pos(obj);
        }
        }
        objtype_error(L, obj);
}

/*
 * Get absolute object position (relative to world). Supported objects: Body, Camera.
 */
vect_f
GetAbsolutePos(void *obj)
{
        switch (*(int *)obj) {
        case OBJTYPE_BODY: {
                valid_body(L, obj);
                return body_absolute_pos(obj);
        }
        case OBJTYPE_CAMERA: {
                valid_camera(L, obj);
                return body_absolute_pos(&((Camera *)obj)->body);
        }
        }
        objtype_error(L, obj);
}

/*
 * Return the velocity of an object. Supported objects: Body, Camera.
 */
vect_f
GetVel(void *obj)
{
        switch (*(int *)obj) {
                case OBJTYPE_BODY: {
                        valid_body(L, obj);
                        return ((Body *)obj)->vel;
                }
                case OBJTYPE_CAMERA: {
                        valid_camera(L, obj);
                        return ((Camera *)obj)->body.vel;
                }
        }
        objtype_error(L, obj);
}

/*
 * Set an object's velocity.
 *
 * obj          Body or Camera object.
 * vel          Floating point velocity vector.
 */
void
SetVel(void *obj, vect_f vel)
{
        switch (*(int *)obj) {
        case OBJTYPE_BODY: {
                valid_body(L, obj);
                ((Body *)obj)->vel = vel;
                return;
        }
        case OBJTYPE_CAMERA: {
                valid_camera(L, obj);
                ((Camera *)obj)->body.vel = vel;
                return;
        }
        }
        objtype_error(L, obj);
}

void
SetVelX(void *obj, float value)
{
        SetVel(obj, (vect_f){value, GetVel(obj).y});
}

void
SetVelY(void *obj, float value)
{
        SetVel(obj, (vect_f){GetVel(obj).x, value});
}

/*
 * Return the acceleration of an object. Supported objects: Body, Camera.
 */
vect_f
GetAcc(void *obj)
{
        switch (*(int *)obj) {
        case OBJTYPE_BODY: {
                valid_body(L, obj);
                return ((Body *)obj)->acc;
        }
        case OBJTYPE_CAMERA: {
                valid_camera(L, obj);
                return ((Camera *)obj)->body.acc;
        }
        }
        objtype_error(L, obj);
}

/*
 * Set an object's acceleration.
 *
 * obj          Body or Camera object pointer.
 * acc          Floating point acceleration vector.
 */
void
SetAcc(void *obj, vect_f acc)
{
        switch (*(int *)obj) {
        case OBJTYPE_BODY: {
                valid_body(L, obj);
                ((Body *)obj)->acc = acc;
                return;
        }
        case OBJTYPE_CAMERA: {
                valid_camera(L, obj);
                ((Camera *)obj)->body.acc = acc;
                return;
        }
        }
        objtype_error(L, obj);
}

void
SetDepth(Tile *t, float depth)
{
        valid_tile(L, t);
        t->depth = depth;
}

/*
 * Set horizontal tile flip state.
 *
 * flip          0 -- no flip, default orientation;
 *               1 -- tile is flipped;
 *              -1 -- current flip state unchanged.
 *
 * Return current flip state (after any changes that this function has done).
 */
int
FlipX(Tile *t, int flip)
{
        if (flip != -1) {
                if (flip)
                        t->flags |= TILE_FLIP_X;
                else
                        t->flags &= ~TILE_FLIP_X;
        }
        
        return (t->flags & TILE_FLIP_X);
}

/*
 * Set vertical tile flip state.
 *
 * flip          0 -- no flip, default orientation;
 *               1 -- tile is flipped;
 *              -1 -- current flip state unchanged.
 *
 * Return current flip state (after any changes that this function has done).
 */
int
FlipY(Tile *t, int flip)
{
        if (flip != -1) {
                if (flip)
                        t->flags |= TILE_FLIP_Y;
                else
                        t->flags &= ~TILE_FLIP_Y;
        }
        
        return (t->flags & TILE_FLIP_Y);
}

float
GetAngle(Tile *t)
{
        return tile_angle(t);
}

void
SetAngle(Tile *t, vect_f pivot, float angle)
{
        valid_tile(L, t);
        tile_set_angle(t, pivot, angle);
}

void
AnimateAngle(void *obj, uint8_t type, vect_f pivot, float angle, float duration, float start_time)
{
        valid_tile(L, obj);
        tile_anim_angle(obj, type, pivot, angle, duration, start_time);
}

void
SetShape(Shape *s, BB bb)
{
        valid_shape(L, s);
        shape_set_def(s, (ShapeDef){.rect=bb});
}

/*
 * Set the step and after-step functions of an object.
 * Setting the step function of a World object sets the step function of its
 * static body.
 *
 * object               Accepted objects: Body, Camera, World.
 * stepFuncID           Step function ID.
 * afterStepFuncID      After-step function ID.
 */
void
SetStep(void *obj, StepFunction sf, intptr_t data)
{
        Body *b = get_body(L, obj);
        
        /* Set step/after-step function. */
        b->flags |= BODY_STEP_C;
        b->step_func = (intptr_t)sf;
        b->step_cb_data = data;
}

/*
 * Assign sprite-list to a tile.
 *
 * tile         Tile pointer as returned by NewTile().
 * spriteList   Sprite-list pointer as returend by NewSpriteList() or NULL if
 *              current sprite-list should be removed from tile.
 */
void
SetSpriteList(Tile *t, SpriteList *sprite_list)
{
        /*
         * If explicit tile size was not previously set, calculate it from
         * either current or previous sprite-list.
         */
        valid_tile(L, t);
        if (tile_size(t).x <= 0) {
                SpriteList *sz_sprite_list = sprite_list != NULL ? sprite_list :
                t->sprite_list;
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
}

/*
 * Select tile frame manually.
 * If an ongoing frame animation exists, it is stopped.
 */
void
SetFrame(Tile *t, unsigned frame_index)
{
        valid_tile(L, t);
        info_assert(L, t->sprite_list, "Cannot set frame because tile has no sprite-list.");
        info_assert_va(L, frame_index < t->sprite_list->num_frames,
                       "Frame index (%d) out of range [%d..%d]", frame_index, 0, t->sprite_list->num_frames);
        tile_set_frame(t, frame_index);
}

/*
 * Exactly like SetFrame() except if frame number falls outside valid range, it
 * is adjusted like so:
 *      if frame_number < 0 then frame_number = 0
 *      if frame_number >= numFrames then frame_number = num_frames - 1
 *
 * t            Tile object as returned by NewTile().
 * new_index    Index into a sprite-list frame array.
 */
void
SetFrameClamp(Tile *t, int new_index)
{
        valid_tile(L, t);
        if (new_index < 0)
                new_index = 0;
        else if ((unsigned)new_index >= t->sprite_list->num_frames)
                new_index = t->sprite_list->num_frames - 1;
        
        tile_set_frame(t, new_index);
}

/*
 * Get current frame index.
 */
unsigned
GetFrame(Tile *t)
{
        valid_tile(L, t);
        return tile_frame(t);
}

/*
 * Animate(obj, animType, FPS, startTime=0)
 *
 * Do frame animation.
 *
 * obj          Tile object.
 * animType     eapi.ANIM_CLAMP, eapi.ANIM_LOOP, etc. See property.h for a full
 *              list.
 * FPS          Animation speed: frames per second. If negative, animation is
 *              assumed to start from last frame and go backwards.
 * startTime    When to assume the animation started, in seconds relative to
 *              current world time.
 */
void
Animate(Tile *t, uint8_t anim_type, float FPS, float start_time)
{
        valid_tile(L, t);
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
        float duration = (float)(last-first) / FPS;
        tile_set_frame(t, first);
        if (start_time > 0.0)
                start_time = fmod(start_time, duration) - duration;
        tile_anim_frame(t, anim_type, last, duration, start_time);
}

void
AnimateShape(Shape *s, uint8_t type, BB end, float duration, float start_time)
{
        valid_shape(L, s);
        shape_anim_def(s, type, (ShapeDef){ .rect = end }, duration, start_time);
}

/*
 * Select shapes that cover the given point.
 *
 * world        World pointer as returned by NewWorld().
 * point        Position vector (relative to world origin).
 * group        Ignore all shapes that do not belong to this group (i.e.,
 *              consider only the shapes that belong to this group). Passing
 *              NULL here means that no such filtering is done.
 * shape_buf    Result buffer that receives shape pointers.
 * max_shapes   Max number of shape pointers that can fit into shape_buf.
 *
 * Returns the number of shapes that cover a given point (this number will not
 * exceed max_shapes even if in reality there are more shapes there than
 * max_shapes).
 */
uint
SelectShapes(World *world, vect_i point, const char *group_name, Shape *shape_buf[], unsigned max_shapes)
{
        assert(shape_buf && max_shapes > 0);
        valid_world(L, world);
        
        /* See if we have to filter by group name. */
        Group *group = NULL;
        if (group_name != NULL) {
                HASH_FIND_STR(world->groups, group_name, group);
                if (group == NULL)
                        return 0;       /* No such group found. */
        }
        
        /* Expand the point a bit to create a bounding box. */
        BB bb = {
                .l=point.x-1, .b=point.y-1, .r=point.x+1, .t=point.y+1
        };
        
        /* Look up nearby shapes from grid. */
        void *nearby[100];
        unsigned num_nearby = grid_lookup(&world->grid, bb, nearby,
                                          ARRAYSZ(nearby), shape_filter);
        
        /*
         * Go over the lookup shapes. If we find one which really intersects our
         * point (and belongs to requested group), add it to result array.
         */
        unsigned num_shapes = 0;
        for (unsigned i = 0; i < num_nearby; i++) {
                Shape *s = nearby[i];
                valid_shape(L, s);
                if (group != NULL && s->group != group)
                        continue;       /* Not in requested group. */
                
                /* Add to user shape buf if there's overlap. */
                if (shape_vs_bb(s, bb))
                        shape_buf[num_shapes++] = s;
                
                if (num_shapes == max_shapes)
                        break;  /* User shape buffer is full. */
        }
        return num_shapes;
}

/*
 * Select the first shape that covers the given point (return a pointer to it).
 * If none do, return NULL.
 *
 * world        World pointer as returned by NewWorld().
 * point        Position vector.
 * group        Ignore all shapes that do not belong to this group (i.e.,
 *              consider only the shapes that belong to this group). Passing
 *              NULL here means that no such filtering is done.
 */
Shape *
SelectShape(World *world, vect_i point, const char *group_name)
{
        Shape *s = NULL;
        SelectShapes(world, point, group_name, &s, 1);
        return s;
}

/*
 * Schedule a task for later execution.
 *
 * obj          World or Body object.
 * when         Number of seconds that should pass before timer is executed.
 * func         Timer function that will be called.
 *
 * Timer callback C prototype:
 *      void (*TimerFunction)(void *obj, void *data);
 */
TimerPtr
AddTimer(void *obj, float when, TimerFunction func, intptr_t data)
{
        /* Add timer to body. */
        info_assert(L, when >= 0.0, "Negative time offset.");
        Body *b = get_body(L, obj);
        Timer *tmr = body_add_timer(b, obj, when, OBJTYPE_TIMER_C,
                                    (intptr_t)func, data);
        return (TimerPtr){
                .objtype=OBJTYPE_TIMERPTR,
                .ptr=tmr,
                .timer_id=tmr->timer_id
        };
}

/*
 * Stop timer from being executed.
 */
void
CancelTimer(TimerPtr *timer_ptr)
{
        /* Ignore if timer is not alive. */
        if (!Alive(timer_ptr))
                return;
        
        void *owner = timer_ptr->ptr->owner;
        switch (*(int *)owner) {
        case OBJTYPE_BODY: {
                valid_body(L, owner);
                body_cancel_timer(owner, timer_ptr->ptr);
                break;
        }
        case OBJTYPE_WORLD: {
                World *world = owner;
                valid_world(L, world);
                body_cancel_timer(&world->static_body, timer_ptr->ptr);
                break;
        }
        case OBJTYPE_CAMERA: {
                Camera *cam = owner;
                valid_camera(L, cam);
                body_cancel_timer(&cam->body, timer_ptr->ptr);
                break;
        }
        default:
                objtype_error(L, owner);
        }
        
        timer_ptr->ptr = NULL;
}

/*
 * Pause(obj)
 *
 * Pause world or disable camera.
 */
void
Pause(void *obj)
{
        switch (*(int *)obj) {
        case OBJTYPE_WORLD: {
                valid_world(L, obj);
#if ENABLE_AUDIO
                /* Pause world audio. */
                audio_pause_group((uintptr_t)obj);
#endif
                ((World *)obj)->paused = 1;
                return;
        }
        case OBJTYPE_CAMERA: {
                valid_camera(L, obj);
                ((Camera *)obj)->disabled = 1;
                return;
        }
        }
        objtype_error(L, obj);
}

/*
 * Resume(obj)
 *
 * Resume world or camera.
 */
void
Resume(void *obj)
{
        switch (*(int *)obj) {
        case OBJTYPE_WORLD: {
                valid_world(L, obj);
#if ENABLE_AUDIO
                /* Resume world audio. */
                audio_resume_group((uintptr_t)obj);
#endif
                ((World *)obj)->paused = 0;
                return;
        }
        case OBJTYPE_CAMERA: {
                valid_camera(L, obj);
                ((Camera *)obj)->disabled = 0;
                return;
        }
        }
        objtype_error(L, obj);
}

/*
 * Returns true if something is enabled and active.
 *
 * Timer:       If true, timer is active and is going to be executed at some
 *              later time.
 * World:       If true, World is not paused, its step functions, collision
 *              routines and timers are being executed.
 * Camera:      If true, camera is handling touch events and rendering its view.
 */
int
Alive(void *obj)
{
        switch (*(int *)obj) {
        case OBJTYPE_TIMERPTR: {
                TimerPtr *timer_ptr = obj;
                assert(valid_timerptr(timer_ptr));
                
                /*
                 * If timer pointer is NULL or timer ID is zero, then timer is
                 * not alive.
                 */
                if (timer_ptr->ptr == NULL || timer_ptr->timer_id == 0)
                        return 0;
                /*
                 * If timer memory has been cleared (owner == NULL) or timer IDs
                 * do not match (memory reused), then timer is not alive.
                 */
                if (timer_ptr->ptr->owner == NULL || timer_ptr->timer_id != timer_ptr->ptr->timer_id) {
                        memset(timer_ptr, 0, sizeof(*timer_ptr));
                        return 0;
                }
                return 1;
        }
        case OBJTYPE_CAMERA: {
                valid_camera(L, obj);
                return !((Camera *)obj)->disabled;
        }
        case OBJTYPE_WORLD: {
                valid_world(L, obj);
                return !((World *)obj)->paused;
        }
        }
        
        /* Destroyed objects are considered `not alive`. */
        return 0;
}

/*
 * Render to named texture.
 *
 * name         Name of the texture that we're drawing into.
 * flags        Flags affect texture behavior; see NewSpriteList() for details.
 */
void
DrawToTexture(const char *name, unsigned flags)
{
        assert(name && *name != '\0');
        
        /* Set up the texture with screen size. */
        Texture *tex = texture_load_blank(name, flags);
        texture_set_size(tex, config.screen_width, config.screen_height);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tex->pow_w, tex->pow_h, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, NULL);
        if (glGenerateMipmap != NULL && (flags & TEXFLAG_FILTER))
                glGenerateMipmap(GL_TEXTURE_2D);
        
        /* Create and bind framebuffer. */
        GLuint framebuf;
        glGenFramebuffers(1, &framebuf);
        glBindFramebuffer(GL_FRAMEBUFFER_EXT, framebuf);
        assert(tex->id);
        glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                               GL_TEXTURE_2D, tex->id, 0);
        
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
}

void
StopAccelerometer(void)
{
#if PLATFORM_IOS
        glue_stop_accelerometer();
#endif
}

/*
 * Read current accelerometer values. If accelerometer device is not
 * initialized yet, then GetAccelerometerAxes() will do it.
 *
 * Note that if the accelerometer device has not been initialized, then the
 * function returns 0 and `values` vector will not be modified. So the `values`
 * are usable only if retuned value is 1.
 *
 * Accelerometer device is stopped whenever Clear() is called. This behavior
 * should, however, be configurable because some applications may need the
 * accelerometer to be enabled all the time.
 */
int
GetAccelerometerAxes(vect3d *values)
{
#if PLATFORM_IOS
        return glue_get_accelerometer_axes(values);
#else
        UNUSED(values);
        return 0;
#endif
}

/*
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
 * data         User callback data.
 *
 * Collision function C prototype:
 *      int (*CollisionFunc)(World *, Shape *A, Shape *B, int new_collision, BB *resolve, void *data);
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
void
Collide(World *w, const char *groupname_a, const char *groupname_b,
        CollisionFunc cf, int update, int priority, intptr_t data)
{
        /* Create group A if it doesn't exist. */
        Group *groupA;
        HASH_FIND_STR(w->groups, groupname_a, groupA);
        if (groupA == NULL) {
                extern mem_pool mp_group;
                groupA = mp_alloc(&mp_group);
                
                assert(strlen(groupname_a) < sizeof(groupA->name));
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
                
                assert(strlen(groupname_b) < sizeof(groupB->name));
                strcpy(groupB->name, groupname_b);
                
                groupB->index = w->next_group_id++;
                assert(groupB->index < SHAPEGROUPS_MAX);
                HASH_ADD_STR(w->groups, name, groupB);
        }
        
        /* Get handler structure pointer from `handler_map` array. */
        Handler *handler = &w->handler_map[groupA->index][groupB->index];
        
        /* Verify there is no handler for reversed groups. */
        assert(w->handler_map[groupB->index][groupA->index].type == HANDLER_NONE);
        
        /* Each group remembers how many handlers we have registered for it. */
        if (handler->func == 0 && cf != NULL)
                groupA->num_handlers++;
        else if (handler->func != 0 && cf == NULL)
                groupA->num_handlers--;
        
        /* Set handler data. */
        handler->type = HANDLER_C;
        handler->func = (intptr_t)cf;
        handler->update = update;
        handler->priority = priority;
        handler->data = data;
}

/*
 * Free any resources owned by object. Passing an object into API routines after
 * it has been destroyed will result in assertion failures saying it is of
 * incorrect type. It is also possible (though unlikely) that its memory has
 * been reused by some other object, in which case the other object will be
 * affected instead.
 */
void
Destroy(void *obj)
{
        switch (*(int *)obj) {
        case OBJTYPE_BODY: {
                valid_body(L, obj);
                body_free(obj);
                return;
        }
        case OBJTYPE_SHAPE: {
                valid_shape(L, obj);
                shape_free(obj);
                return;
        }
        case OBJTYPE_TILE: {
                valid_tile(L, obj);
                tile_free(obj);
                return;
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
                return;
        }
        }
        objtype_error(L, obj);
}

/*
 * Clear engine state.
 */
void
Clear(void)
{
        StopAccelerometer();         /* Close accelerometer if it was opened. */
        
        /* Destroy textures that have not been used in a while. */
        texture_free_unused();

#if ENABLE_AUDIO
        audio_fadeout_group(0, 1000);   /* Fade out all sound channels. */
        // audio_free_unused();            /* Free sounds not recently used. */
#endif
#if ENABLE_KEYS
        extern EventFunc key_bind;
        memset(&key_bind, 0, sizeof(key_bind));
#endif
#if ENABLE_JOYSTICK
        extern EventFunc joybutton_bind, joyaxis_bind;
        memset(&joybutton_bind, 0, sizeof(joybutton_bind));
        memset(&joyaxis_bind, 0, sizeof(joyaxis_bind));
#endif
#if ENABLE_TOUCH
        /* Clear any remaining touches. */
        clear_touches();
#endif
        /* Clear out worlds and schedule them for destruction. */
        extern mem_pool mp_world;
        for (World *w = mp_first(&mp_world); w != NULL; w = mp_next(w)) {
                if (!w->killme)
                        world_kill(w);
        }
#ifndef NDEBUG
        /* Certain pools should be empty at this point. */
        extern mem_pool mp_body, mp_camera, mp_group, mp_collision, mp_gridcell;
        extern mem_pool mp_property;
        assert(mp_first(&mp_gridcell) == NULL);
        assert(mp_first(&mp_collision) == NULL);
        assert(mp_first(&mp_body) == NULL);
        assert(mp_first(&mp_camera) == NULL);
        assert(mp_first(&mp_group) == NULL);
        assert(mp_first(&mp_property) == NULL);
#if TRACE_MAX
        extern mem_pool mp_bodytrace, mp_tiletrace, mp_shapetrace;
        assert(mp_first(&mp_bodytrace) == NULL);
        assert(mp_first(&mp_tiletrace) == NULL);
        assert(mp_first(&mp_shapetrace) == NULL);
#endif
#endif  /* !NDEBUG */
}

/*
 * Terminate program.
 */
void
Quit(void)
{
        exit(EXIT_SUCCESS);
}

/*
 * Sets how many megabytes may be used for texture storage inside video memory.
 * As soon as some new texture exceeds this approximate amount, older textures
 * are unloaded from memory.
 * If limit is set to zero, then all textures are unloaded from memory.
 */
void
SetTextureMemoryLimit(unsigned MB)
{
        texture_limit(MB);
}
