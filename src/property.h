#ifndef GAME2D_PROPERTY_H
#define GAME2D_PROPERTY_H

#include <stdint.h>
#include "common.h"
#include "geometry.h"

enum {
        ANIM_NONE       = 0,    /* No animation, static value. */
        ANIM_CLAMP,             /* Linear animation, stops once it reaches its
                                   end. */
        ANIM_CLAMP_EASEIN,      /* Quadratic ease-in. */
        ANIM_CLAMP_EASEOUT,     /* Quadratic ease-out. */
        ANIM_CLAMP_EASEINOUT,   /* Quadratic ease-in and ease-out. */
        ANIM_LOOP,              /* Linear looping animation. */
        ANIM_REVERSE_LOOP,      /* Linear looping animation, reverses once it
                                   reaches its end. */
        ANIM_REVERSE_CLAMP      /* Linear animation, reverses once then stops.*/
};

/*
 * A union of various values that can be animated.
 *
 * refc         Reference count (used to avoid too many copies of Property
 *              structures).
 * anim_type    Animation type.
 * start_time   Start time (seconds since some event).
 * duration     Seconds it takes to get from `start` to `end` value.
 */
typedef struct {
        uint16_t        refc;
        uint8_t         anim_type;
        float           start_time;
        float           duration;
        
        /* Values for the various properties. */
        union {
                struct {
                        unsigned start, end;
                } frame;
                struct {
                        uint32_t start, end;
                } color;
                struct {
                        vect_f pivot;
                        float start, end;
                } angle;
                struct {
                        vect_f start, end;
                } vectf;
                struct {
                        ShapeDef start, end;
                } shapedef;                
        } _;
} Property;

void      prop_free(Property *p);
Property *prop_new(void);
#define   prop_copy(p) ((p)->refc++, (p))

/* typedef float (*interp_func)(float, float, float, float); */

static inline float
interp_linear(float start, float end, float duration, float delta)
{
        return start + (end - start) * delta / duration;
}

static inline float
interp_easein(float start, float end, float duration, float delta)
{
        delta /= duration;
        return start + (end - start) * delta * delta;
}

static inline float
interp_easeout(float start, float end, float duration, float delta)
{
        delta /= duration;
        return start + (end - start) * delta * (2.0 - delta);
}

static inline float
interp_easeinout(float start, float end, float duration, float delta)
{
        delta /= (duration / 2.0);
        if (delta < 1.0)
                return start + (end - start) * delta * delta / 2.0;
        delta -= 1.0;
        return start + (end - start) * (delta * (2.0 - delta) + 1.0) / 2.0;
}

uint32_t        color_32bit(float r, float g, float b, float a);
#define COLOR_RED(x)   ((((x) & 0x000000FF)      ) / 255.0)
#define COLOR_GREEN(x) ((((x) & 0x0000FF00) >> 8 ) / 255.0)
#define COLOR_BLUE(x)  ((((x) & 0x00FF0000) >> 16) / 255.0)
#define COLOR_ALPHA(x) ((((x) & 0xFF000000) >> 24) / 255.0)

int             interp_color(uint8_t type, uint32_t start, uint32_t end,
                             float duration, float delta, uint32_t *val);
#endif
