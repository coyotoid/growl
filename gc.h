#ifndef GC_H
#define GC_H

#include "common.h"
#include "object.h"

#define GC_DEBUG 1
#define HEAP_BYTES (4 * 1024 * 1024)

typedef struct Gs {
  U8 *start, *end;
  U8 *free;
} Gs;

typedef struct Gc {
  Gs from, to;
  struct {
    O **items;
    Z count, capacity;
  } roots;
} Gc;

V gc_addroot(Gc *, O *);
I gc_mark(Gc *);
V gc_reset(Gc *, I);
V gc_collect(Gc *);
Hd *gc_alloc(Gc *, Z);
V gc_init(Gc *);
V gc_deinit(Gc *);

#endif
