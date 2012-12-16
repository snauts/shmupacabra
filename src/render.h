#ifndef GAME2D_RENDER_H
#define GAME2D_RENDER_H

#include "camera.h"

void    render_clear(void);
void    render(Camera *cam);
void    render_debug(Camera *cam);
void    render_to_framebuffer(Camera *cam);

#endif
