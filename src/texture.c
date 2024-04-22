#include <SDL_image.h>
#include "OpenGL_include.h"
#include <assert.h>
#include "config.h"
#include "mem.h"
#include "misc.h"
#include "log.h"
#include "spritelist.h"
#include "texture.h"
#include "utlist.h"

#define valid_texture(x)                                        \
        ((x) != NULL &&                                         \
         (x)->w <= (x)->pow_w &&                                \
         (x)->h <= (x)->pow_h &&                                \
         (x)->name[0] != '\0')                                  \

static Texture *texture_hash;   /* "texture name" -> (Texture *) */

/*
 * Keep track of which texture ID is currently bound. If 0, then texturing is
 * disabled.
 */
static unsigned bound_texture = 0;

static unsigned     loaded_size; /* Approx total size of loaded textures. */
static unsigned     loaded_max_size = 1024 * 1024 * 512; /* 512 MB */

static inline Texture *
texture_alloc(const char *fullname, unsigned flags)
{
        extern mem_pool mp_texture;
        Texture *tex = mp_alloc(&mp_texture);
        snprintf(tex->name, sizeof(tex->name), "%s", fullname);
        tex->flags = flags;
        return tex;
}


static inline void
texture_fullname(const char *name, unsigned flags, char *buf, unsigned bufsize)
{
        assert(name && *name);
        int filter = flags & TEXFLAG_FILTER;
        snprintf(buf, bufsize, filter ? "f=1;%s" : "%s" , name);
}

/*
 * Return true if texture is loaded into OpenGL.
 */
int
texture_is_loaded(const char *name, unsigned flags)
{
        char fullname[128];
        texture_fullname(name, flags, fullname, sizeof(fullname));

        Texture *tex;
        HASH_FIND_STR(texture_hash, fullname, tex);
        if (tex == NULL)
                return 0;
        return (tex->id != 0);
}

static void
texture_unload(Texture *tex)
{
        assert(valid_texture(tex) && tex->id != 0);
        glDeleteTextures(1, &tex->id);
        tex->id = 0;
        loaded_size -= (tex->pow_w * tex->pow_h * 4);
        tex->w = tex->pow_w = 0;
        tex->h = tex->pow_h = 0;
}

/*
 * Release any resources held inside texture struct, and then free the texture
 * struct itself.
 */
static void
texture_free(Texture *tex)
{
        assert(valid_texture(tex));

        /* Unload from OpenGL if it was loaded. */
        if (tex->id != 0)
                texture_unload(tex);

        /* Remove from texture hash. */
        HASH_DEL(texture_hash, tex);

        /* Free owned sprite-lists. */
        while (tex->sprites != NULL) {
                SpriteList *sprite_list = tex->sprites;
                HASH_DEL(tex->sprites, sprite_list);
                spritelist_free(sprite_list);
        }

        /* Free texture struct memory. */
        extern mem_pool mp_texture;
        mp_free(&mp_texture, tex);
}

/*
 * Free textures that have not been used in a while.
 */
void
texture_free_unused(void)
{
        Texture *tex, *tmp;

        HASH_ITER(hh, texture_hash, tex, tmp) {
                if (--tex->usage < 1)
                        texture_free(tex);
        }
}

/*
 * Free older textures to keep total texture memory size under
 * `loaded_max_size`.
 */
static void
texture_cleanup(void)
{
        if (loaded_size <= loaded_max_size)
                return;         /* Under limit. */

        /* Clear out some older textures. */
        Texture *tex, *tex_tmp;
        HASH_ITER(hh, texture_hash, tex, tex_tmp) {
                if (tex->id != 0) {
                        texture_unload(tex);
                        if (loaded_size <= loaded_max_size)
                                break;
                }
        }
}

void
texture_limit(unsigned MB)
{
        loaded_max_size = MB * 1024 * 1024;
        texture_cleanup();
}

/*
 * Users must not manually set texture width and height. This function should be
 * used instead.
 */
void
texture_set_size(Texture *tex, unsigned width, unsigned height)
{
        /* Texture size should be zero before this call. */
        assert(tex->w == 0 && tex->h == 0 && width > 0 && height > 0);
        assert(tex->pow_w == 0 && tex->pow_h == 0);
        tex->w = width;
        tex->h = height;
        tex->pow_w = nearest_pow2(width);
        tex->pow_h = nearest_pow2(height);
        assert(valid_texture(tex) && tex->id != 0);

        /* Cleanup if max size exceeded. */
        loaded_size += (tex->pow_w * tex->pow_h * 4);
        texture_cleanup();

        log_msg("Load `%s`   total: %.2f MB", tex->name,
                (float)loaded_size / 1024.0 / 1024.0);
}

static void
surface_to_texture(SDL_Surface *img, unsigned flags, unsigned *w, unsigned *h)
{
        assert(img->format->BytesPerPixel == 4);

        /* Store width and height as return values. */
        assert(w && h);
        *w = img->w;
        *h = img->h;
        unsigned pow_w = nearest_pow2(img->w);
        unsigned pow_h = nearest_pow2(img->h);

        /*
         * Create a blank texture with power-of-two dimensions. Then load
         * converted image data into its lower left.
         */
        GLint iformat = (flags & TEXFLAG_INTENSITY) ? GL_INTENSITY : GL_RGBA;
        glTexImage2D(GL_TEXTURE_2D, 0, iformat, pow_w, pow_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, img->w, img->h, GL_RGBA, GL_UNSIGNED_BYTE, img->pixels);

        if (flags & TEXFLAG_FILTER)
                glGenerateMipmap(GL_TEXTURE_2D);

        check_gl_errors();
}

static void
enable_texturing(void)
{
        assert(bound_texture == 0);
        glEnable(GL_TEXTURE_2D);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
}

static void
disable_texturing(void)
{
        assert(bound_texture != 0);
        glDisable(GL_TEXTURE_2D);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        bound_texture = 0;
}

static void
bind_texture_id(unsigned tex_id)
{
        assert(tex_id != 0);
        if (bound_texture == 0)
                enable_texturing();

        glBindTexture(GL_TEXTURE_2D, tex_id);
        bound_texture = tex_id;
}

static void
gen_and_bind(unsigned *id, int filter)
{
        assert(id && *id == 0);
        glGenTextures(1, id);
        bind_texture_id(*id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                        filter ? GL_LINEAR : GL_NEAREST);

        /* Min filter: for filtered textures use mipmaps. */
        GLint param = GL_NEAREST;
        if (filter) {
                param = GL_LINEAR_MIPMAP_LINEAR;
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, param);
        check_gl_errors();
}

/*
 * Return true if current texture binding would change if argument texture were
 * to be given to texture_bind().
 */
int
texture_would_change(Texture *tex)
{
        /*
         * If request to disable texturing, return true if texturing is already
         * disabled.
         */
        if (tex == NULL)
                return (bound_texture != 0);

        /* If texturing is disabled, then texturing would have to be enabled. */
        assert(valid_texture(tex));
        if (bound_texture == 0)
                return 1;

        /* Return true if texture IDs do not match. */
        return (bound_texture != tex->id);
}

void
texture_bind_id(unsigned texid)
{
        if (texid == 0) {
                if (bound_texture != 0)
                        disable_texturing();
                return;
        }
        if (bound_texture == texid)
                return;                 /* Already bound. */
        bind_texture_id(texid);
}

static void
update_matrix(Texture *tex)
{
        /* Update texture matrix. */
        glMatrixMode(GL_TEXTURE);
        glLoadIdentity();
        glScaled(1.0/tex->pow_w, 1.0/tex->pow_h, 1.0);
        glMatrixMode(GL_MODELVIEW);
}

void
texture_bind(Texture *tex)
{
        if (tex == NULL) {
                /* Disable texturing if not already disabled. */
                if (bound_texture != 0)
                        disable_texturing();
                return;
        }

        /* Do nothing if texturing enabled and texture already bound. */
        if (bound_texture != 0 && bound_texture == tex->id)
                return;

        /* See if texture is loaded into OpenGL. */
        if (tex->id == 0) {
                /* Remove "f=1;" from name if the texture is filtered. */
                int filter = tex->flags & TEXFLAG_FILTER;
                const char *img_name = filter ? &tex->name[4] : tex->name;
                SDL_Surface *img = IMG_Load(img_name);
                if (img == NULL)
                        return; /* Image could not be loaded. */

                /* Generate texture ID, bind it, set parameters. */
                gen_and_bind(&tex->id, filter);

                /* Read image data into OpenGL. */
                unsigned w, h;
                surface_to_texture(img, tex->flags, &w, &h);
                SDL_FreeSurface(img);

                /* Remove texture from hash and re-add at the end. */
                assert(valid_texture(tex));
                HASH_DEL(texture_hash, tex);
                HASH_ADD_STR(texture_hash, name, tex);

                texture_set_size(tex, w, h);
                update_matrix(tex);
                return;
        }

        assert(valid_texture(tex));
        bind_texture_id(tex->id);
        update_matrix(tex);
}

static Texture *
lookup_or_create(const char *img_name, unsigned flags)
{
        char fullname[128];
        texture_fullname(img_name, flags, fullname, sizeof(fullname));

        /* See if a texture with this name already exists. */
        Texture *tex;
        HASH_FIND_STR(texture_hash, fullname, tex);
        if (tex != NULL) {
                HASH_DEL(texture_hash, tex);            /* Remove from hash. */
        } else {
                tex = texture_alloc(fullname, flags);   /* Create. */
        }

        /* Mark texture as recently used and add it to hash. */
        tex->usage = TEXTURE_HISTORY;
        HASH_ADD_STR(texture_hash, name, tex);

        return tex;
}

/*
 * Look up texture struct by name in the global texture_hash. If it's not there,
 * create a new texture.
 *
 * name         File name of the texture image.
 * flags        Flags affect texture behavior.
 *              Use TEXFLAG_FILTER to have filtered textures.
 *
 * Returns the new texture object or NULL if there was an error and texture
 * could not be loaded. Error messages are printed to log stream.
 */
Texture *
texture_load(const char *img_name, unsigned flags)
{
        Texture *tex = lookup_or_create(img_name, flags);
        if (tex->id != 0)
                return tex;     /* Texture is loaded and ready! */

        /* Read image data. */
        log_msg("Loading image `%s`\n", img_name);
        SDL_Surface *img = IMG_Load(img_name);
        if (img == NULL) {
                log_err("Image load error: %s\n", IMG_GetError());
                texture_free(tex);
                return NULL;    /* Not found. */
        }

        /* Generate texture ID, bind it, set parameters. */
        gen_and_bind(&tex->id, (flags & TEXFLAG_FILTER));

        /* Load data into OpenGL. */
        unsigned w, h;
        surface_to_texture(img, flags, &w, &h);
        SDL_FreeSurface(img);

        /* Store width & height in texture struct. */
        texture_set_size(tex, w, h);
        return tex;
}

/*
 * Same thing as texture_preload() except we receive a valid SDL_Surface to read
 * image data from instead of just memory buffer with compressed image data
 * (e.g., PNG).
 */
Texture *
texture_preload_surface(const char *img_name, unsigned flags, SDL_Surface *img)
{
        Texture *tex = lookup_or_create(img_name, flags);
        if (tex->id != 0)
                return tex;     /* Texture is loaded and ready! */

        /* Generate texture ID, bind it, set parameters. */
        gen_and_bind(&tex->id, (flags & TEXFLAG_FILTER));

        /* Load image data into OpenGL. */
        unsigned w, h;
        surface_to_texture(img, flags, &w, &h);

        /* Store width & height in texture struct. */
        texture_set_size(tex, w, h);
        return tex;
}

/*
 * This loads texture data from memory. It's an optimization hack for the few
 * instances where image data is fetched over network or somehow ends up in
 * memory before being written to disk. Then it should be faster to read
 * directly from that very same memory rather than the extra step of reading it
 * from disk.
 */
Texture *
texture_preload(const char *img_name, unsigned flags, const void *buf,
                unsigned bufsize)
{
        Texture *tex = lookup_or_create(img_name, flags);
        if (tex->id != 0)
                return tex;     /* Texture is loaded and ready! */

        /* Read image data. */
        SDL_Surface *img = IMG_Load_RW(SDL_RWFromConstMem(buf, bufsize), 0);
        assert(img != NULL);

        /* Generate texture ID, bind it, set parameters. */
        gen_and_bind(&tex->id, (flags & TEXFLAG_FILTER));

        /* Load image data into OpenGL. */
        unsigned w, h;
        surface_to_texture(img, flags, &w, &h);
        SDL_FreeSurface(img);

        /* Store width & height in texture struct. */
        texture_set_size(tex, w, h);
        return tex;
}

Texture *
texture_load_blank(const char *img_name, unsigned flags)
{
        Texture *tex = lookup_or_create(img_name, flags);

        /* Unload texture from OpenGL if it's in there. */
        if (tex->id != 0)
                texture_unload(tex);

        /* Generate texture ID, bind it, set parameters. */
        gen_and_bind(&tex->id, (flags & TEXFLAG_FILTER));
        return tex;
}
