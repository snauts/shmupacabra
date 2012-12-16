#ifndef GAME2D_MISC_H
#define GAME2D_MISC_H

#include <SDL.h>
#include <stdint.h>
#include "camera.h"

void             prep_stmt(sqlite3_stmt **stmt, const char *sql);

const char      *object_name(void *obj);
unsigned         nearest_pow2(unsigned number);
int              check_extension(const char *name);

void             srand_eglibc(unsigned);
long             rand_eglibc(void);

int              random_rangepick(int start, int end);
float            random_rangepickf(float start, float end, float step);

const char      *x2_add(const char *filename, char *strbuf, unsigned bufsize);
const char      *x2_strip(const char *filename, char *strbuf, unsigned bufsize);

void check_gl_errors__(void);
#ifndef NDEBUG
#define check_gl_errors check_gl_errors__
#else
#define check_gl_errors(...) (void)0
#endif

#endif
