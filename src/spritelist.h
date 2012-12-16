#ifndef GAME2D_SPRITELIST_H
#define GAME2D_SPRITELIST_H

#include "OpenGL_include.h"
#include "uthash_tuned.h"

struct Texture_t;

/*
 * Texture fragment.
 */
typedef struct TexFrag_t {
        GLshort l, r, b, t;
} TexFrag;

vect_i           texfrag_size(const TexFrag *tf);
vect_f           texfrag_sizef(const TexFrag *tf);
vect_i           texfrag_maxsize(const TexFrag *array, unsigned num_frags);

/*
 * A sprite list is a sequence of images. "Image" meaning rectangular piece of
 * an OpenGL texture. A sequence of images can be used for creating animation,
 * but timing and anything else related to selecting the current image is not
 * part of a SpriteList structure.
 *
 * Each TexFrag structure in the frames list specifies a rectangular texture
 * fragment.
 */
typedef struct SpriteList_t {
        int                     objtype;       /* = OBJTYPE_SPRITELIST */
        struct Texture_t        *tex;          /* Owned by texture. */
        unsigned                num_frames;    /* Number of frames. */
        struct TexFrag_t        *frames;       /* List of texture fragments. */
        UT_hash_handle          hh;            /* Makes this struct hashable. */
#if 0
        unsigned                vbo_offset;    /* Offset into texcoord VBO. */
#endif
} SpriteList;

SpriteList      *spritelist_new(struct Texture_t *tex, const TexFrag *frames,
                                unsigned num_frames);
void             spritelist_free(SpriteList *s);

#endif
