#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "misc.h"

#ifndef M_PI
#define M_PI           3.14159265358979323846  /* pi */
#endif

#define TABSIZE           256
#define TABMASK           (TABSIZE - 1)
#define PERM(x)           perm[(x) & TABMASK]
#define INDEX(ix, iy, iz) PERM((ix) + PERM((iy) + PERM(iz)))

static unsigned char perm[TABSIZE] = {
225, 155, 210, 108, 175, 199, 221, 144, 203, 116, 70,  213, 69,  158, 33,  252,
5,   82,  173, 133, 222, 139, 174, 27,  9,   71,  90,  246, 75,  130, 91,  191,
169, 138, 2,   151, 194, 235, 81,  7,   25,  113, 228, 159, 205, 253, 134, 142,
248, 65,  224, 217, 22,  121, 229, 63,  89,  103, 96,  104, 156, 17,  201, 129,
36,  8,   165, 110, 237, 117, 231, 56,  132, 211, 152, 20,  181, 111, 239, 218,
170, 163, 51,  172, 157, 47,  80,  212, 176, 250, 87,  49,  99,  242, 136, 189,
162, 115, 44,  43,  124, 94,  150, 16,  141, 247, 32,  10,  198, 223, 255, 72,
53,  131, 84,  57,  220, 197, 58,  50,  208, 11,  241, 28,  3,   192, 62,  202,
18,  215, 153, 24,  76,  41,  15,  179, 39,  46,  55,  6,   128, 167, 23,  188,
106, 34,  187, 140, 164, 73,  112, 182, 244, 195, 227, 13,  35,  77,  196, 185,
26,  200, 226, 119, 31,  123, 168, 125, 249, 68,  183, 230, 177, 135, 160, 180,
12,  1,   243, 148, 102, 166, 38,  238, 251, 37,  240, 126, 64,  74,  161, 40,
184, 149, 171, 178, 101, 66,  29,  59,  146, 61,  254, 107, 42,  86,  154, 4,
236, 232, 120, 21,  233, 209, 45,  98,  193, 114, 78,  19,  206, 14,  118, 127,
48,  79,  147, 85,  30,  207, 219, 54,  88,  234, 190, 122, 95,  67,  143, 109,
137, 214, 145, 93,  92,  100, 245, 0,   216, 186, 60,  83,  105, 97,  204, 52
};

#define GRANULARITY 1000

float *gradientTab;
float gradientBank[3 * TABSIZE * GRANULARITY];

#define RANDMASK 0x7fffffff
#define RANDNBR ((rand_eglibc() & RANDMASK) / (float) RANDMASK)

float mutiply_and_sum_vectors(float vector1[3], float vector2[3]) {
    float sum = 0;
    unsigned i;
    for (i = 0; i < 3; i++) {
	sum += vector1[i] * vector2[i];
    }
    return sum;
}

void multiply_vector_with_matrix(float vector[3], float matrix[9]) {
    unsigned i;
    float tmp[3];
    for (i = 0; i < 3; i++) {
	tmp[i] = mutiply_and_sum_vectors(vector, matrix + (3 * i));
    }
    memcpy(vector, tmp, 3 * sizeof(float));
}

float vector_length(float a[3]) {
    return sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
}

void rotate_vector(float vector[3], float axis[3], float angle) {
    float len = vector_length(axis);
    if (len > 0.0) {
	float c = cos(-angle);
	float s = sin(-angle);
	float x = axis[0] / len;
	float y = axis[1] / len;
	float z = axis[2] / len;
	float matrix[9] = { x*x+(1-x*x)*c, x*y*(1-c)-z*s, x*z*(1-c)+y*s, 
			     x*y*(1-c)+z*s, y*y+(1-y*y)*c, y*z*(1-c)-x*s,
			     x*z*(1-c)-y*s, y*z*(1-c)+x*s, z*z+(1-z*z)*c };
	multiply_vector_with_matrix(vector, matrix);
    }
}

#include <stdio.h>

void rotate_gradients(float angle) {
    int x = GRANULARITY * angle / (2.0 * M_PI);
    gradientTab = gradientBank + 3 * TABSIZE * (x % GRANULARITY);
}

void gradientTabInitTab(int seed, float *table) {
    float z, r, theta;
    int i;
    srand_eglibc(seed);
    for(i = 0; i < TABSIZE; i++) {
	z = 1.0 - 2.0 * RANDNBR;
	r = sqrtf(1 - z * z);
	theta = (float) (2.0 * M_PI * RANDNBR);
	*table++ = r * cosf(theta);
	*table++ = r * sinf(theta);
	*table++ = z;
    }
}

void gradientTabInit(int seed) {
    int i, j;
    float axisTab[3 * TABSIZE];
    gradientTab = gradientBank;
    gradientTabInitTab(seed + 1, axisTab);
    for (i = 0; i < GRANULARITY; i++) {
	int offset = 3 * i * TABSIZE;
	gradientTabInitTab(seed, gradientBank + offset);
	for (j = 0; j < 3 * TABSIZE; j += 3) {
	    rotate_vector(gradientBank + offset + j, axisTab + j,
			  (2.0 * M_PI * i) / (float) GRANULARITY);
	}    
    }
}

float glattice(int ix, int iy, int iz, float fx, float fy, float fz) {
    float *g = &gradientTab[INDEX(ix,iy,iz) * 3];
    return g[0] * fx + g[1] * fy + g[2] * fz;
}

#define LERP(t, x0, x1) ((x0) + (t) * ((x1) - (x0)))
#define FLOOR(x) ((int)(x) - ((x) < 0 && (x) != (int)(x)))
#define SMOOTHSTEP(x) ((x) * (x) * (3 - 2 * (x)))

float noise(float x, float y, float z) {
    int ix, iy, iz;
    float wx, wy, wz;
    float fx0, fx1, fy0, fy1, fz0, fz1;
    float vx0, vx1, vy0, vy1, vz0, vz1;

    static int initialized = 0;
    if (!initialized) {
	gradientTabInit(12347);
	initialized = 1;
    }

    ix = FLOOR(x);
    fx0 = x - ix;
    fx1 = fx0 - 1;
    wx = SMOOTHSTEP(fx0);
    iy = FLOOR(y);
    fy0 = y - iy;
    fy1 = fy0 - 1;
    wy = SMOOTHSTEP(fy0);
    iz = FLOOR(z);
    fz0 = z - iz;
    fz1 = fz0 - 1;
    wz = SMOOTHSTEP(fz0);

    vx0 = glattice(ix,iy,iz,fx0,fy0,fz0);
    vx1 = glattice(ix+1,iy,iz,fx1,fy0,fz0);
    vy0 = LERP(wx, vx0, vx1);
    vx0 = glattice(ix,iy+1,iz,fx0,fy1,fz0);
    vx1 = glattice(ix+1,iy+1,iz,fx1,fy1,fz0);
    vy1 = LERP(wx, vx0, vx1);
    vz0 = LERP(wy, vy0, vy1);
    vx0 = glattice(ix,iy,iz+1,fx0,fy0,fz1);
    vx1 = glattice(ix+1,iy,iz+1,fx1,fy0,fz1);
    vy0 = LERP(wx, vx0, vx1);
    vx0 = glattice(ix,iy+1,iz+1,fx0,fy1,fz1);
    vx1 = glattice(ix+1,iy+1,iz+1,fx1,fy1,fz1);
    vy1 = LERP(wx, vx0, vx1);
    vz1 = LERP(wy, vy0, vy1);
    return ((LERP(wz, vz0, vz1)) + 0.7) * 0.71428573;
}

float fractal(float x, float y, float z, float s, int octaves) {
    int i;
    float sum = 0.0;
    for (i = 0; i < octaves; i++) {
	float m = s * powf(2.0, i);
	sum += noise(m * x, m * y, m * z) / powf(2.0, i);
    }
    return 0.5 * sum;
}
