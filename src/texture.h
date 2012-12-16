#ifndef GAME2D_TEXTURE_H
#define GAME2D_TEXTURE_H

#include "OpenGL_include.h"
#include <SDL_image.h>
#include "common.h"
#include "uthash_tuned.h"

struct SpriteList_t;

/* Determines how long unused textures stay in memory. */
#define TEXTURE_HISTORY         5

enum TextureFlags {
        TEXFLAG_FILTER     = (1 << 0),
        TEXFLAG_INTENSITY  = (1 << 1)
};

/*
 * Images are loaded as OpenGL textures. As such, both their width and height
 * must be numbers that are powers of two. If an image does not have power of
 * two dimensions, its buffer is extended to the smallest possible enclosing
 * power of two size.
 */
typedef struct Texture_t {
        GLuint   id;            /* OpenGL texture ID. */
        char     name[128];     /* Texture name = hash key. */
        unsigned w, h;          /* Image width and height in pixels. */
        unsigned pow_w, pow_h;  /* Power of two extended widht & height. */
        unsigned flags;
        int      usage;         /* Determines how recent was last use. */
        struct SpriteList_t *sprites;   /* Hash of sprite lists. */
#if 0
        GLuint   vbo_id;        /* Texcoord vertex buffer ID. */
        unsigned vbo_offset;    /* Current offset into VBO. */
#endif
        UT_hash_handle hh;      /* We hash textures by name. */
} Texture;

int      texture_is_loaded(const char *name, unsigned flags);
Texture *texture_load(const char *name, unsigned flags);
Texture *texture_preload(const char *, unsigned, const void *, unsigned);
Texture *texture_preload_surface(const char *, unsigned, SDL_Surface *);
Texture *texture_load_blank(const char *name, unsigned flags);

void     texture_free_unused(void);
void     texture_bind(Texture *tex);
void     texture_bind_id(unsigned texid);
int      texture_would_change(Texture *tex);
void     texture_set_size(Texture *tex, unsigned width, unsigned height);
void     texture_limit(unsigned MB);

#endif  /* GAME2D_TEXTURE_H */
