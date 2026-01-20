#include "arena.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

V *_arena_alloc(Ar *ar, I count, I size, I align) {
  I pad = -(U)ar->start & (align - 1);
  assert(count < (ar->end - ar->start - pad) / size);
  V *r = ar->start + pad;
  ar->start += pad + count * size;
  return memset(r, 0, count * size);
}

V arena_init(Ar *ar, Z size) {
  ar->data = malloc(size);
  ar->start = ar->data;
  ar->end = ar->start + size;
}

V arena_free(Ar *ar) {
  free(ar->data);
  ar->data = ar->start = ar->end = NULL;
}

char *arena_strdup(Ar *ar, const char *str) {
  Z len = strlen(str) + 1;
  char *copy = arena_alloc(ar, len, char);
  memcpy(copy, str, len);
  return copy;
}
