#ifndef GAME2D_TILE_H
#define GAME2D_TILE_H

#include "common.h"
#include "body.h"
#include "geometry.h"
#include "property.h"
#include "grid.h"
#include "spritelist.h"

#define BLEND_SHIFT     0

/* Tile flags.
 *
 * TILE_BLEND           Use first three bits for blending function number.
 * TILE_FLIP_X          Horizontal flip.
 * TILE_FLIP_Y          Vertical flip.
 * TILE_SMOOTH          Do not round position and size.
 * TILE_VISITED         For traversal functions.
 */
enum {
        TILE_BLEND       = 7<<BLEND_SHIFT,
        TILE_FLIP_X      = 1<<3,
        TILE_FLIP_Y      = 1<<4,
#if !ALL_SMOOTH
        TILE_SMOOTH      = 1<<5,
#endif
        TILE_VISITED     = 1<<6
};

/* Available blend functions (number stored within flags). */
enum {
        TILE_BLEND_DEFAULT      = 0,
        TILE_BLEND_SOURCE       = 1<<BLEND_SHIFT,
        TILE_BLEND_MULTIPLY     = 2<<BLEND_SHIFT,
        TILE_BLEND_MASK         = 3<<BLEND_SHIFT,
        TILE_BLEND_DESTINATION  = 4<<BLEND_SHIFT,
        TILE_BLEND_ADD          = 5<<BLEND_SHIFT,
        TILE_BLEND_ALPHA        = 6<<BLEND_SHIFT
};

#if TRACE_MAX
/*
 * TileState records tile state at a given moment in time.
 */
typedef struct {
        Property *pos, *size, *frame, *color, *angle; 
} TileState;
#endif

/*
 * A tile is like a canvas: drawing area specified by position and size.
 * A list of sprites (SpriteList) is always bound to a tile. One frame from this
 * sprite list is drawn at any one time (see frame_index member variable).
 * Tile's position is relative to its owner Body object, and it specifies the
 * position of its lower left corner.
 */
typedef struct Tile_t {
        int             objtype;                /* = OBJTYPE_TILE */
        Body            *body;                  /* Body object = owner. */
        
        SpriteList      *sprite_list;           /* Graphic. */
        
        Property        *pos;                   /* Position relative to Body. */
        Property        *size;                  /* Tile size. */
        Property        *frame;
        Property        *color;
        Property        *angle;
        
        float           depth;                  /* Determines drawing order. */
        unsigned        flags;
        
#if TRACE_MAX
        TileState       *trace;
#endif
        GridObject      go;                     /* Tile can be added to grid. */
        struct Tile_t   *prev, *next;           /* For use in lists. */
} Tile;

int      tile_filter(void *ptr);

Tile    *tile_new(Body *, vect_f pos, vect_f size, float depth, int grid_store);
Tile    *tile_clone(Body *parent, const Tile *orig);
void     tile_free(Tile *);
void     tile_record_trace(Tile *, unsigned trace_index);

#if ENABLE_TILE_GRID
void     tile_bb_changed(Tile *);
#else
#define tile_bb_changed(t)      ((void)0)
#endif

/* Retrieve tile properties. */
unsigned tile_frame(Tile *);
vect_f   tile_size(Tile *);
vect_f   tile_pos(Tile *);
float    tile_angle(Tile *);
uint32_t tile_color(Tile *);

/* Set tile properties. */
void     tile_set_frame(Tile *t, unsigned frame);
void     tile_set_size(Tile *t, vect_f size);
void     tile_set_pos(Tile *t, vect_f pos);
void     tile_set_color(Tile *t, uint32_t color);
void     tile_set_angle(Tile *t, vect_f pivot, float angle);

/* Animate tile properties. */
void     tile_anim_frame(Tile *t, uint8_t type, unsigned end, float duration,
                         float start_time);
void     tile_anim_size(Tile *t, uint8_t type, vect_f end, float duration,
                        float start_time);
void     tile_anim_pos(Tile *t, uint8_t type, vect_f end, float duration,
                       float start_time);
void     tile_anim_color(Tile *t, uint8_t type, uint32_t end, float duration,
                         float start_time);
void     tile_anim_angle(Tile *t, uint8_t type, vect_f pivot, float end,
                         float duration, float start_time);

#endif
