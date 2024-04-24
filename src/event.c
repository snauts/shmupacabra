#include <SDL.h>
#include "common.h"
#include "camera.h"
#include "config.h"
#include "event.h"
#include "mem.h"
#include "utlist.h"
#include "uthash_tuned.h"
#include "util_lua.h"

static void
callfunc_prepare(lua_State *L, EventFunc *bind)
{
        extern int callfunc_index;
        lua_pushvalue(L, callfunc_index);               /* + callfunc */
        assert(lua_isfunction(L, -1));
        lua_pushinteger(L, bind->func.lua_func_id);     /* + func_id */
        lua_pushinteger(L, bind->callback_data);        /* + arg_id  */
        lua_pushboolean(L, 0);                          /* + false   */
}

static void
callfunc_call(lua_State *L, unsigned num_args, unsigned num_ret)
{
        extern int errfunc_index;
        if (lua_pcall(L, num_args + 3, num_ret, errfunc_index))
                fatal_error("[Lua] %s", lua_tostring(L, -1));
}


#if ENABLE_KEYS

/* Keypress handling function. */
EventFunc key_bind;

static void
exec_key_binding(lua_State *L, SDL_Keysym key, int key_down)
{
#ifndef NDEBUG
        /* Switch debug camera with bracket keys. */
        if (key_down && (key.mod & KMOD_LALT) && config.debug) {
                extern Camera *debug_cam, *cam_list;
                int dcam_isset = (debug_cam != NULL &&
                                  debug_cam->objtype == OBJTYPE_CAMERA);

                switch (key.sym) {
                case 'c': {
                        debug_cam = dcam_isset ? debug_cam->next : cam_list;
                        break;
                }
                case 's': {
                        extern int drawShapes;
                        if (!dcam_isset) {
                                debug_cam = cam_list;
                                drawShapes = 1;
                        } else {
                                TOGGLE(drawShapes);
                        }
                        break;
                }
                case 'd': {
                        extern int drawShapeBBs;
                        if (!dcam_isset) {
                                debug_cam = cam_list;
                                drawShapeBBs = 1;
                        } else {
                                TOGGLE(drawShapeBBs);
                        }
                        break;
                }
                case 't': {
                        extern int drawTileBBs;
                        if (!dcam_isset) {
                                debug_cam = cam_list;
                                drawTileBBs = 1;
                        } else {
                                TOGGLE(drawTileBBs);
                        }
                        break;
                }
                case 'b': {
                        extern int drawBodies;
                        if (!dcam_isset) {
                                debug_cam = cam_list;
                                drawBodies = 1;
                        } else {
                                TOGGLE(drawBodies);
                        }
                        break;
                }
                case 'g': {
                        extern int drawGrid;
                        if (!dcam_isset) {
                                debug_cam = cam_list;
                                drawGrid = 1;
                        } else {
                                TOGGLE(drawGrid);
                        }
                        break;
                }
                case 'o': {
                        extern int outsideView;
                        if (!dcam_isset) {
                                debug_cam = cam_list;
                                outsideView = 1;
                        } else {
                                TOGGLE(outsideView);
                        }
                        break;
                }
                default:
                        break;
                }
        }
#endif  /* !NDEBUG */
        if (key_bind.type == EVFUNC_TYPE_NONE)
                return;

        assert(key_bind.type == EVFUNC_TYPE_C ||
               key_bind.type == EVFUNC_TYPE_LUA);
        if (key_bind.type == EVFUNC_TYPE_C) {
                /* Call key handler function. */
                assert(key_bind.func.key_func != NULL);
                key_bind.func.key_func(key, key_down, key_bind.callback_data);
                return;
        }

        callfunc_prepare(L, &key_bind);
        lua_createtable(L, 0, 2);       /* + {scancode=?, keycode=?} */
        lua_pushstring(L, "scancode");
        lua_pushinteger(L, key.scancode);
        lua_rawset(L, -3);
        lua_pushstring(L, "keycode");
        lua_pushinteger(L, key.sym);
        lua_rawset(L, -3);
        lua_pushboolean(L, key_down);   /* + pressed */
        callfunc_call(L, 2, 0);
}
#endif  /* ENABLE_KEYS */

#if ENABLE_JOYSTICK

EventFunc joybutton_bind;
EventFunc joyaxis_bind;

void
open_joysticks(void)
{
        static int joysticks_opened = 0;
        if (joysticks_opened)
                return;

        /* Open joysticks. */
        SDL_JoystickEventState(SDL_ENABLE);
        int num_joy = SDL_NumJoysticks();
        log_msg("Number of joysticks: %d", num_joy);
        for (int i = 0; i < num_joy; i++) {
                SDL_Joystick *joy = SDL_JoystickOpen(i);
                if (joy == NULL) {
                        log_warn("    Could not open joystick (%i).", i);
                } else {
                        log_msg("    Opened joystick `%s`.", SDL_JoystickName(joy));
                }
        }
        joysticks_opened = 1;
}

static void
exec_joybutton(lua_State *L, SDL_JoyButtonEvent ev)
{
        if (joybutton_bind.type == EVFUNC_TYPE_NONE)
                return;

        callfunc_prepare(L, &joybutton_bind);
        lua_pushinteger(L, ev.which);                   /* + joyID    */
        lua_pushinteger(L, ev.button);                  /* + buttonID */
        lua_pushboolean(L, ev.state == SDL_PRESSED);    /* + pressed  */
        callfunc_call(L, 3, 0);
}

static void
exec_joyaxis(lua_State *L, SDL_JoyAxisEvent ev)
{
        if (joyaxis_bind.type == EVFUNC_TYPE_NONE)
                return;

        callfunc_prepare(L, &joyaxis_bind);
        lua_pushinteger(L, ev.which);                   /* + joyID  */
        lua_pushinteger(L, ev.axis);                    /* + axisID */
        lua_pushnumber(L, (float)ev.value / (32767 + (ev.value < 0)));
        callfunc_call(L, 3, 0);
}

static void
exec_joyhat_dir(lua_State *L, SDL_JoyHatEvent ev,
		int num, Uint8 oldValue, Uint8 dir) {
	if ((ev.value & dir) != (oldValue & dir)) {
		callfunc_prepare(L, &joybutton_bind);
		lua_pushinteger(L, ev.which);
		lua_pushinteger(L, 1000 + ev.hat * 4 + num);
		lua_pushboolean(L, (ev.value & dir) != 0);
		callfunc_call(L, 3, 0);
	}
}

static void
exec_joyhat(lua_State *L, SDL_JoyHatEvent ev)
{
        if (joyaxis_bind.type == EVFUNC_TYPE_NONE)
                return;

	static Uint8 oldValue = 0;
	exec_joyhat_dir(L, ev, 0, oldValue, SDL_HAT_UP);
	exec_joyhat_dir(L, ev, 1, oldValue, SDL_HAT_RIGHT);
	exec_joyhat_dir(L, ev, 2, oldValue, SDL_HAT_DOWN);
	exec_joyhat_dir(L, ev, 3, oldValue, SDL_HAT_LEFT);
	oldValue = ev.value;
}
#endif

#if ENABLE_TOUCH || ENABLE_MOUSE

/*
 * Translate screen position into world coordinate space.
 */
static vect_f
screenpos_to_world(Camera *cam, vect_i pos)
{
        BB viewp = cam->viewport;
        int view_w = viewp.r - viewp.l;
        int view_h = viewp.b - viewp.t;
        float zoom = cam->zoom;
        vect_f cam_pos = body_pos(&cam->body);
        vect_i sz = cam->size;

        /*
         * First, make coordinates relative to bottom left screen
         * corner.
         */
        pos = (vect_i){pos.x, config.window_height - pos.y};

        /* Make coordinates relative to camera viewport bottom left. */
        pos.x -= viewp.l;
        pos.y -= (config.screen_height - viewp.b);

        /* Transform position into world coordinate space. */
        return (vect_f){
                cam_pos.x + sz.x * ((float)pos.x/view_w - 0.5) / zoom,
                cam_pos.y + sz.y * ((float)pos.y/view_h - 0.5) / zoom
        };
}

/*
 * Translate screen delta into world delta.
 */
static vect_f
screendelta_to_world(Camera *cam, vect_i delta)
{
        BB viewp = cam->viewport;
        int view_w = viewp.r - viewp.l;
        int view_h = viewp.b - viewp.t;
        float zoom = cam->zoom;
        vect_i sz = cam->size;

        /*
         * First, make coordinates relative to bottom left screen
         * corner.
         */
        delta = (vect_i){delta.x, -delta.y};

        /* Transform delta into world coordinate space. */
        return (vect_f){
                (float)delta.x * sz.x / view_w / zoom,
                (float)delta.y * sz.y / view_h / zoom
        };
}

#endif  /* ENABLE_TOUCH || ENABLE_MOUSE */

#if ENABLE_TOUCH

/* Keep track of all onscreen touches. */
static Touch *touch_hash;

static void
exec_touch_binding(lua_State *L, uint32_t type, const Touch *event)
{
        /*
         * Create a temporary array that holds pointers to all onscreen
         * touches.
         */
        unsigned num_touches = 0;
        const Touch *touch_array[20];
        Touch *t, *t_tmp;
        HASH_ITER(hh, touch_hash, t, t_tmp) {
                assert(num_touches < ARRAYSZ(touch_array));
                touch_array[num_touches++] = t;
        }
        assert(num_touches > 0);        /* Must have at least one. */

#ifndef NDEBUG
        /* Switch debug camera every time 3 fingers touch the screen. */
        if (type == SDL_FINGERDOWN && num_touches == 3) {
                extern Camera *debug_cam, *cam_list;
                if (debug_cam != NULL && debug_cam->objtype == OBJTYPE_CAMERA)
                        debug_cam = debug_cam->next;
                else
                        debug_cam = cam_list;
        }
#endif  /* !NDEBUG */

        /* Create a temporary array of all cameras. */
        unsigned num_cameras = 0;
        Camera *cam_array[10], *cam;
        extern Camera *cam_list;
        DL_FOREACH(cam_list, cam) {
                assert(num_cameras < ARRAYSZ(cam_array));
                cam_array[num_cameras++] = cam;
        }

        /*
         * Cameras are sorted for drawing order: back to front.
         * When handling touches, we traverse this list in reverse, so that
         * topmost camera gets the touch event first.
         */
        while (num_cameras) {
                cam = cam_array[--num_cameras];

                EventFunc *bind = NULL;
                switch (type) {
                case SDL_FINGERDOWN:
                        bind = &cam->touchdown_bind;
                        break;
                case SDL_FINGERUP:
                        bind = &cam->touchup_bind;
                        break;
                case SDL_FINGERMOTION:
                        bind = &cam->touchmove_bind;
                        break;
                default:
                        abort();
                }

                /*
                 * See if camera is active and touch handlers are bound for
                 * this camera.
                 */
                if (cam->disabled || bind->type == EVFUNC_TYPE_NONE)
                        continue;

                /*
                 * Call user touch handler function.
                 *
                 * If a non-zero value is returned from handler, it means user
                 * has requested that this touch event is not given to remaining
                 * cameras, so we return immediately.
                 */
                assert(bind->type == EVFUNC_TYPE_C && bind->func.touch_func);
                if (bind->func.touch_func(cam, event, touch_array, num_touches,
                                          bind->callback_data)) {
                        return;
                }
        }
}

/*
 * Position where the touch started.
 */
vect_f
touch_first_pos(Camera *cam, const Touch *t)
{
        assert(cam && t);
        return screenpos_to_world(cam, t->first_pos);
}

/*
 * Last (current) position of touch.
 */
vect_f
touch_pos(Camera *cam, const Touch *t)
{
        assert(cam && t);
        return screenpos_to_world(cam, t->pos[t->pos_index]);
}

/*
 * Last delta motion of touch.
 */
vect_f
touch_delta(Camera *cam, const Touch *t)
{
        assert(cam && t);
        assert(t->num_pos > 0);

        if (t->num_pos == 1)
                return (vect_i){0, 0};

        int index = t->pos_index;
        int prev_index = index - 1;
        if (prev_index < 0)
                prev_index = ARRAYSZ(t->pos) - 1;

        return screendelta_to_world(cam, vect_i_sub(t->pos[index],
                                              t->pos[prev_index]));
}

/*
 * Last nonzero delta motion (separate for each direction).
 */
vect_f
touch_effective_delta(struct Camera_t *cam, const Touch *t)
{
        assert(cam && t);
        assert(t->num_pos > 0);

        if (t->num_pos == 1)
                return (vect_i){0, 0};

        vect_i nonzero_delta = {0, 0};
        unsigned num_looked = 1;
        int index = t->pos_index, prev_index;
        do {
                /* Previous position index. */
                prev_index = index - 1;
                if (prev_index < 0)
                        prev_index = ARRAYSZ(t->pos) - 1;

                /* Delta motion. */
                vect_i delta = vect_i_sub(t->pos[index], t->pos[prev_index]);
                if (nonzero_delta.x == 0) {
                        if (delta.x != 0)
                                nonzero_delta.x = delta.x;
                } else if (nonzero_delta.y != 0)
                        break;
                if (nonzero_delta.y == 0 && delta.y != 0)
                        nonzero_delta.y = delta.y;

                index = prev_index;
                ++num_looked;
        } while (num_looked < t->num_pos && num_looked < ARRAYSZ(t->pos));

        return screendelta_to_world(cam, nonzero_delta);
}

/*
 * Displacement of touch from beginning to current position.
 */
vect_f
touch_disp(struct Camera_t *cam, const Touch *t)
{
        vect_f first = touch_first_pos(cam, t);
        vect_f last = touch_pos(cam, t);
        return (vect_f){
                last.x - first.x,
                last.y - first.y
        };
}

void
clear_touches(void)
{
        /* Clear touches. */
        while (touch_hash != NULL) {
                Touch *touch = touch_hash;
                HASH_DEL(touch_hash, touch);

                extern mem_pool mp_touch;
                mp_free(&mp_touch, touch);
        }
}

#endif  /* ENABLE_TOUCH */

#if ENABLE_MOUSE

vect_f
mouse_pos(Camera *cam)
{
        int x, y;
        SDL_GetMouseState(&x, &y);
        return screenpos_to_world(cam, (vect_i){x, y});
}

static void
exec_mouseclick(lua_State *L, SDL_MouseButtonEvent ev)
{
        /* Create a temporary array of all cameras. */
        unsigned num_cameras = 0;
        Camera *cam_array[10], *cam;
        extern Camera *cam_list;
        DL_FOREACH(cam_list, cam) {
                assert(num_cameras < ARRAYSZ(cam_array));
                cam_array[num_cameras++] = cam;
        }

        /*
         * Cameras are sorted for drawing order: back to front.
         * When handling events, we traverse this list in reverse, so that
         * topmost camera gets the events first.
         */
        while (num_cameras) {
                cam = cam_array[--num_cameras];
                EventFunc *bind = &cam->mouseclick_bind;

                /* See if camera is active and handlers are bound for it. */
                if (cam->disabled || bind->type == EVFUNC_TYPE_NONE)
                        continue;

                callfunc_prepare(L, bind);
                lua_pushlightuserdata(L, cam);               /* + cam      */
                lua_pushinteger(L, ev.button);               /* + buttonID */
                lua_pushboolean(L, ev.state == SDL_PRESSED); /* + pressed  */
                L_push_vectf(L, screenpos_to_world(cam, (vect_i){ev.x, ev.y}));
                callfunc_call(L, 4, 1);

                /*
                 * If a non-false value is returned from handler, it means user
                 * has requested that this event is not given to remaining
                 * cameras, so we return immediately.
                 */
                int skip = lua_toboolean(L, -1);
                lua_pop(L, 1);
                if (skip)
                        return;
        }
}

static void
exec_mousemove(lua_State *L, SDL_MouseMotionEvent ev)
{
        /* Create a temporary array of all cameras. */
        unsigned num_cameras = 0;
        Camera *cam_array[10], *cam;
        extern Camera *cam_list;
        DL_FOREACH(cam_list, cam) {
                assert(num_cameras < ARRAYSZ(cam_array));
                cam_array[num_cameras++] = cam;
        }

        /*
         * Cameras are sorted for drawing order: back to front.
         * When handling events, we traverse this list in reverse, so that
         * topmost camera gets the events first.
         */
        while (num_cameras) {
                cam = cam_array[--num_cameras];
                EventFunc *bind = &cam->mousemove_bind;

                /* See if camera is active and handlers are bound for it. */
                if (cam->disabled || bind->type == EVFUNC_TYPE_NONE)
                        continue;

                callfunc_prepare(L, bind);
                lua_pushlightuserdata(L, cam);          /* + cam */
                L_push_vectf(L, screenpos_to_world(cam, (vect_i){ev.x, ev.y}));
                L_push_vectf(L, screendelta_to_world(cam,
                                                  (vect_i){ev.xrel, ev.yrel}));
                callfunc_call(L, 3, 1);

                /*
                 * If a non-false value is returned from handler, it means user
                 * has requested that this event is not given to remaining
                 * cameras, so we return immediately.
                 */
                int skip = lua_toboolean(L, -1);
                lua_pop(L, 1);
                if (skip)
                        return;
        }
}

#endif  /* ENABLE_MOUSE */

/*
 * Process input/window events.
 */
void
process_events(lua_State *L)
{
        SDL_Event ev;
        while (SDL_PollEvent(&ev) != 0) {
                switch (ev.type) {
                case SDL_QUIT:
                        exit(EXIT_SUCCESS);
#if ENABLE_KEYS
                case SDL_KEYDOWN:
                        exec_key_binding(L, ev.key.keysym, 1);
                        continue;
                case SDL_KEYUP:
                        exec_key_binding(L, ev.key.keysym, 0);
                        continue;
#endif  /* ENABLE_KEYS */
#if ENABLE_JOYSTICK
                case SDL_JOYBUTTONDOWN:
                case SDL_JOYBUTTONUP:
                        exec_joybutton(L, ev.jbutton);
                        continue;
                case SDL_JOYHATMOTION:
                        exec_joyhat(L, ev.jhat);
                        continue;
                case SDL_JOYAXISMOTION:
                        exec_joyaxis(L, ev.jaxis);
                        continue;
#endif  /* ENABLE_JOYSTICK */
#if ENABLE_MOUSE
                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEBUTTONUP:
                        exec_mouseclick(L, ev.button);
                        continue;
                case SDL_MOUSEMOTION:
                        exec_mousemove(L, ev.motion);
                        continue;
#endif  /* ENABLE_MOUSE */
#if ENABLE_TOUCH
                case SDL_FINGERDOWN: {
                        /* See if a touch with this ID already exists.. */
                        Touch *t;
                        SDL_TouchID touch_id = ev.tfinger.touchId;
#ifndef NDEBUG
                        HASH_FIND(hh, touch_hash, &touch_id, sizeof(touch_id),
                                  t);
                        assert(t == NULL);
#endif
                        /* Allocate a new Touch structure and add it to hash. */
                        extern mem_pool mp_touch;
                        t = mp_alloc(&mp_touch);

                        /* Set touch-id, position and start position. */
                        t->id = touch_id;
                        t->first_pos = (vect_i){ev.tfinger.x, ev.tfinger.y};
                        t->pos[0] = t->first_pos;
                        t->num_pos = 1;

                        /* Add touch to hash using touch ID as key. */
                        HASH_ADD(hh, touch_hash, id, sizeof(t->id), t);

                        /* Run touch-down event handlers. */
                        exec_touch_binding(L, SDL_FINGERDOWN, t);
                        continue;
                }
                case SDL_FINGERMOTION: {
                        /* Find touch. */
                        Touch *t;
                        SDL_TouchID touch_id = ev.tfinger.touchId;
                        HASH_FIND(hh, touch_hash, &touch_id, sizeof(touch_id),
                                  t);
                        if (t == NULL)
                                continue;       /* Not tracking this touch. */

                        /* Add touch position. */
                        if (++t->pos_index >= ARRAYSZ(t->pos))
                                t->pos_index = 0;
                        t->pos[t->pos_index] = (vect_i){
                                ev.tfinger.x,
                                ev.tfinger.y
                        };
                        t->num_pos++;

                        /* Run touch-move event handlers. */
                        exec_touch_binding(L, SDL_FINGERMOTION, t);
                        continue;
                }
                case SDL_FINGERUP: {
                        /* Find touch. */
                        Touch *t;
                        SDL_TouchID touch_id = ev.tfinger.touchId;
                        HASH_FIND(hh, touch_hash, &touch_id, sizeof(touch_id),
                                  t);
                        if (t == NULL)
                                continue;       /* Not tracking this touch. */

                        /* Run touch-up event handlers. */
                        exec_touch_binding(L, SDL_FINGERUP, t);

                        /*
                         * Free touch memory (or not if clear_touches() was
                         * called).
                         */
                        if (touch_hash != NULL) {
                                extern mem_pool mp_touch;
                                HASH_DEL(touch_hash, t);
                                mp_free(&mp_touch, t);
                        }
                        continue;
                }
#endif /* ENABLE_TOUCH */
                }
        }
}
