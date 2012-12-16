#ifndef GAME2D_SHAPE_H
#define GAME2D_SHAPE_H

#include "common.h"
#include "geometry.h"
#include "property.h"
#include "grid.h"
#include "uthash_tuned.h"

struct Body_t;
struct World_t;
struct Group_t;

/* Shape types. */
enum ShapeType {
        SHAPE_CIRCLE    = 1,
        SHAPE_RECTANGLE
};

/*
 * Shape flags:
 *
 * SHAPE_INTERSECT      Shape is intersecting with some other shape.
 *                      This is only used for debugging -- intersecting shapes
 *                      are drawn in a different color.
 */
enum {
        SHAPE_INTERSECT = (1<<0),
        SHAPE_VISITED   = (1<<1)
};

#if TRACE_MAX
/*
 * ShapeState records shape state at a given moment in time.
 */
typedef struct {
        Property *def; 
} ShapeState;
#endif

/*
 * Shape represents a physical shape: either rectangle or circle.
 */
typedef struct Shape_t {
        int             objtype;        /* = OBJTYPE_SHAPE */
        struct Body_t   *body;          /* Body the shape belongs to. */

        uint8_t         shape_type;     /* Circle or rectangle. */
        Property        *def;           /* Actual shape definition. */

        uint32_t        color;          /* Color for display in shape editor. */
        unsigned        flags;
        
#if TRACE_MAX
        ShapeState      *trace;
#endif
        struct Group_t  *group;         /* Collision group. */
        
        GridObject      go;             /* So shape can be added to grid.*/
        struct Shape_t *prev, *next;    /* For use in lists. */
} Shape;

int              shape_filter(void *ptr);

int              shape_vs_bb(Shape *, BB bb);
int              shape_vs_shape(Shape *a, Shape *b, BB *resolve);

void             shape_bb_changed(Shape *);

ShapeDef         shape_def(Shape *);
void             shape_set_def(Shape *, ShapeDef def);
void             shape_anim_def(Shape *, uint8_t, ShapeDef, float, float);

Shape           *shape_new(struct Body_t *, struct Group_t *, uint8_t stype,
                           ShapeDef def);
Shape           *shape_clone(struct Body_t *, const Shape *);
void             shape_free(Shape *);
void             shape_record_trace(Shape *, unsigned trace_index);

#endif /* GAME2D_SHAPE_H */
