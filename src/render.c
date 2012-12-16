#include <assert.h>
#include "OpenGL_include.h"
#include "common.h"
#include "camera.h"
#include "config.h"
#include "geometry.h"
#include "log.h"
#include "misc.h"
#include "shape.h"
#include "spritelist.h"
#include "texture.h"
#include "tile.h"
#include "world.h"
#include "utlist.h"
#include "render.h"

#ifndef NDEBUG
int     drawShapes, drawBodies, drawTileBBs, drawShapeBBs, drawGrid;
int     outsideView;
#endif

/*
 * Disable texturing before calling draw_quad() and re-enable it afterwards:
 *
 *      glDisable(GL_TEXTURE_2D);
 *      glDisableClientState(GL_TEXTURE_COORD_ARRAY);
 *      draw_quad(...);
 *      glEnableClientState(GL_TEXTURE_COORD_ARRAY);
 *      glEnable(GL_TEXTURE_2D);
 */
static void
draw_quad(BB bb, uint32_t color)
{
        GLfloat vertex_array[] = {
                bb.l, bb.b,
                bb.r, bb.b,
                bb.l, bb.t,
                bb.r, bb.t,
        };
        uint32_t color_array[] = {color, color, color, color};
        
        glVertexPointer(2, GL_FLOAT, 0, vertex_array);
        glColorPointer(4, GL_UNSIGNED_BYTE, 0, color_array);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

/*
 * Depth comparison routine to use with qsort(). Into the screen is the negative
 * direction, out of the screen -- positive. We want tiles sorted back to front.
 */
static int
tile_depth_cmp(const void *a, const void *b)
{
        assert(a != NULL && b != NULL && a != b);
        assert((*(Tile **)a)->objtype == OBJTYPE_TILE);
        assert((*(Tile **)b)->objtype == OBJTYPE_TILE);
        
        /*
         * If depth values of both tiles are the same, compare their pointers.
         * This ensures that overlapping tiles with equal depth will not flicker
         * (change drawing order) due to unstable implementations of qsort() or
         * other factors.
         */
        if ((*(Tile **)a)->depth == (*(Tile **)b)->depth) {
                if (*(Tile **)a < *(Tile **)b)
                        return -1;
                return (*(Tile **)a > *(Tile **)b);
        }
        
        return ((*(Tile **)a)->depth < (*(Tile **)b)->depth) ? -1 : 1;
}

/* Vertex buffer offsets. */
#define VERT_COORD_SPACE     (sizeof(GLfloat) * 2)
#define VERT_TEXCOORD_SPACE  (sizeof(GLshort) * 2)
#define VERT_COLOR_SPACE     (sizeof(GLubyte) * 4)
#define VERT_COORD_OFFSET    0
#define VERT_TEXCOORD_OFFSET (VERT_COORD_OFFSET + VERT_COORD_SPACE)
#define VERT_COLOR_OFFSET    (VERT_TEXCOORD_OFFSET + VERT_TEXCOORD_SPACE)
#define VERT_SPACE           (VERT_COORD_SPACE + VERT_TEXCOORD_SPACE + VERT_COLOR_SPACE)

static void
prepare_tile_buf(Tile *t, unsigned char *buf)
{
        /* Put color values into buffer. */
        // XXX it's possible to replace this with glColor call but then
        // we have to call glDraw every time we switch.
        uint32_t color = tile_color(t);
        *((uint32_t *)&buf[VERT_SPACE*0 + VERT_COLOR_OFFSET]) = color;
        *((uint32_t *)&buf[VERT_SPACE*1 + VERT_COLOR_OFFSET]) = color;
        *((uint32_t *)&buf[VERT_SPACE*2 + VERT_COLOR_OFFSET]) = color;
        *((uint32_t *)&buf[VERT_SPACE*3 + VERT_COLOR_OFFSET]) = color;
        
        /* Use rounded coords/size if not requested otherwise. */
        vect_f pos = tile_pos(t);
        vect_f sz = tile_size(t);
#if !ALL_SMOOTH
        if (!(t->flags & TILE_SMOOTH)) {
                pos.x = posround(pos.x);
                pos.y = posround(pos.y);
                sz.x = lroundf(sz.x);
                sz.y = lroundf(sz.y);
        }
#endif  /* !ALL_SMOOTH */
        SpriteList *sl = t->sprite_list;
        assert((sz.x > 0.0 && sz.y > 0.0) ||
               (sl != NULL && sz.x < 0 && sz.y < 0));
        if (sl != NULL) {
                unsigned frame = tile_frame(t);
                assert(sl->frames != NULL && sl->num_frames > 0 &&
                       frame < sl->num_frames);
                
                /* If size is negative, use sprite size. */
                TexFrag tf = sl->frames[frame];
                if (sz.x < 0.0)
                        sz = texfrag_sizef(&tf);
                                
                /* Dump tile texture coordinates into buffer. */
                *((GLshort *)&buf[VERT_SPACE*0 + VERT_TEXCOORD_OFFSET]    ) = tf.l;
                *((GLshort *)&buf[VERT_SPACE*0 + VERT_TEXCOORD_OFFSET] + 1) = tf.b;
                *((GLshort *)&buf[VERT_SPACE*1 + VERT_TEXCOORD_OFFSET]    ) = tf.r;
                *((GLshort *)&buf[VERT_SPACE*1 + VERT_TEXCOORD_OFFSET] + 1) = tf.b;
                *((GLshort *)&buf[VERT_SPACE*2 + VERT_TEXCOORD_OFFSET]    ) = tf.l;
                *((GLshort *)&buf[VERT_SPACE*2 + VERT_TEXCOORD_OFFSET] + 1) = tf.t;
                *((GLshort *)&buf[VERT_SPACE*3 + VERT_TEXCOORD_OFFSET]    ) = tf.r;
                *((GLshort *)&buf[VERT_SPACE*3 + VERT_TEXCOORD_OFFSET] + 1) = tf.t;
        }

        /* Dump tile vertex coordinates into buffer. */
        *((GLfloat *)&buf[VERT_SPACE*0 + VERT_COORD_OFFSET]    ) = pos.x;
        *((GLfloat *)&buf[VERT_SPACE*0 + VERT_COORD_OFFSET] + 1) = pos.y;
        *((GLfloat *)&buf[VERT_SPACE*1 + VERT_COORD_OFFSET]    ) = pos.x + sz.x;
        *((GLfloat *)&buf[VERT_SPACE*1 + VERT_COORD_OFFSET] + 1) = pos.y;
        *((GLfloat *)&buf[VERT_SPACE*2 + VERT_COORD_OFFSET]    ) = pos.x;
        *((GLfloat *)&buf[VERT_SPACE*2 + VERT_COORD_OFFSET] + 1) = pos.y + sz.y;
        *((GLfloat *)&buf[VERT_SPACE*3 + VERT_COORD_OFFSET]    ) = pos.x + sz.x;
        *((GLfloat *)&buf[VERT_SPACE*3 + VERT_COORD_OFFSET] + 1) = pos.y + sz.y;
}

static inline void
draw_tile_buf(unsigned num_tiles)
{
        for (unsigned i = 0; i < num_tiles; i++) {
                glDrawArrays(GL_TRIANGLE_STRIP, i * 4, 4);
        }
}

/* Storage for tile vertex, texcoord, and color data. */
static unsigned char huge_buf[VISIBLE_TILES_MAX * VERT_SPACE * 4];

/*
 * Call glTranslate() with body position but be sure to add the positions of
 * parent bodies too!
 */
static void
body_translation(Body *b)
{
#if !ALL_SMOOTH
        int smooth = 0;
        vect_f trans = {0.0, 0.0};
        do {
                vect_f pos = body_pos(b);
                trans.x += pos.x;
                trans.y += pos.y;
                if (b->flags & BODY_SMOOTH_POS)
                        smooth = 1;
        } while ((b = b->parent) != NULL);
        
        if (smooth)
                glTranslatef(trans.x, trans.y, 0.0);
        else
                glTranslatef(posround(trans.x), posround(trans.y), 0.0);
#else
        vect_f trans = {0.0, 0.0};
        do {
                vect_f pos = body_pos(b);
                trans.x += pos.x;
                trans.y += pos.y;
        } while ((b = b->parent) != NULL);        
        glTranslatef(trans.x, trans.y, 0.0);
#endif  /* ALL_SMOOTH */
}

/* Blending state. */
static unsigned blend_mode_current = TILE_BLEND_SOURCE;
unsigned blend_mode_default = TILE_BLEND_SOURCE;

/*
 * Returns 0 if blending should not be switched for argument tile.
 * Returns new mode number if it should.
 */
static unsigned
blendmode_would_change(const Tile *t)
{
        /* See if we have to switch. */
        unsigned mode = (t->flags & TILE_BLEND);
        if (mode == TILE_BLEND_DEFAULT) {
                if (blend_mode_default == blend_mode_current)
                        return 0;
                mode = blend_mode_default;
        } else if (mode == blend_mode_current) {
                return 0;
        }
        return mode;
}

static void change_texture_env(void) {		
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);

    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB, GL_PREVIOUS);
    glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_RGB, GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
}

/*
 * Changes blending mode.
 * Returns 1 if an extra pass is necessary.
 */
static int
blendmode_set(unsigned mode, int pass)
{
        blend_mode_current = mode;
        
        /* Change mode. */
        switch (mode) {
        case TILE_BLEND_SOURCE:
                glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                return 0;
        case TILE_BLEND_MULTIPLY:
                glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
                glBlendFunc(GL_ZERO, GL_SRC_COLOR);
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                return 0;
        case TILE_BLEND_MASK:
                glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
                glBlendFunc(GL_ONE, GL_ZERO);
                glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
                return 0;
        case TILE_BLEND_DESTINATION:
                if (pass == 0) {
                        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,
                                  GL_MODULATE);
                        glBlendFunc(GL_DST_ALPHA, GL_ZERO);
                        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
                        return 1;
                }
                glBlendFunc(GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA);
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
                blend_mode_current = UINT_MAX;  /* Force blend mode switch. */
                return 0;
        case TILE_BLEND_ADD:
                glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                return 0;
        case TILE_BLEND_ALPHA:
                change_texture_env();
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                return 0;
        }
        abort();
}

/*
 * Add all tiles from given body to `tiles` array. Then proceed recursively to
 * add all tiles from the given body's children as well.
 */
static unsigned
add_all_tiles(Body *b, Tile *tiles[], unsigned num_tiles, unsigned max_tiles)
{
        Tile *t;
        DL_FOREACH(b->tiles, t) {
                assert(num_tiles < max_tiles);
                tiles[num_tiles++] = t;
        }
        
        Body *child;
        DL_FOREACH(b->children, child) {
                num_tiles = add_all_tiles(child, tiles, num_tiles, max_tiles);
        }
        return num_tiles;
}

static void
draw_visible_tiles(Camera *cam, BB visible_area)
{	
        /* Look up visible tiles. */
        World *w = cam->body.world;
        void *visible_tiles[VISIBLE_TILES_MAX];
        const unsigned max_tiles = ARRAYSZ(visible_tiles);
#if ENABLE_TILE_GRID
        unsigned num_tiles = grid_lookup(&w->grid, visible_area,
                                         visible_tiles, max_tiles, tile_filter);
#else
        UNUSED(visible_area);
        unsigned num_tiles = add_all_tiles(&w->static_body,
                                           (Tile **)visible_tiles, 0,
                                           max_tiles);
#endif
        
        /*
         * Add camera tiles to the list, since those are always visible and not
         * within the grid.
         */
        Tile *t;
        DL_FOREACH(cam->body.tiles, t) {
                assert(num_tiles < max_tiles);
                visible_tiles[num_tiles++] = t;
        }
        
        if (num_tiles == 0)
                return; /* Nothing to draw. */
        	
        /* Sort tiles by depth, so drawing happens back to front. */
        qsort(visible_tiles, num_tiles, sizeof(Tile *), tile_depth_cmp);
        
        /* We always put fresh tiles at the beginning of the buffer. */
        assert(max_tiles * VERT_SPACE * 4 <= sizeof(huge_buf));
        glVertexPointer(2, GL_FLOAT, VERT_SPACE, huge_buf + VERT_COORD_OFFSET);
        glTexCoordPointer(2, GL_SHORT, VERT_SPACE,
                          huge_buf + VERT_TEXCOORD_OFFSET);
        glColorPointer(4, GL_UNSIGNED_BYTE, VERT_SPACE,
                       huge_buf + VERT_COLOR_OFFSET);
        
        /* Set body of first tile as current_body. */
        Tile *first_tile = visible_tiles[0];
        assert(first_tile->objtype == OBJTYPE_TILE && first_tile->body);
        Body *current_body = first_tile->body;
        
        /* Bind texture for first tile. */
        SpriteList *sl = first_tile->sprite_list;
        texture_bind(sl == NULL ? NULL : sl->tex);
        
        /* Prepare modelview matrix for first body. */
        glPushMatrix();
        body_translation(current_body);
        
        /* Draw visible tiles. */
        unsigned num_undrawn = 0;       /* Number of undrawn tiles. */
        for (unsigned i = 0; i < num_tiles; i++) {
                t = visible_tiles[i];
                if (COLOR_ALPHA(tile_color(t)) == 0.0)
                        continue;       /* Tile is invisible. */
                
                if (t->body != current_body) {
                        /* Draw first, then switch matrix. */
                        if (num_undrawn > 0) {
                                draw_tile_buf(num_undrawn);
                                num_undrawn = 0;
                        }
                        
                        /* Next tile body is the "current" body. */
                        current_body = t->body;
                        
                        /* Set up matrix for the new body. */
                        glPopMatrix();
                        glPushMatrix();
                        body_translation(current_body);
                }
                
                /*
                 * Enable/disable texturing and/or switch texture if necessary.
                 */
                sl = t->sprite_list;
                if (texture_would_change(sl == NULL ? NULL : sl->tex)) {
                        /* Draw first, then switch texture. */
                        if (num_undrawn > 0) {
                                draw_tile_buf(num_undrawn);
                                num_undrawn = 0;
                        }
                        texture_bind(sl == NULL ? NULL : sl->tex);
                }
                                
                /* Apply rotation. */
                Property *rot = t->angle;
                if (rot != NULL) {
                        if (num_undrawn > 0) {
                                draw_tile_buf(num_undrawn);
                                num_undrawn = 0;
                        }
                        
                        glPushMatrix();
                        vect_f pivot = rot->_.angle.pivot;
                        glTranslatef(pivot.x, pivot.y, 0.0);
                        glRotatef(RAD_DEG(tile_angle(t)), 0.0, 0.0, 1.0);
                        glTranslatef(-pivot.x, -pivot.y, 0.0);
                }
                
                /* Switch blending function if necessary. */
                unsigned newblend = blendmode_would_change(t);
                if (newblend) {
                        if (num_undrawn > 0) {
                                draw_tile_buf(num_undrawn);
                                num_undrawn = 0;
                        }
                        if (blendmode_set(newblend, 0) != 0) {
                                /* Extra pass necessary. */
                                assert(num_undrawn == 0);
                                prepare_tile_buf(t, huge_buf);
                                draw_tile_buf(1);
                                blendmode_set(newblend, 1);
                        }
                }
                
                /* Put tile vertex data into buffer. */
                prepare_tile_buf(t, &huge_buf[num_undrawn * VERT_SPACE * 4]);
                num_undrawn++;
                
                if (rot != NULL) {
                        draw_tile_buf(num_undrawn);
                        num_undrawn = 0;
                        glPopMatrix();
                }
        }
        
        /* Finish off any undrawn tiles. */
        if (num_undrawn > 0)
                draw_tile_buf(num_undrawn);
        
        glPopMatrix();
}

int height_offset = 0;

static void
render_start(Camera *cam, BB *visible_area)
{
        /* Camera viewport. */
#if PLATFORM_IOS
        if (!config.flip) {
                glViewport(cam->viewport.t, cam->viewport.l,
                           cam->viewport.b - cam->viewport.t,	/* width */
                           cam->viewport.r - cam->viewport.l);	/* height */
        } else {
                glViewport(config.screen_height - cam->viewport.b,
                           config.screen_width - cam->viewport.r,
                           cam->viewport.b - cam->viewport.t,	/* width */
                           cam->viewport.r - cam->viewport.l);	/* height */
        }
#else
        glViewport(cam->viewport.l,
		   config.screen_height - cam->viewport.b + height_offset,
                   cam->viewport.r - cam->viewport.l,	/* width */
                   cam->viewport.b - cam->viewport.t);	/* height */
#endif
        /* Visible area projection. */
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        vect_i visible_size = {
                ceilf(cam->size.x/cam->zoom),
                ceilf(cam->size.y/cam->zoom)
        };
        vect_i visible_halfsize = {visible_size.x/2, visible_size.y/2};
#ifndef NDEBUG
        extern int outsideView;
        if (outsideView) {
                visible_halfsize = visible_size;
                visible_size.x *= 2;
                visible_size.y *= 2;
        }
#endif
        
#if PLATFORM_IOS
        if (config.screen_width > 500) {
                glOrthof(-visible_halfsize.y, visible_halfsize.y,
                         -visible_halfsize.x, visible_halfsize.x, 0.0, 1.0);
        } else {
                /*
                 * XXX
                 *
                 * For some reason on 480x320 device (where the world
                 * is twice as large as screen), we must substract 0.5
                 * from visible size components to prevent seams from 
                 * showing up everywhere.
                 *
                 * I thought I understood why this was so, but then I
                 * cannot explain why it's not necessary for when the
                 * world size matches screen size.
                 */
                glOrthof(-visible_halfsize.y-0.5, visible_halfsize.y-0.5,
                         -visible_halfsize.x-0.5, visible_halfsize.x-0.5, 0.0,
                         1.0);
        }
#else
        glOrtho(-visible_halfsize.x, visible_halfsize.x,
                -visible_halfsize.y, visible_halfsize.y, 0.0, 1.0);
#endif
        glMatrixMode(GL_MODELVIEW);
        
        /* Transform matrix according to camera position. */
        glPushMatrix();
        vect_f cam_pos = body_pos(&cam->body);
        int cam_x = posround(cam_pos.x);
        int cam_y = posround(cam_pos.y);
#if !ALL_SMOOTH
        if (cam->body.flags & BODY_SMOOTH_POS)
                glTranslatef(-cam_pos.x, -cam_pos.y, 0.0);
        else
                glTranslatef(-cam_x, -cam_y, 0.0);
#else
        glTranslatef(-cam_pos.x, -cam_pos.y, 0.0);
#endif  /* ALL_SMOOTH */
        /* Visible area bounding box. */
        *visible_area = (BB){
                .l=cam_x - visible_halfsize.x,
                .r=cam_x + visible_halfsize.x,
                .b=cam_y - visible_halfsize.y,
                .t=cam_y + visible_halfsize.y
        };
}

static inline void
render_end()
{
        glPopMatrix();
}

#ifndef NDEBUG

/*
 * Disable texturing before calling draw_bb() and re-enable it afterwards:
 *
 *      glDisable(GL_TEXTURE_2D);
 *      glDisableClientState(GL_TEXTURE_COORD_ARRAY);
 *      draw_bb(...);
 *      glEnableClientState(GL_TEXTURE_COORD_ARRAY);
 *      glEnable(GL_TEXTURE_2D);
 */
static void
draw_bb(BB bb, uint32_t color)
{        
        GLfloat vertex_array[] = {
                bb.l, bb.b,
                bb.l, bb.t,
                bb.r, bb.t,
                bb.r, bb.b,
        };
        uint32_t color_array[] = {color, color, color, color};
        
        glVertexPointer(2, GL_FLOAT, 0, vertex_array);
        glColorPointer(4, GL_UNSIGNED_BYTE, 0, color_array);
        glDrawArrays(GL_LINE_LOOP, 0, 4);        
}

/*
 * Draw shape grid bounding boxes.
 */
static void
draw_shape_BBs(World *world, BB visible_area)
{
        /* Look up visible shapes. */
        void *visible_shapes[500];
        unsigned num_shapes = grid_lookup(&world->grid, visible_area,
                                          visible_shapes,
                                          ARRAYSZ(visible_shapes), NULL);        
        if (num_shapes == 0)
                return;
                
        /* Draw shape grid bounding boxes if they are stored inside the grid. */
        uint32_t color = color_32bit(1.0, 1.0, 0.0, 1.0);
        for (unsigned i = 0; i < num_shapes; i++) {
                Shape *s = visible_shapes[i];
                if (grid_stored(&s->go))
                        draw_bb(s->go.area, color);
        }        
}

#if ENABLE_TILE_GRID

/*
 * Draw tile grid bounding boxes.
 */
static void
draw_tile_BBs(Camera *cam, BB visible_area)
{
        World *world = cam->body.world;
        
        /* Look up visible tiles. */
        void *visible_tiles[1500];
        unsigned max_tiles = ARRAYSZ(visible_tiles);
        unsigned num_tiles = grid_lookup(&world->grid, visible_area,
                                         visible_tiles, max_tiles, tile_filter);
        /*
         * Add camera tiles to the list, since those are always visible and not
         * within the grid.
         */
        Tile *t;
        DL_FOREACH(cam->body.tiles, t) {
                assert(num_tiles < max_tiles);
                visible_tiles[num_tiles++] = t;
        }        
        if (num_tiles == 0)
                return; /* Nothing to draw. */
        
        uint32_t color = color_32bit(0.0, 1.0, 1.0, 1.0);
        for (unsigned i = 0; i < num_tiles; i++) {
                t = visible_tiles[i];
                if (grid_stored(&t->go))
                        draw_bb(t->go.area, color);
        }
}

#endif  /* ENABLE_TILE_GRID */

static void
prepare_shape_buf(Shape *s, unsigned char *buf)
{
        assert(buf && s && s->shape_type == SHAPE_RECTANGLE);
        
        /* Put color values into buffer. */
        uint32_t color;
        if (s->flags & SHAPE_INTERSECT)
                color = color_32bit(1.0, 0.0, 0.0, 1.0);
        else
                color = s->color;
        *((uint32_t *)&buf[VERT_SPACE*0 + VERT_COLOR_OFFSET]) = color;
        *((uint32_t *)&buf[VERT_SPACE*1 + VERT_COLOR_OFFSET]) = color;
        *((uint32_t *)&buf[VERT_SPACE*2 + VERT_COLOR_OFFSET]) = color;
        *((uint32_t *)&buf[VERT_SPACE*3 + VERT_COLOR_OFFSET]) = color;
        
        /* Dump shape vertex coordinates into buffer. */
        assert(s->shape_type == SHAPE_RECTANGLE);
        BB rect = shape_def(s).rect;
        *((GLfloat *)&buf[VERT_SPACE*0 + VERT_COORD_OFFSET]    ) = rect.l;
        *((GLfloat *)&buf[VERT_SPACE*0 + VERT_COORD_OFFSET] + 1) = rect.b;
        *((GLfloat *)&buf[VERT_SPACE*1 + VERT_COORD_OFFSET]    ) = rect.r;
        *((GLfloat *)&buf[VERT_SPACE*1 + VERT_COORD_OFFSET] + 1) = rect.b;
        *((GLfloat *)&buf[VERT_SPACE*2 + VERT_COORD_OFFSET]    ) = rect.r;
        *((GLfloat *)&buf[VERT_SPACE*2 + VERT_COORD_OFFSET] + 1) = rect.t;
        *((GLfloat *)&buf[VERT_SPACE*3 + VERT_COORD_OFFSET]    ) = rect.l;
        *((GLfloat *)&buf[VERT_SPACE*3 + VERT_COORD_OFFSET] + 1) = rect.t;
}

static void
draw_visible_shapes(World *world, BB visible_area)
{
        /* Look up visible shapes. */
        void *visible_shapes[2000];
        unsigned num_shapes = grid_lookup(&world->grid, visible_area,
                                          visible_shapes,
                                          ARRAYSZ(visible_shapes),
                                          shape_filter);
        if (num_shapes == 0)
                return;
        
        unsigned char buf[VERT_SPACE * 4]; /* Storage for shape verex data. */
        glVertexPointer(2, GL_FLOAT, VERT_SPACE, buf + VERT_COORD_OFFSET);
        glColorPointer(4, GL_UNSIGNED_BYTE, VERT_SPACE,
                       buf + VERT_COLOR_OFFSET);
        
        /* Set body of first shape as current_body. */
        Shape *first_shape = visible_shapes[0];
        assert(first_shape && first_shape->objtype == OBJTYPE_SHAPE);
        Body *current_body = first_shape->body;
        
        /* Prepare modelview matrix for first body. */
        glPushMatrix();
        body_translation(current_body);
        
        /* Draw visible shapes. */
        for (unsigned i = 0; i < num_shapes; i++) {
                /* Update matrix if necessary. */
                Shape *s = visible_shapes[i];
                if (current_body != s->body) {
                        current_body = s->body;
                        assert(current_body);
                        
                        /* Set up matrix for the new body. */
                        glPopMatrix();
                        glPushMatrix();
                        body_translation(current_body);
                }
                
                prepare_shape_buf(s, buf);
                glDrawArrays(GL_LINE_LOOP, 0, 4);
        }        
        glPopMatrix();
}

/*
 * Disable texturing before calling draw_point() and re-enable it afterwards:
 *
 *      glDisable(GL_TEXTURE_2D);
 *      glDisableClientState(GL_TEXTURE_COORD_ARRAY);
 *      draw_point(...);
 *      glEnableClientState(GL_TEXTURE_COORD_ARRAY);
 *      glEnable(GL_TEXTURE_2D);
 */
static void
draw_point(vect_f pos, uint32_t color)
{        
        GLfloat vertex_array[] = {pos.x, pos.y};
        uint32_t color_array[] = {color, color, color, color};
        
        glVertexPointer(2, GL_FLOAT, 0, &vertex_array);
        glColorPointer(4, GL_UNSIGNED_BYTE, 0, color_array);
        glDrawArrays(GL_POINTS, 0, 1);        
}

static void
draw_line(vect_i start, vect_i end, uint32_t color)
{        
        GLfloat vertex_array[] = {start.x, start.y, end.x, end.y};
        uint32_t color_array[] = {color, color, color, color};
        
        glVertexPointer(2, GL_FLOAT, 0, &vertex_array);
        glColorPointer(4, GL_UNSIGNED_BYTE, 0, color_array);
        glDrawArrays(GL_LINES, 0, 2);        
}

static void
draw_axes(void)
{
        /* X axis */
        uint32_t color = color_32bit(1.0, 0.0, 0.0, 1.0);
        draw_line((vect_i){-100, 0}, (vect_i){100, 0}, color);
        
        /* Y axis */
        color = color_32bit(0.0, 1.0, 0.0, 1.0);
        draw_line((vect_i){0, -100}, (vect_i){0, 100}, color);
}

static void
draw_body(Body *b)
{
        /* Draw point at body position. */
        vect_f pos = body_pos(b);
        draw_point(pos, color_32bit(1.0, 0.0, 0.0, 1.0));
        
        /* Draw children recursively. */
        Body *child;
        DL_FOREACH(b->children, child) {
                glPushMatrix();
                {
                        glTranslatef(pos.x, pos.y, 0.0);
                        draw_body(child);
                }
                glPopMatrix();
        }
}

static void
draw_grid(Grid *grid, BB bb)
{
        /* Calculate cell index ranges. */
        int size = grid->size;
        BB cells = grid->cells;
        BB lookcells = {
                .l=(bb.l < 0) ? -((-bb.l-1)/size)-1 : bb.l/size,
                .r=(bb.r <= 0) ? -(-bb.r/size)-1 : (bb.r-1)/size,
                .b=(bb.b < 0) ? -((-bb.b-1)/size)-1 : bb.b/size,
                .t=(bb.t <= 0) ? -(-bb.t/size)-1 : (bb.t-1)/size
        };
        assert(lookcells.r >= lookcells.l && lookcells.t >= lookcells.b);
        
        /*
         * Clamp visible cells to actual existing cells.
         * This is necessary in case user provided area falls outside of grid
         * area.
         */
        if (lookcells.l < cells.l)
                lookcells.l = cells.l;
        if (lookcells.r > cells.r)
                lookcells.r = cells.r;
        if (lookcells.b < cells.b)
                lookcells.b = cells.b;
        if (lookcells.t > cells.t)
                lookcells.t = cells.t;
        
        int cols = grid->cols;
        for (int y = lookcells.b; y <= lookcells.t; y++) {
                for (int x = lookcells.l; x <= lookcells.r; x++) {
                        int index = (x - cells.l) + (y - cells.b) * cols;
                        float num_obj = grid->cellstat[index].current;
                        if (num_obj == 0.0)
                                continue;
                        
                        /* Draw cell. */
                        bb = (BB) {
                                .l=x*size+2, .r=(x+1)*size-2,
                                .b=y*size+2, .t=(y+1)*size-2
                        };
                        float red = CLAMP(num_obj/config.grid_many, 0.0, 1.0);
                        float green = 1.0 - red;
                        uint32_t c = color_32bit(red, green, 0.0, 0.4);
                        draw_quad(bb, c);
                }
        }
}

void
render_debug(Camera *cam)
{
        /* Setup viewport and camera matrix. */
        BB visible_area;
        render_start(cam, &visible_area);
        
        /* Disable texturing, blending, and reset color mask. */
        glDisable(GL_TEXTURE_2D);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisable(GL_BLEND);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        {        
                extern int drawShapes;
                if (drawShapes)
                        draw_visible_shapes(cam->body.world, visible_area);
                
                extern int drawShapeBBs;
                if (drawShapeBBs)
                        draw_shape_BBs(cam->body.world, visible_area);
#if ENABLE_TILE_GRID
                extern int drawTileBBs;
                if (drawTileBBs)
                        draw_tile_BBs(cam, visible_area);
#endif
                extern int drawBodies;
                if (drawBodies) {
                        glPointSize(3.0);
                        draw_body(&cam->body.world->static_body);
                        draw_body(&cam->body);
                }
                
                extern int drawGrid;
                if (drawGrid)
                        draw_grid(&cam->body.world->grid, visible_area);
                
                if (drawShapes || drawBodies || drawTileBBs || drawShapeBBs)
                        draw_axes();
        }
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        blend_mode_current = UINT_MAX;  /* Force blend mode reset. */
        
        /* Remove camera transform from matrix stack. */
        render_end();
}
#endif  /* NDEBUG */

static void
draw_camera_background(Camera *cam, BB visible_area)
{
        uint32_t bg_color = cam_color(cam);
        if (COLOR_ALPHA(bg_color) == 0.0)
                return;         /* Invisible. */
                
        /* Use standard blending and force blend mode reset later. */
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        blend_mode_current = UINT_MAX;

        /* Disable texturing, draw the quad, and then re-enable texturing. */
        glDisable(GL_TEXTURE_2D);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        draw_quad(visible_area, bg_color);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glEnable(GL_TEXTURE_2D);
}

void
render(Camera *cam)
{
        BB visible_area;
        render_start(cam, &visible_area);
        {
                /* Draw camera background quad and all visible tiles. */
                draw_camera_background(cam, visible_area);
                draw_visible_tiles(cam, visible_area);
        }
        render_end();
}

void
render_to_framebuffer(Camera *cam)
{
        glViewport(cam->viewport.l, cam->viewport.t,
                   cam->viewport.r - cam->viewport.l,	/* width */
                   cam->viewport.b - cam->viewport.t);	/* height */
        
        /*
         * Visible area projection.
         * Note that the Y-coords are flipped for framebuffer.
         */
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        vect_i visible_size = {
                ceilf(cam->size.x/cam->zoom),
                ceilf(cam->size.y/cam->zoom)
        };
        vect_i visible_halfsize = {visible_size.x/2, visible_size.y/2};
#if PLATFORM_IOS
        if (config.screen_width > 500) {
                glOrthof(-visible_halfsize.x,  visible_halfsize.x,
                         visible_halfsize.y, -visible_halfsize.y, 0.0, 1.0);
        } else {
                /*
                 * XXX
                 *
                 * For some reason on 480x320 device (where the world
                 * is twice as large as screen), we must substract 0.5
                 * from visible size components to prevent seams from 
                 * showing up everywhere.
                 *
                 * I thought I understood why this was so, but then I
                 * cannot explain why it's not necessary for when the
                 * world size matches screen size.
                 */
                glOrthof(-visible_halfsize.x-0.5,  visible_halfsize.x-0.5,
                          visible_halfsize.y-0.5, -visible_halfsize.y-0.5, 0.0,
                         1.0);
        }
#else
        glOrtho(-visible_halfsize.x,  visible_halfsize.x,
                 visible_halfsize.y, -visible_halfsize.y, 0.0, 1.0);
#endif
        
        glMatrixMode(GL_MODELVIEW);
        
        /* Transform matrix according to camera position. */
        glPushMatrix();
#if PLATFORM_IOS
        glLoadIdentity();       /* No device rotation for framebuffer. */
#endif
        vect_f cam_pos = body_pos(&cam->body);
        int cam_x = posround(cam_pos.x);
        int cam_y = posround(cam_pos.y);
#if !ALL_SMOOTH
        if (cam->body.flags & BODY_SMOOTH_POS)
                glTranslatef(-cam_pos.x, -cam_pos.y, 0.0);
        else
                glTranslatef(-cam_x, -cam_y, 0.0);
#else
        glTranslatef(-cam_pos.x, -cam_pos.y, 0.0);
#endif  /* ALL_SMOOTH */
        /* Visible area bounding box. */
        BB visible_area = {
                .l=cam_x - visible_halfsize.x,
                .r=cam_x + visible_halfsize.x,
                .b=cam_y - visible_halfsize.y,
                .t=cam_y + visible_halfsize.y
        };
        
        /* Draw camera background quad and all visible tiles. */
        draw_camera_background(cam, visible_area);
        draw_visible_tiles(cam, visible_area);
        
        render_end();
}

void
render_clear(void)
{
        glClear(GL_COLOR_BUFFER_BIT);   /* Clear framebuffer. */
}
