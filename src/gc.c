#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "gc.h"
#include "object.h"
#include "vendor/yar.h"
#include "vm.h"

#define ALIGN(n) (((n) + 7) & ~7)
static inline int infrom(Gc *gc, V *ptr) {
  const U8 *x = (const U8 *)ptr;
  return (x >= gc->from.start && x < gc->from.end);
}

V gc_addroot(Gc *gc, O *ptr) { *yar_append(&gc->roots) = ptr; }
I gc_mark(Gc *gc) { return gc->roots.count; }
V gc_reset(Gc *gc, I mark) { gc->roots.count = mark; }

static O copy(Gc *gc, Hd *hdr) {
  assert(infrom(gc, hdr));
  assert(hdr->type != OBJ_FWD);

  Z sz = ALIGN(hdr->size);
  Hd *new = (Hd *)gc->to.free;
  gc->to.free += sz;
  memcpy(new, hdr, sz);

  hdr->type = OBJ_FWD;
  O *obj = (O *)(hdr + 1);
  *obj = BOX(new);
  return *obj;
}

static O forward(Gc *gc, O obj) {
  if (obj == 0)
    return 0;
  if (IMM(obj))
    return obj;
  if (!infrom(gc, (V *)obj))
    return obj;

  Hd *hdr = UNBOX(obj);
  if (hdr->type == OBJ_FWD) {
    O *o = (O *)(hdr + 1);
    return *o;
  } else {
    return copy(gc, hdr);
  }
}

#if GC_DEBUG
static V printstats(Gc *gc, const char *label) {
  Z used = (Z)(gc->from.free - gc->from.start);
  fprintf(stderr, "[%s] used=%zu/%zu bytes (%.1f%%)\n", label, used,
          (Z)HEAP_BYTES, (F)used / (F)HEAP_BYTES * 100.0);
}
#endif

V gc_collect(Vm *vm) {
  Gc *gc = &vm->gc;
  uint8_t *scan = gc->to.free;

#if GC_DEBUG
  printstats(gc, "before GC");
#endif

  for (Z i = 0; i < gc->roots.count; i++) {
    O *o = gc->roots.items[i];
    *o = forward(gc, *o);
  }

  Dt *dstack[256];
  Dt **dsp = dstack;
  *dsp++ = vm->dictionary;

  // Forward constants referenced by dictionary entries
  while (dsp > dstack) {
    Dt *node = *--dsp;
    if (!node)
      continue;
    if (node->chunk != NULL) {
      for (Z i = 0; i < node->chunk->constants.count; i++) {
        node->chunk->constants.items[i] =
            forward(gc, node->chunk->constants.items[i]);
      }
    }
    for (I i = 0; i < 4; i++) {
      if (node->child[i] != NULL)
        *dsp++ = node->child[i];
    }
  }

  while (scan < gc->to.free) {
    if (scan >= gc->to.end) {
      fprintf(stderr, "fatal GC error: out of memory\n");
      abort();
    }
    Hd *hdr = (Hd *)scan;
    switch (hdr->type) {
      // TODO: the rest of the owl
    case OBJ_QUOT: {
      Bc **chunk_ptr = (Bc **)(hdr + 1);
      Bc *chunk = *chunk_ptr;
      for (Z i = 0; i < chunk->constants.count; i++)
        chunk->constants.items[i] = forward(gc, chunk->constants.items[i]);
      break;
    }
    case OBJ_FWD:
      fprintf(stderr, "fatal GC error: forwarding pointer in to-space\n");
      abort();
    default:
      fprintf(stderr, "GC warning: junk object type %" PRId32 "\n", hdr->type);
    }
    scan += ALIGN(hdr->size);
  }

  scan = gc->from.start;
  while (scan < gc->from.free) {
    Hd *hdr = (Hd *)scan;
    if (hdr->type != OBJ_FWD) {
      switch (hdr->type) {
      case OBJ_QUOT: {
        Bc **chunk_ptr = (Bc **)(hdr + 1);
        chunk_release(*chunk_ptr);
        break;
      }
      default:
        break;
      }
    }
    scan += ALIGN(hdr->size);
  }

  Gs tmp = gc->from;
  gc->from = gc->to;
  gc->to = tmp;
  gc->to.free = gc->to.start;

#if GC_DEBUG
  printstats(gc, "after GC");
#endif
}

Hd *gc_alloc(Vm *vm, Z sz) {
  Gc *gc = &vm->gc;
  sz = ALIGN(sz);
  if (gc->from.free + sz > gc->from.end) {
    gc_collect(vm);
    if (gc->from.free + sz > gc->from.end) {
      fprintf(stderr, "out of memory (requested %" PRIdPTR "bytes\n", sz);
      abort();
    }
  }
  Hd *hdr = (Hd *)gc->from.free;
  gc->from.free += sz;
  hdr->size = sz;
  return hdr;
}

V gc_init(Gc *gc) {
  gc->from.start = malloc(HEAP_BYTES);
  if (!gc->from.start)
    goto fatal;
  gc->from.end = gc->from.start + HEAP_BYTES;
  gc->from.free = gc->from.start;

  gc->to.start = malloc(HEAP_BYTES);
  if (!gc->to.start)
    goto fatal;
  gc->to.end = gc->to.start + HEAP_BYTES;
  gc->to.free = gc->to.start;

  gc->roots.capacity = 0;
  gc->roots.count = 0;
  gc->roots.items = NULL;
  return;

fatal:
  fprintf(stderr, "failed to allocate heap space\n");
  abort();
}

V gc_deinit(Gc *gc) {
  free(gc->from.start);
  free(gc->to.start);
  yar_free(&gc->roots);
}
