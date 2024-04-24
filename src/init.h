#ifndef GAME2D_INIT_H
#define GAME2D_INIT_H

#include <SDL.h>
#include "common.h"

void             init_main_framebuffer(void);
void             bind_main_framebuffer(void);
void             draw_main_framebuffer(void);

void             setup_memory(void);
void             cleanup(void);
SDL_Window      *create_window(void);

#endif  /* GAME2D_INIT_H */
