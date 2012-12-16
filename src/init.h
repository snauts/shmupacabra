#ifndef GAME2D_INIT_H
#define GAME2D_INIT_H

#include <SDL.h>
#include "common.h"

void             init_main_framebuffer(void);
void             bind_main_framebuffer(void);
void             draw_main_framebuffer(void);

void             setup_memory(void);
void             cleanup(void);
void             parse_cmd_opt(int argc, char *argv[]);
SDL_Window      *create_window(void);
const char      *platform_name(void);

#endif  /* GAME2D_INIT_H */
