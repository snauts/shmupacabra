#ifndef GAME2D_TEXTURE_ASYNC_H
#define GAME2D_TEXTURE_ASYNC_H

#include "common.h"
#include "texture.h"

typedef void (*TextureLoaded)(const char *img_name, uint flags, void *cb_data);

void    texasync_thread_start(void);
void    texasync_load(const char *img_name, uint flags, uintptr_t group,
                      TextureLoaded sync_cb, void *cb_data);
void    texasync_loadmem(const char *img_name, uint flags,
                         void *img_data, uint img_size, uintptr_t group,
                         TextureLoaded sync_cb, void *cb_data);
void    texasync_runsync(uintptr_t group, ...);
void    texasync_clear(void);

#endif /* GAME2D_TEXTURE_ASYNC_H */
