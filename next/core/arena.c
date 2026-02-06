#include <growl.h>
#include <stdlib.h>
#include <string.h>

void growl_arena_init(GrowlGCArena *arena, size_t size) {
  arena->start = arena->free = malloc(size);
  if (arena->start == NULL)
    abort();
  arena->end = arena->start + size;
}

void growl_arena_free(GrowlGCArena *arena) {
  free(arena->start);
  arena->start = arena->end = arena->free = NULL;
}

void *growl_arena_alloc(GrowlGCArena *arena, size_t size, size_t align,
                        size_t count) {
  ptrdiff_t padding = -(uintptr_t)arena->start & (align - 1);
  ptrdiff_t available = arena->end - arena->start - padding;
  if (available < 0 || count > available / size)
    abort();
  void *p = arena->start + padding;
  arena->start += padding + count * size;
  return memset(p, 0, count * size);
}
