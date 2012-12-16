#ifndef GAME2D_COMPILE_CFG_H
#define GAME2D_COMPILE_CFG_H

/* Feature switches. */
#define ENABLE_LUA              1
#define ENABLE_SQLITE           0
#define ENABLE_SDL_VIDEO        1
#define ENABLE_SDL2             0
#define ENABLE_AUDIO            1
#define ENABLE_TOUCH            0
#define ENABLE_KEYS             1
#define ENABLE_MOUSE            0
#define ENABLE_JOYSTICK         1
#define ENABLE_ACCELEROMETER    0
#define ENABLE_TILE_GRID        0

#define PLATFORM_IOS            0

/* Limits. */
#define VISIBLE_TILES_MAX       4000
#define GRID_VISITED_MAX        5000
#define SHAPEGROUPS_MAX         20
#define ACTIVE_BODIES_MAX       4000
#define ACTIVE_SHAPES_MAX       4000
#define ACTIVE_TILES_MAX        4000

/*
 * If true, bodies do not go to sleep once they leave camera vicinity. All
 * bodies are always active.
 */
#define ALL_NOCTURNAL 1

#endif

