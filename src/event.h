#ifndef GAME2D_EVENT_H
#define GAME2D_EVENT_H

#include <SDL.h>
#include "common.h"
#include "geometry.h"
#include "uthash_tuned.h"

struct Camera_t;

#if !ENABLE_SDL_VIDEO
typedef struct HackEvent_t {
        SDL_Event ev;
        struct HackEvent_t *prev, *next;
} HackEvent;
#endif




/*
 *
 * TOUCH
 *
 */
#if ENABLE_TOUCH

/*
 * Touch structure represents a finger moving across the screen.
 *
 * hh           Touches are hashed (id -> Touch). Used internally -- not for
 *              scripts.
 */
typedef struct {
        SDL_TouchID id;
        
        vect_i   first_pos;     /* The very first recorded position. */
        
        unsigned num_pos;       /* Number of recorded positions. */
        unsigned pos_index;     /* Last position index within `pos` array. */
        vect_i   pos[100];      /* Last 100 positions. */

        UT_hash_handle hh;
} Touch;

/* Touch handling function type. */
typedef int (*TouchHandler)(struct Camera_t *cam, const Touch *event,
                            const Touch *touches[], unsigned num_touches,
                            intptr_t data);

vect_f  touch_first_pos(struct Camera_t *cam, const Touch *t);
vect_f  touch_pos(struct Camera_t *cam, const Touch *t);
vect_f  touch_delta(struct Camera_t *cam, const Touch *t);
vect_f  touch_effective_delta(struct Camera_t *cam, const Touch *t);
vect_f  touch_disp(struct Camera_t *cam, const Touch *t);

void    clear_touches(void);

#else   /* ENABLE_TOUCH */
typedef void * TouchHandler;
#endif  /* ENABLE_TOUCH */





/*
 *
 * KEYS
 *
 */
#if ENABLE_KEYS

#if !ENABLE_SDL2

typedef int SDL_Scancode;
typedef SDL_keysym SDL_Keysym;
#define SDL_SCANCODE_UNKNOWN 0
#define SDL_NUM_SCANCODES 512

const char   *SDL_GetScancodeName(SDL_Scancode scancode);
SDL_Scancode  SDL_GetScancodeFromName(const char *name);
#endif  /* !ENABLE_SDL2 */

/* Key handling function type. */
typedef void (*KeyHandler)(SDL_Keysym key, int key_down, intptr_t data);

#else   /* ENABLE_KEYS */

typedef void * KeyHandler;

#endif  /* !ENABLE_KEYS */




/*
 *
 * JOYSTICK
 *
 */
#if ENABLE_JOYSTICK

void    open_joysticks(void);

#endif  /* ENABLE_JOYSTICK */





/*
 *
 * MOUSE
 *
 */
#if ENABLE_MOUSE

/* Mouse event handlers. */
typedef int (*MouseClickHandler)(struct Camera_t *cam, int button_id,
                                 int pressed, intptr_t data);
typedef int (*MouseMoveHandler)(struct Camera_t *cam, intptr_t data);

vect_f  mouse_pos(struct Camera_t *cam);

#else   /* ENABLE_MOUSE */

typedef void * MouseClickHandler;
typedef void * MouseMoveHandler;

#endif  /* !ENABLE_MOUSE */


enum {
        EVFUNC_TYPE_NONE   = 0,         /* This MUST be zero. */
        EVFUNC_TYPE_C      = 1,
        EVFUNC_TYPE_LUA    = 2
};

/*
 * Structure that will be bound to a key or touch event.
 */
typedef struct {
        uint8_t type;
        union {
                KeyHandler key_func;
                TouchHandler touch_func;
                unsigned lua_func_id;
        } func;
        intptr_t        callback_data;
} EventFunc;

void    process_events(lua_State *L);

#endif  /* GAME2D_EVENT_H */
