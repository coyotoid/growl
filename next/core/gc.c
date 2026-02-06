//
// Created by lobo on 2/5/26.
//

#include <assert.h>
#include <growl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALIGN(n) (((n) + 7) & ~7)

static int in_from(GrowlVM *vm, void *ptr) {
  const uint8_t *x = ptr;
  return (x >= vm->from.start && x < vm->from.end);
}

static Growl copy(GrowlVM *vm, GrowlObjectHeader *hdr) {
  assert(in_from(vm, hdr));
  assert(hdr->type != UINT32_MAX);
  size_t size = ALIGN(hdr->size);
  GrowlObjectHeader *new = (GrowlObjectHeader *)vm->to.free;
  vm->to.free += size;
  memcpy(new, hdr, size);
  hdr->type = UINT32_MAX;
  Growl *obj = (Growl *)(hdr + 1);
  *obj = (Growl)(new);
  return *obj;
}

static Growl forward(GrowlVM *vm, Growl obj) {
  if (obj == 0)
    return 0;
  if (!in_from(vm, (void *)obj))
    return obj;

  GrowlObjectHeader *hdr = (GrowlObjectHeader *)obj;
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
  hdr->size = size;
  return hdr;
}

GrowlObjectHeader *growl_gc_alloc_tenured(GrowlVM *vm, size_t size) {
  size = ALIGN(size);
  GrowlObjectHeader *hdr = growl_arena_alloc(&vm->arena, size, 8, 1);
  hdr->size = size;
  return hdr;
}

static void scan(GrowlVM *vm, GrowlObjectHeader *hdr) {
  switch (hdr->type) {
  case GROWL_STRING:
    break;
  case GROWL_LIST: {
    GrowlList *list = (GrowlList *)(hdr + 1);
    list->head = forward(vm, list->head);
    list->tail = forward(vm, list->tail);
    break;
  }
  case GROWL_TUPLE: {
    GrowlTuple *tuple = (GrowlTuple *)(hdr + 1);
    for (size_t i = 0; i < tuple->count; ++i) {
      tuple->data[i] = forward(vm, tuple->data[i]);
    }
    break;
  }
  case GROWL_QUOTATION: {
    GrowlQuotation *quot = (GrowlQuotation *)(hdr + 1);
    quot->constants = forward(vm, quot->constants);
    break;
  }
  case GROWL_COMPOSE: {
    GrowlCompose *comp = (GrowlCompose *)(hdr + 1);
    comp->first = forward(vm, comp->first);
    comp->second = forward(vm, comp->second);
    break;
  }
  case GROWL_CURRY: {
    GrowlCurry *comp = (GrowlCurry *)(hdr + 1);
    comp->value = forward(vm, comp->value);
    comp->callable = forward(vm, comp->callable);
    break;
  }
  case UINT32_MAX:
    fprintf(stderr, "gc: fwd pointer during scan\n");
    abort();
  default:
    fprintf(stderr, "gc: junk object type %" PRIu32 "\n", hdr->type);
    abort();
  }
}

static void gc_print_stats(GrowlVM *vm, const char *label) {
  size_t used = vm->from.free - vm->from.start;
  size_t total = vm->from.end - vm->from.start;
  fprintf(stderr, "[%s] used=%zu/%zu bytes (%.1f%%)\n", label, used, total,
          (double)used / (double)total * 100.0);
}

void growl_gc_collect(GrowlVM *vm) {
  uint8_t *gc_scan = vm->to.free;

  gc_print_stats(vm, "before GC");

  for (size_t i = 0; i < GROWL_STACK_SIZE; ++i) {
    vm->wst[i] = forward(vm, vm->wst[i]);
  }

  for (size_t i = 0; i < vm->root_count; ++i) {
    *vm->roots[i] = forward(vm, *vm->roots[i]);
  }

  uint8_t *arena_scan = vm->arena.start;
  while (arena_scan < vm->arena.free) {
    GrowlObjectHeader *hdr = (GrowlObjectHeader *)arena_scan;
    scan(vm, hdr);
    arena_scan += ALIGN(hdr->size);
  }

  while (gc_scan < vm->to.free) {
    GrowlObjectHeader *hdr = (GrowlObjectHeader *)gc_scan;
    scan(vm, hdr);
    gc_scan += ALIGN(hdr->size);
  }

  GrowlGCArena tmp = vm->from;
  vm->from = vm->to;
  vm->to = tmp;
  vm->to.free = vm->to.start;
  vm->scratch.free = vm->scratch.start;

  gc_print_stats(vm, "after GC");
}

void growl_gc_root(GrowlVM *vm, Growl *ptr) {
  if (vm->root_count >= vm->root_capacity) {
    size_t cap = vm->root_capacity == 0 ? 16 : vm->root_capacity * 2;
    Growl **data = realloc(vm->roots, cap * sizeof(Growl *));
    if (!data) {
      fprintf(stderr, "expanding roots array: oom\n");
      abort();
    }
    vm->root_capacity = cap;
    vm->roots = data;
  }
  vm->roots[vm->root_count++] = ptr;
}

size_t growl_gc_mark(GrowlVM *vm) { return vm->root_count; }

void growl_gc_reset(GrowlVM *vm, size_t mark) { vm->root_count = mark; }
