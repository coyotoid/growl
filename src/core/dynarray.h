#ifndef GROWL_DYNARRAY_H
#define GROWL_DYNARRAY_H

// See https://nullprogram.com/blog/2023/10/05/

#include <growl.h>
#include <stddef.h>

#define growl_dynarray_push(s, a)                                                             \
  ({                                                                           \
    typeof(s) s_ = (s);                                                        \
    typeof(a) a_ = (a);                                                        \
    if (s_->count >= s_->capacity) {                                           \
      growl_dynarray_grow(s_, sizeof(*s_->data), _Alignof(*s_->data), a_);     \
    }                                                                          \
    s_->data + s_->count++;                                                    \
  })

void growl_dynarray_grow(void *slice, ptrdiff_t size, ptrdiff_t align,
                         GrowlArena *a);

#endif // GROWL_DYNARRAY_H
