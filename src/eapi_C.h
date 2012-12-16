#ifndef GAME2D_EAPIC_H
#define GAME2D_EAPIC_H

#include "camera.h"
#include "event.h"
#include "shape.h"
#include "spritelist.h"
#include "tile.h"
#include "world.h"

/* Keys. */
void             BindKeyboard(KeyHandler handler, intptr_t);
const char      *GetKeyName(SDL_Scancode scancode);
SDL_Scancode     GetKeyFromName(const char *name);

/* Bind joystick event handlers. */
void             BindJoystickButton(KeyHandler handler, intptr_t);
void             BindJoystickAxis(int placeholder, ...);

/* Bind mouse event handlers. */
void             BindMouseClick(int placeholder, ...);
void             BindMouseMove(int placeholder, ...);

/* Bind touch handler functions. */
void             BindTouchDown(Camera *cam, TouchHandler handler, intptr_t);
void             BindTouchMove(Camera *cam, TouchHandler handler, intptr_t);
void             BindTouchUp(Camera *cam, TouchHandler handler, intptr_t);
void             UnbindTouches(Camera *cam);

/* Create objects. */
World           *NewWorld(const char *name, unsigned step, BB area,
                          unsigned grid_cell_size, unsigned trace_skip);
Body            *NewBody(void *parent, vect_f pos);
Camera          *NewCamera(World *, vect_f, const vect_i *, const BB *, int);
Shape           *NewShape(void *, vect_f, BB, const char *);
Tile            *NewTile(void *, vect_f, const vect_f *, SpriteList *, float);
Tile            *NewTileCentered(void *, vect_f, const vect_f *, SpriteList *,
                                 float);
SpriteList      *NewSpriteList(const char *, unsigned, const BB *, ...);
SpriteList      *ChopImage(const char *texname, unsigned flags, vect_i size,
                           unsigned first, unsigned total, unsigned skip);

/* Get misc stuff. */
World           *GetWorld(void *obj);
Body            *GetBody(void *obj);
vect_i           GetImageSize(const char *img_name, unsigned flags);
float            GetTime(void *obj);
Tile            *GetFirstTile(void *obj);
Tile            *GetNextTile(Tile *t);
Group           *GetGroup(World *world, const char *name);
SpriteList      *GetSpriteList(Tile *t);

/* Set misc stuff. */
void             SetZoom(Camera *cam, float zoom);
void             SetSpriteList(Tile *tile, SpriteList *spriteList);

/* Set step/after-step functions. */
void             SetStep(void *obj, StepFunction sf, intptr_t);

/* Shape. */
void             SetShape(Shape *s, BB bb);
void             AnimateShape(Shape *, uint8_t, BB, float, float);

/* Angle. */
float            GetAngle(Tile *t);
void             SetAngle(Tile *t, vect_f pivot, float angle);
void             AnimateAngle(void *, uint8_t, vect_f, float, float, float);

void             SetDepth(Tile *t, float depth);
int              FlipX(Tile *t, int flipx);
int              FlipY(Tile *t, int flipy);

/* Size. */
vect_f           GetSize(void *obj);
void             SetSize(void *obj, vect_f size);
void             AnimateSize(void *, uint8_t, vect_f, float, float);
void             AnimateSizeCentered(void *, uint8_t, vect_f, float, float);

/* Color. */
void             SetColor(void *obj, uint32_t c);
uint32_t         GetColor(void *obj);
void             AnimateColor(void *, uint8_t, uint32_t, float, float);

/* Position. */
vect_f           GetPos(void *obj);
vect_f           GetAbsolutePos(void *obj);
void             SetPos(void *obj, vect_f pos);
void             SetPosCentered(void *obj, vect_f pos);
void             SetPosX(void *obj, float value);
void             SetPosY(void *obj, float value);
void             AnimatePos(void *, uint8_t, vect_f, float, float);

/* Velocity. */
vect_f           GetVel(void *obj);
void             SetVel(void *obj, vect_f vel);
void             SetVelX(void *obj, float value);
void             SetVelY(void *obj, float value);

/* Acceleration. */
vect_f           GetAcc(void *obj);
void             SetAcc(void *obj, vect_f acc);
void             SetAccX(void *obj, float value);
void             SetAccY(void *obj, float value);

/* Frame. */
unsigned         GetFrame(Tile *t);
void             SetFrame(Tile *tile, unsigned frame_index);
void             SetFrameClamp(Tile *tile, int frame_index);
void             Animate(Tile *t, uint8_t type, float FPS, float startTime);

Shape           *SelectShape(World *, vect_i, const char *);
unsigned         SelectShapes(World *, vect_i, const char *, Shape *[],
                              unsigned);

/* Timers. */
TimerPtr         AddTimer(void *obj, float when, TimerFunction func, intptr_t);
void             CancelTimer(TimerPtr *timer_ptr);

void             DrawToTexture(const char *name, unsigned flags);

void             Pause(void *obj);
void             Resume(void *obj);
int              Alive(void *obj);

void             Collide(World *, const char *, const char *, CollisionFunc,
                         int, int, intptr_t);

void             Destroy(void *obj);
void             Clear(void);
void             Quit(void);
void             SetTextureMemoryLimit(unsigned MB);

void             StopAccelerometer(void);
int              GetAccelerometerAxes(vect3d *values);

#endif
