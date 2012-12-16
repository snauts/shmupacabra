#ifndef GAME2D_GEOM_H
#define GAME2D_GEOM_H

#include "common.h"

/*
 * Two dimensional floating point vector.
 */
typedef struct {
        float x, y;
} vect_f;

/*
 * Two dimensional integer vector.
 */
typedef struct {
        int x, y;
} vect_i;

/*
 * Three dimensional floating point vector.
 */
typedef struct {
        float x, y, z;
} vect3d;

/* 
 * Rectangle specified by integer edge offsets from origin.
 */
typedef struct {
        int l, r, b, t; /* Left, right, bottom, top. */
} BB;

/*
 * Circle specified by radius and offset.
 */
typedef struct {
        uint    radius;
        vect_i  offset;
} Circle;

/*
 * Shape definition: rectangle or circle.
 */
typedef union {
        BB      rect;
        Circle  circle;
} ShapeDef;

/* Bounding box sanity (physical world, not texture). */
#define bb_valid(bb) ((bb).l < (bb).r && (bb).b < (bb).t)

/* True if bounding box `a` is inside `b`. */
#define bb_inside(aa, bb) ((aa).l >= (bb).l && (aa).r <= (bb).r && (aa).b >= (bb).b && (aa).t <= (bb).t)

/* True if bounding boxes are the same. */
#define bb_equal(aa, bb) ((aa).l == (bb).l && (aa).r == (bb).r && (aa).b == (bb).b && (aa).t == (bb).t)

/* Bounding box functions. */
int     bb_overlap_area(const BB *a, const BB *b);
int     bb_overlap(const BB *a, const BB *b);
int     bb_intersect_resolve(const BB *a, const BB *b, BB *resolve);
void    bb_union(BB *bb, BB add);
void    bb_add_vect(BB *bb, vect_i v);

/* 2D integer vector functions. */
float   vect_i_size(vect_i v);
int     vect_i_dot(vect_i a, vect_i b);
int     vect_i_eq(vect_i a, vect_i b);
vect_i  vect_i_add(vect_i a, vect_i b);
vect_i  vect_i_sub(vect_i a, vect_i b);

/* 2D floating point vector functions. */
float   vect_f_dot(vect_f a, vect_f b);
float   vect_f_size(vect_f v);
vect_f  vect_f_add(vect_f a, vect_f b);
vect_f  vect_f_sub(vect_f a, vect_f b);
vect_f  vect_f_rotate(vect_f v, float theta);

/* 3D floating point vector functions. */
float   vect3d_dot(vect3d a, vect3d b);
vect3d  vect3d_plane_proj(vect3d v, vect3d plane);

#endif
