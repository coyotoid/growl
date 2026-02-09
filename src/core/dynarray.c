#include "dynarray.h"
#include <string.h>

void growl_dynarray_grow(void *slice, ptrdiff_t size, ptrdiff_t align,
                         GrowlArena *a) {
  struct {
    uint8_t *data;
    ptrdiff_t len;
    ptrdiff_t cap;
  } replica;
  memcpy(&replica, slice, sizeof(replica));
  if (!replica.data) {
    replica.cap = 1;
    replica.data = growl_arena_alloc(a, 2 * size, align, replica.cap);
  } else if (a->free == replica.data + size * replica.cap) {
    growl_arena_alloc(a, size, 1, replica.cap);
  } else {
    void *data = growl_arena_alloc(a, 2 * size, align, replica.cap);
    memcpy(data, replica.data, size * replica.len);
    replica.data = data;
  }
  replica.cap *= 2;
  memcpy(slice, &replica, sizeof(replica));
}
