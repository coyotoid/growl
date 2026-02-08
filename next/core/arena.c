#include <growl.h>
#include <stdlib.h>
#include <string.h>

void growl_arena_init(GrowlArena *arena, size_t size) {
  arena->start = arena->free = malloc(size);
  if (arena->start == NULL)
    abort();
  arena->end = arena->start + size;
}

void growl_arena_free(GrowlArena *arena) {
  free(arena->start);
  arena->start = arena->end = arena->free = NULL;
}

void *growl_arena_alloc(GrowlArena *arena, size_t size, size_t align,
                        size_t count) {
  ptrdiff_t padding = -(uintptr_t)arena->free & (align - 1);
  ptrdiff_t available = arena->end - arena->free - padding;
  if (available < 0 || count > available / size) {
    fprintf(stderr, "arena: out of memory :(");
    abort();
  }
  void *p = arena->free + padding;
  arena->free += padding + count * size;
  return memset(p, 0, count * size);
}

char *growl_arena_strdup(GrowlArena *ar, const char *str) {
  size_t len = strlen(str) + 1;
  char *copy = growl_arena_new(ar, char, len);
  memcpy(copy, str, len);
  return copy;
}
