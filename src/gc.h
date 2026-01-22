#ifndef GC_H
#define GC_H

#include "common.h"
#include "object.h"

#define GC_DEBUG 0
#if GC_DEBUG
#define HEAP_BYTES (8 * 1024)
#else
#define HEAP_BYTES (4 * 1024 * 1024)
#endif

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
V gc_init(Gc *);
V gc_deinit(Gc *);

typedef struct Vm Vm;

V gc_collect(Vm *);
Hd *gc_alloc(Vm *, Z);

#endif
