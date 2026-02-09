#ifndef GROWL_DYNARRAY_H
#define GROWL_DYNARRAY_H

// See https://nullprogram.com/blog/2023/10/05/

#include <growl.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define push(s, a)                                                             \
  ({                                                                           \
    typeof(s) s_ = (s);                                                        \
    typeof(a) a_ = (a);                                                        \
    if (s_->count >= s_->capacity) {                                           \
      __grow(s_, sizeof(*s_->data), _Alignof(*s_->data), a_);                    \
    }                                                                          \
    s_->data + s_->count++;                                                    \
  })

static void __grow(void *slice, ptrdiff_t size, ptrdiff_t align, GrowlArena *a) {
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

#endif // GROWL_DYNARRAY_H
