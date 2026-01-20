#ifndef ARENA_H
#define ARENA_H

#include "common.h"

typedef struct Ar {
  U8 *data;
  U8 *start, *end;
} Ar;

#define arena_alloc(a, n, t) (t *)_arena_alloc(a, n, sizeof(t), _Alignof(t))
V *_arena_alloc(Ar *, ptrdiff_t, ptrdiff_t, ptrdiff_t);

V arena_init(Ar *, Z);
V arena_free(Ar *);
char *arena_strdup(Ar *, const char *);

#endif
