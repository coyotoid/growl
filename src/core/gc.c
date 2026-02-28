#include <assert.h>
#include <growl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GC_DEBUG 0
#define ALIGN(n) (((n) + 7) & ~7)

static Growl copy(GrowlVM *vm, GrowlObjectHeader *hdr) {
  assert(hdr->type != UINT32_MAX);
  size_t size = ALIGN(hdr->size);
  GrowlObjectHeader *new = (GrowlObjectHeader *)vm->to.free;
  vm->to.free += size;
  memcpy(new, hdr, size);
  uint64_t new_offset = (uint64_t)((uint8_t *)new - vm->to.start);
  Growl fwd_val = GROWL_MKPTR(GROWL_ARENA_NURSERY, new_offset);
  hdr->type = UINT32_MAX;
  Growl *fwd = (Growl *)(hdr + 1);
  *fwd = fwd_val;
  return fwd_val;
}

static Growl forward(GrowlVM *vm, Growl obj) {
  if (!GROWL_IS_PTR(obj))
    return obj;
  if (GROWL_PTR_ARENA(obj) != GROWL_ARENA_NURSERY)
    return obj;

  uint64_t offset = GROWL_PTR_OFFSET(obj);
  GrowlObjectHeader *hdr = (GrowlObjectHeader *)(vm->from.start + offset);
  if (hdr->type == UINT32_MAX) {
    Growl *fwd = (Growl *)(hdr + 1);
    return *fwd;
  }
  return copy(vm, hdr);
}

GrowlObjectHeader *growl_gc_alloc(GrowlVM *vm, size_t size) {
  size = ALIGN(size);
  if (vm->from.free + size > vm->from.end) {
    growl_gc_collect(vm);
    if (vm->from.free + size > vm->from.end) {
      fprintf(stderr, "gc: oom (requested %" PRIdPTR " bytes)\n", size);
      abort();
    }
  }
  GrowlObjectHeader *hdr = (GrowlObjectHeader *)vm->from.free;
  vm->from.free += size;
  memset(hdr, 0, size);
  hdr->size = size;
  return hdr;
}

GrowlObjectHeader *growl_gc_alloc_tenured(GrowlVM *vm, size_t size) {
  size = ALIGN(size);
  GrowlObjectHeader *hdr = growl_arena_alloc(&vm->tenured, size, 8, 1);
  hdr->size = size;
  return hdr;
}

static void scan(GrowlVM *vm, GrowlObjectHeader *hdr) {
  switch (hdr->type) {
  case GROWL_TYPE_STRING:
    break;
  case GROWL_TYPE_LIST: {
    GrowlList *list = (GrowlList *)(hdr + 1);
    list->head = forward(vm, list->head);
    list->tail = forward(vm, list->tail);
    break;
  }
  case GROWL_TYPE_TUPLE: {
    GrowlTuple *tuple = (GrowlTuple *)(hdr + 1);
    for (size_t i = 0; i < tuple->count; ++i) {
      tuple->data[i] = forward(vm, tuple->data[i]);
    }
    break;
  }
  case GROWL_TYPE_QUOTATION: {
    GrowlQuotation *quot = (GrowlQuotation *)(hdr + 1);
    quot->constants = forward(vm, quot->constants);
    break;
  }
  case GROWL_TYPE_COMPOSE: {
    GrowlCompose *comp = (GrowlCompose *)(hdr + 1);
    comp->first = forward(vm, comp->first);
    comp->second = forward(vm, comp->second);
    break;
  }
  case GROWL_TYPE_CURRY: {
    GrowlCurry *comp = (GrowlCurry *)(hdr + 1);
    comp->value = forward(vm, comp->value);
    comp->callable = forward(vm, comp->callable);
    break;
  }
  case GROWL_TYPE_ALIEN:
    break;
  case UINT32_MAX:
    fprintf(stderr, "gc: fwd pointer during scan\n");
    abort();
  default:
    fprintf(stderr, "gc: junk object type %" PRIu32 "\n", hdr->type);
    abort();
  }
}

#if GC_DEBUG
static void gc_print_stats(GrowlVM *vm, const char *label) {
  size_t nursery_used = vm->from.free - vm->from.start;
  size_t nursery_total = vm->from.end - vm->from.start;
  size_t tenured_used = vm->tenured.free - vm->tenured.start;
  size_t tenured_total = vm->tenured.end - vm->tenured.start;
  size_t arena_used = vm->arena.free - vm->arena.start;
  size_t arena_total = vm->arena.end - vm->arena.start;

  fprintf(stderr, "%s:\n", label);
  fprintf(stderr, "  arena:   %zu/%zu bytes (%.1f%%)\n", arena_used,
          arena_total, (double)arena_used / (double)arena_total * 100.0);
  fprintf(stderr, "  tenured: %zu/%zu bytes (%.1f%%)\n", tenured_used,
          tenured_total, (double)tenured_used / (double)tenured_total * 100.0);
  fprintf(stderr, "  nursery: %zu/%zu bytes (%.1f%%)\n", nursery_used,
          nursery_total, (double)nursery_used / (double)nursery_total * 100.0);
}
#endif

void growl_gc_collect(GrowlVM *vm) {
  uint8_t *gc_scan = vm->to.free;

#if GC_DEBUG
  fprintf(stderr, ">>> starting garbage collection\n");
  gc_print_stats(vm, "before GC");
#endif

  // Forward stacks
  for (Growl *obj = vm->wst; obj < vm->sp; ++obj) {
    *obj = forward(vm, *obj);
  }
  for (Growl *obj = vm->rst; obj < vm->rsp; ++obj) {
    *obj = forward(vm, *obj);
  }
  for (GrowlFrame *frame = vm->cst; frame < vm->csp; ++frame) {
    frame->next = forward(vm, frame->next);
  }

  // Forward GC roots
  for (size_t i = 0; i < vm->root_count; ++i) {
    *vm->roots[i] = forward(vm, *vm->roots[i]);
  }

  uint8_t *tenured_scan = vm->tenured.start;
  while (tenured_scan < vm->tenured.free) {
    GrowlObjectHeader *hdr = (GrowlObjectHeader *)tenured_scan;
    scan(vm, hdr);
    tenured_scan += ALIGN(hdr->size);
  }

  while (gc_scan < vm->to.free) {
    GrowlObjectHeader *hdr = (GrowlObjectHeader *)gc_scan;
    scan(vm, hdr);
    gc_scan += ALIGN(hdr->size);
  }

  gc_scan = vm->from.start;
  while (gc_scan < vm->from.free) {
    GrowlObjectHeader *hdr = (GrowlObjectHeader *)gc_scan;
    if (hdr->type != UINT32_MAX) {
      switch (hdr->type) {
      case GROWL_TYPE_ALIEN: {
        GrowlAlien *alien = (GrowlAlien *)(hdr + 1);
        if (alien->type->finalizer != NULL) {
          alien->type->finalizer(alien->data);
          alien->data = NULL;
        }
        break;
      }
      default:
        break;
      }
    }
    gc_scan += ALIGN(hdr->size);
  }

  GrowlArena tmp = vm->from;
  vm->from = vm->to;
  vm->to = tmp;
  vm->to.free = vm->to.start;
  vm->scratch.free = vm->scratch.start;

#if GC_DEBUG
  gc_print_stats(vm, "after GC");
  fprintf(stderr, ">>> garbage collection done\n");
#endif
}

void growl_gc_root(GrowlVM *vm, Growl *ptr) {
  if (vm->root_count >= vm->root_capacity) {
    size_t cap = vm->root_capacity == 0 ? 16 : vm->root_capacity * 2;
    Growl **data = realloc(vm->roots, cap * sizeof(Growl *));
    if (!data) {
      fprintf(stderr, "roots: oom\n");
      abort();
    }
    vm->root_capacity = cap;
    vm->roots = data;
  }
  vm->roots[vm->root_count++] = ptr;
}

size_t growl_gc_mark(GrowlVM *vm) { return vm->root_count; }

void growl_gc_reset(GrowlVM *vm, size_t mark) { vm->root_count = mark; }
