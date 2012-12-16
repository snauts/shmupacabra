#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "geometry.h"
#include "log.h"
#include "mem.h"
#include "misc.h"
#include "spritelist.h"
#include "texture.h"
#include "uthash_tuned.h"

SpriteList *
spritelist_new(Texture *tex, const TexFrag *frames, unsigned num_frames)
{
        /* See if a sprite list with the same exact frames already exists. */
        assert(tex && frames && num_frames > 0);
        SpriteList *s;
        unsigned framebuf_sz = num_frames * sizeof(TexFrag);
        HASH_FIND(hh, tex->sprites, frames, framebuf_sz, s);
        if (s != NULL)
                return s; /* Found it! */

        /* Allocate and initialize. */
        extern mem_pool mp_sprite;
        s = mp_alloc(&mp_sprite);
        s->objtype = OBJTYPE_SPRITELIST;
        s->tex = tex;
        
        /* Allocate space for sprites and copy them from argument buffer. */
        s->num_frames = num_frames;
        s->frames = mem_alloc(framebuf_sz, "Sprites");
        memcpy(s->frames, frames, framebuf_sz);
        
        /* Add sprite-list to hash. */
        HASH_ADD_KEYPTR(hh, tex->sprites, s->frames, framebuf_sz, s);
#if 0
        /* Create VBO data for this sprite-list. */
        glBindBuffer(GL_ARRAY_BUFFER_ARB, tex->vbo_id);
        s->vbo_offset = tex->vbo_offset;
        for (unsigned i = 0; i < num_frames; i++) {
                const TexFrag *f = &frames[i];
                GLshort data[] = {
                        f->l, f->b,
                        f->r, f->b,
                        f->l, f->t,
                        f->r, f->t
                };
                assert(tex->vbo_offset + sizeof(data) < TEXCOORD_VBO_SIZE);
                glBufferSubData(GL_ARRAY_BUFFER_ARB, tex->vbo_offset, 
                                sizeof(data), data);
                tex->vbo_offset += sizeof(data);
        }
        glBindBuffer(GL_ARRAY_BUFFER_ARB, 0);
#endif
        return s;
}

void
spritelist_free(SpriteList *s)
{
        assert((s->num_frames == 0 && s->frames == NULL) ||
               (s->num_frames > 0 && s->frames != NULL));
        
        /* Release frame memory. */
        mem_free(s->frames);
        
        /* Free sprite-list memory. */
        extern mem_pool mp_sprite;
        mp_free(&mp_sprite, s);
}

vect_i
texfrag_size(const TexFrag *tf)
{
#if PLATFORM_IOS
        int f = (config.screen_width > 500) ? 1 : 2;
        return (vect_i){abs(tf->r - tf->l) * f, abs(tf->b - tf->t) * f};
#else
        return (vect_i){abs(tf->r - tf->l), abs(tf->b - tf->t)};
#endif
}

vect_f
texfrag_sizef(const TexFrag *tf)
{
#if PLATFORM_IOS
        int f = (config.screen_width > 500) ? 1 : 2;
        return (vect_f){abs(tf->r - tf->l) * f, abs(tf->b - tf->t) * f};
#else
        return (vect_f){abs(tf->r - tf->l), abs(tf->b - tf->t)};
#endif
}

vect_i
texfrag_maxsize(const TexFrag *array, unsigned num_frags)
{
        vect_i maxsz = {0.0, 0.0};
        for (unsigned i = 0; i < num_frags; i++) {
                vect_i size = texfrag_size(&array[i]);
                if (size.x > maxsz.x)
                        maxsz.x = size.x;
                if (size.y > maxsz.y)
                        maxsz.y = size.y;
        }
        return maxsz;
}
