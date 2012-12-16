#ifndef GAME2D_CAMERA_H
#define GAME2D_CAMERA_H

#include "body.h"
#include "event.h"
#include "geometry.h"
#include "property.h"

/*
 * Camera (i.e., viewing & projection transform).
 *
 * body                 Body object determines camera position in the world.
 * bg_color             Background color property.
 * size                 Width and height of physical world area that camera
 *                      sees.
 * zoom                 Scale of visible area.
 * box                  If camera body is within this box, do not show anything
 *                      that's outside.
 * sort                 Determines camera sort order. Those with smaller "sort"
 *                      values will be rendered first.
 * disable              If true, do not render camera view.
 *
 * touchdown_bind       Function bound to touch-down event.
 * touchup_bind         Function bound to touch-up event.
 * touchmove_bind       Function bound to touch-move event.
 */
typedef struct Camera_t {
        int             objtype;        /* = OBJTYPE_CAMERA */
        
        Body            body;
        Property        *bg_color;
        vect_i          size;
        float           zoom;
        BB              viewport;
        BB              box;
        int             sort;
        int             disabled;
        
#if ENABLE_TOUCH
        EventFunc       touchdown_bind;
        EventFunc       touchup_bind;
        EventFunc       touchmove_bind;
#endif
#if ENABLE_MOUSE
        EventFunc       mouseclick_bind;
        EventFunc       mousemove_bind;
#endif
        
        struct Camera_t *prev, *next;  /* For use in global camera list. */
} Camera;

int      cmp_cameras(const Camera *lh, const Camera *rh);

Camera  *cam_new(struct World_t *world, vect_i size, BB viewport);
void     cam_free(Camera *cam);
void     cam_set_pos(Camera *cam, vect_f pos);

uint32_t cam_color(Camera *cam);
void     cam_set_color(Camera *cam, uint32_t color);
void     cam_anim_color(Camera *cam, uint8_t type, uint32_t end, float duration, float start_time);

#endif
