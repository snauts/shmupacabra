#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include "common.h"
#include "geometry.h"

float
vect_i_size(vect_i v)
{
        return sqrtf((v.x * v.x) + (v.y * v.y));
}

int
vect_i_dot(vect_i a, vect_i b)
{
        return (a.x * b.x) + (a.y * b.y);
}

int
vect_i_eq(vect_i a, vect_i b)
{
        return (a.x == b.x) && (a.y == b.y);
}

vect_i
vect_i_add(vect_i a, vect_i b)
{
        return (vect_i){a.x + b.x, a.y + b.y};
}

vect_i
vect_i_sub(vect_i a, vect_i b)
{
        return (vect_i){a.x - b.x, a.y - b.y};
}

float
vect_f_dot(vect_f a, vect_f b)
{
        return ((a.x * b.x) + (a.y * b.y));
}

float 
vect_f_size(vect_f v)
{
        return sqrtf((v.x * v.x) + (v.y * v.y));
}

vect_f
vect_f_add(vect_f a, vect_f b)
{
        return (vect_f){a.x + b.x, a.y + b.y};
}

vect_f
vect_f_sub(vect_f a, vect_f b)
{
        return (vect_f){a.x - b.x, a.y - b.y};
}

vect_f
vect_f_rotate(vect_f v, float theta)
{
        float cs = cosf(theta);
        float sn = sinf(theta);
        return (vect_f) { v.x * cs - v.y * sn, 
                          v.x * sn + v.y * cs };
}

float
vect3d_dot(vect3d a, vect3d b)
{
        return ((a.x * b.x) + (a.y * b.y) + (a.z * b.z));
}

/*
 * Project vector `v` onto plane given by vector `plane`. The resulting vector
 * is returned.
 *
 * Please take care to check if `plane` is not close to being the zero vector
 * before calling this function. Otherwise you may get divide-by-zero.
 */
vect3d
vect3d_plane_proj(vect3d v, vect3d plane)
{
        float dot_v_plane = vect3d_dot(v, plane);
        float dot_plane_plane = vect3d_dot(plane, plane);
        
        return (vect3d){
                v.x - plane.x * dot_v_plane / dot_plane_plane,
                v.y - plane.y * dot_v_plane / dot_plane_plane,
                v.z - plane.z * dot_v_plane / dot_plane_plane
        };
}

/*
 * Return true if bounding boxes intersect, false otherwise.
 */
int
bb_overlap(const BB *A, const BB *B)
{
        if (A->l >= B->r || A->r <= B->l || A->b >= B->t || A->t <= B->b)
                return 0;
        return 1;
}

/*
 * Pick smallest integer from all the arguments and return it.
 *
 * n            Number of int arguments.
 * ...          n integer numbers.
 */
static int
min_i(int n, ...)
{
        int i, min_value;
        va_list ap;

        min_value = INT_MAX;
        va_start(ap, n);
        while (n--) {
                i = va_arg(ap, int);
                if (i < min_value)
                        min_value = i;
        }
        va_end(ap);
        return min_value;
}

/*
 * Intersection area of two axis-aligned bounding boxes. Negative if they do not
 * intersect.
 */
int
bb_overlap_area(const BB *a, const BB *b)
{
        int wi, hi;
        
        assert(a != NULL && bb_valid(*a));
        assert(b != NULL && bb_valid(*b));

        wi = min_i(4, a->r - b->l, b->r - a->l, a->r - a->l, b->r - b->l);
        hi = min_i(4, a->t - b->b, b->t - a->b, a->t - a->b, b->t - b->b);
        return (wi < 0.0 || hi < 0.0) ? -abs(wi * hi) : wi * hi;
}

/*
 * What can be done to move box [b] out of box [a]? The result is stored in
 * bounding box pointed to by [resolve]. The result isn't really a valid
 * bounding box, but just four values -- distances in each of the four
 * directions (left, bottom, right, top) that can be added to box [b] in order
 * to move it out of intersection state.
 * The actual return value of the function is a boolean -- 0 if the two boxes
 * do not intersect, 1 if they do. Note that a positive result (1) is also
 * returned if the boxes only touch, but do not overlap.
 */
int
bb_intersect_resolve(const BB *a, const BB *b, BB *resolve)
{       
        assert(resolve != NULL);
        assert(a != NULL && bb_valid(*a));
        assert(b != NULL && bb_valid(*b));
                
        resolve->t = a->t - b->b;       /* Upward resolution distance. */
        resolve->r = a->r - b->l;       /* Resolution distance to the left. */
        if (resolve->t < 0 || resolve->t > (a->t - a->b) + (b->t - b->b) ||
            resolve->r < 0 || resolve->r > (a->r - a->l) + (b->r - b->l)) {
                return 0; /* No intersection. */
        }
                
        resolve->b = a->b - b->t;       /* Downward resolution distance. */
        resolve->l = a->l - b->r;       /* Resolution distance to the right. */ 
        return 1;
}

void
bb_add_vect(BB *bb, vect_i v)
{
        bb->l += v.x;
        bb->r += v.x;
        bb->b += v.y;
        bb->t += v.y;
}

void
bb_union(BB *bb, BB add)
{
        assert(bb && bb_valid(*bb) && bb_valid(add));
        
        if (add.l < bb->l)
                bb->l = add.l;
        if (add.r > bb->r)
                bb->r = add.r;
        if (add.b < bb->b)
                bb->b = add.b;
        if (add.t > bb->t)
                bb->t = add.t;
}
