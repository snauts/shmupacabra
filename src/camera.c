#include <assert.h>
#include <string.h>
#include "camera.h"
#include "common.h"
#include "event.h"
#include "misc.h"
#include "geometry.h"
#include "world.h"

/*
 * Camera comparison routine for qsotr() that compares the "sort" members of two
 * cameras.
 */
int
cmp_cameras(const Camera *lh, const Camera *rh)
{
        if (lh->sort < rh->sort)
                return -1;
        return (lh->sort > rh->sort);
}

Camera *
cam_new(World *world, vect_i size, BB viewport)
{
        extern mem_pool mp_camera;
        assert(size.x > 0 && size.y > 0);
        assert(viewport.r - viewport.l > 0 && viewport.b - viewport.t > 0);
        
        /* Allocate and initialize. */
        Camera *cam = mp_alloc(&mp_camera);
        cam->objtype = OBJTYPE_CAMERA;
        body_init(&cam->body, NULL, world, (vect_f){0.0, 0.0}, 0);
        cam->size = size;
        cam->zoom = 1.0;
        cam->viewport = viewport;
#ifndef NDEBUG
        /*
         * If debug camera was set but then destroyed, switch it over to the one
         * that was just created.
         */
        extern Camera *debug_cam;
        if (debug_cam != NULL && debug_cam->objtype != OBJTYPE_CAMERA)
                debug_cam = cam;
#endif  /* NDEBUG */
        return cam;
}

void
cam_free(Camera *cam)
{
        /* Destroy body. */
        body_destroy(&cam->body);
        
        /* Destroy properties. */
        if (cam->bg_color != NULL)
                prop_free(cam->bg_color);
        
        extern mem_pool mp_camera;
        mp_free(&mp_camera, cam);
}

uint32_t
cam_color(Camera *cam)
{
        Property *anim = cam->bg_color;
        if (anim == NULL)
                return 0x00000000;     /* Default color == transparent black. */
        
        /* If animation type is NONE, return the `start` value. */
        if (anim->anim_type == ANIM_NONE)
                return anim->_.color.start;
        
        uint32_t start = anim->_.color.start;
        uint32_t end = anim->_.color.end;
        
        /* Get current time, and time since animation start. */
        float now = cam->body.step * cam->body.world->step_sec;
        float delta = now - anim->start_time;
        float duration = anim->duration;
        
        uint32_t val;
        if (interp_color(anim->anim_type, start, end, duration, delta, &val))
                cam_set_color(cam, val);
        return val;
}

void
cam_set_color(Camera *cam, uint32_t color)
{
        /* If default value is chosen, destroy property. */
        if (color == 0x00000000) {
                if (cam->bg_color != NULL) {
                        prop_free(cam->bg_color);
                        cam->bg_color = NULL;
                }
                return;
        }

        /* Destroy and create property with new value. */
        if (cam->bg_color != NULL)
                prop_free(cam->bg_color);
        cam->bg_color = prop_new();
        cam->bg_color->_.color.start = color;
}

#define SET_ANIM(cam, member, prop, type, start_value, end_value, start_time, duration) \
do { \
        Property *anim = (cam)->member; \
        (anim)->_.prop.start = (start_value); \
        (anim)->_.prop.end = (end_value); \
        (anim)->anim_type = (type); \
        (anim)->start_time = (start_time) + (cam)->body.step * (cam)->body.world->step_sec; \
        (anim)->duration = (duration); \
} while (0)

void
cam_anim_color(Camera *cam, uint8_t type, uint32_t end, float duration,
               float start_time)
{
        /* Start value. */
        uint32_t start_value = cam_color(cam);
        
        /* Destroy previous animation and create a new one. */
        if (cam->bg_color != NULL)
                prop_free(cam->bg_color);
        cam->bg_color = prop_new();
        
        SET_ANIM(cam, bg_color, color, type, start_value, end, start_time,
                 duration);        
}

void
cam_set_pos(Camera *cam, vect_f pos)
{
        assert(cam);
        
        if (!bb_valid(cam->box)) {
                /* Bounding box is not set for camera. */
                body_set_pos(&cam->body, pos);
                return;
        }
        
        /*
         * Make it so that camera cannot see outside its box.
         */
        if (pos.x - cam->size.x/(2*cam->zoom) < cam->box.l)
                pos.x = cam->box.l + cam->size.x/(2*cam->zoom);
        else if (pos.x + cam->size.x/(2*cam->zoom) > cam->box.r)
                pos.x = cam->box.r - cam->size.x/(2*cam->zoom);
        if (pos.y - cam->size.y/(2*cam->zoom) < cam->box.b)
                pos.y = cam->box.b + cam->size.y/(2*cam->zoom);
        else if (pos.y + cam->size.y/(2*cam->zoom) > cam->box.t)
                pos.y = cam->box.t - cam->size.y/(2*cam->zoom);
        
        body_set_pos(&cam->body, pos);
}
