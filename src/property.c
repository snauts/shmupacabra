#include <assert.h>
#include <stdlib.h>
#include "property.h"
#include "log.h"
#include "mem.h"

void
prop_free(Property *p)
{
        assert(p->refc);
        if (--(p->refc) > 0)
                return;
                
        extern mem_pool mp_property;
        mp_free(&mp_property, p);
}

Property *
prop_new(void)
{
        extern mem_pool mp_property;
        Property *p = mp_alloc(&mp_property);
        p->refc = 1;
        return p;
}

uint32_t
color_32bit(float r, float g, float b, float a)
{
        r = CLAMP(r, 0.0, 1.0);
        g = CLAMP(g, 0.0, 1.0);
        b = CLAMP(b, 0.0, 1.0);
        a = CLAMP(a, 0.0, 1.0);
        
        return ((uint32_t)(a*255) << 24) |
            ((uint32_t)(b*255) << 16) |
            ((uint32_t)(g*255) << 8) |
            ((uint32_t)(r*255) << 0);
}

/*
 * Interpolate color value.
 */
int
interp_color(uint8_t type, uint32_t start, uint32_t end, float duration,
             float delta, uint32_t *val)
{
        /* If animation has not started yet, return start value. */
        if (delta <= 0.0) {
                *val = start;
                return 0;
        }
        
        /* Extract components. */
        float r = COLOR_RED(start);
        float g = COLOR_GREEN(start);
        float b = COLOR_BLUE(start);
        float a = COLOR_ALPHA(start);
        float r_end = COLOR_RED(end);
        float g_end = COLOR_GREEN(end);
        float b_end = COLOR_BLUE(end);
        float a_end = COLOR_ALPHA(end);
        
        /* Calculate current value. */
        switch (type) {
        case ANIM_LOOP: {
                delta = fmod(delta, duration);
                *val = color_32bit(interp_linear(r, r_end, duration, delta),
                                   interp_linear(g, g_end, duration, delta),
                                   interp_linear(b, b_end, duration, delta),
                                   interp_linear(a, a_end, duration, delta));
                return 0;
        }
        case ANIM_CLAMP: {
                if (delta >= duration) {
                        *val = end;
                        return 1;
                }
                *val = color_32bit(interp_linear(r, r_end, duration, delta),
                                   interp_linear(g, g_end, duration, delta),
                                   interp_linear(b, b_end, duration, delta),
                                   interp_linear(a, a_end, duration, delta));
                return 0;
        }
        case ANIM_CLAMP_EASEIN: {
                if (delta >= duration) {
                        *val = end;
                        return 1;
                }
                *val = color_32bit(interp_easein(r, r_end, duration, delta),
                                   interp_easein(g, g_end, duration, delta),
                                   interp_easein(b, b_end, duration, delta),
                                   interp_easein(a, a_end, duration, delta));
                return 0;
        }
        case ANIM_CLAMP_EASEOUT: {
                if (delta >= duration) {
                        *val = end;
                        return 1;
                }
                *val = color_32bit(interp_easeout(r, r_end, duration, delta),
                                   interp_easeout(g, g_end, duration, delta),
                                   interp_easeout(b, b_end, duration, delta),
                                   interp_easeout(a, a_end, duration, delta));
                return 0;
        }
        case ANIM_CLAMP_EASEINOUT: {
                if (delta >= duration) {
                        *val = end;
                        return 1;
                }
                *val = color_32bit(interp_easeinout(r, r_end, duration, delta),
                                   interp_easeinout(g, g_end, duration, delta),
                                   interp_easeinout(b, b_end, duration, delta),
                                   interp_easeinout(a, a_end, duration, delta));
                return 0;
        }
        case ANIM_REVERSE_LOOP: {
                if ((delta = fmod(delta, duration * 2)) > duration)
                        delta = duration * 2 - delta;
                *val = color_32bit(interp_linear(r, r_end, duration, delta),
                                   interp_linear(g, g_end, duration, delta),
                                   interp_linear(b, b_end, duration, delta),
                                   interp_linear(a, a_end, duration, delta));
                return 0;
        }
        case ANIM_REVERSE_CLAMP: {
                if (delta >= duration * 2) {
                        *val = start;
                        return 1;
                }
                if (delta > duration)
                        delta = duration * 2 - delta;
                *val = color_32bit(interp_linear(r, r_end, duration, delta),
                                   interp_linear(g, g_end, duration, delta),
                                   interp_linear(b, b_end, duration, delta),
                                   interp_linear(a, a_end, duration, delta));
                return 0;
        }
        }
        fatal_error("Invalid animation type: (%i).", type);
        abort();
}
