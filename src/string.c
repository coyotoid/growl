#include <string.h>

#include "string.h"
#include "src/gc.h"

O string_make(Vm *vm, const char *str, I len) {
  if (len < 0)
    len = strlen(str);
  Z size = sizeof(Hd) + sizeof(Str) + len + 1;
  Hd *hdr = gc_alloc(vm, size);
  hdr->type = OBJ_STR;
  Str *s = (Str *)(hdr + 1);
  s->len = len;
  memcpy(s->data, str, len);
  s->data[len] = 0;
  return BOX(hdr);
}

Str *string_unwrap(O o) {
  if (o == NIL || IMM(o))
    return NULL;
  Hd *hdr = UNBOX(o);
  if (hdr->type != OBJ_STR)
    return NULL;
  return (Str *)(hdr + 1);
}

O string_concat(Vm *vm, O a_obj, O b_obj) {
  I mark = gc_mark(&vm->gc);
  gc_addroot(&vm->gc, &a_obj);
  gc_addroot(&vm->gc, &b_obj);

  Str *as = string_unwrap(a_obj);
  Str *bs = string_unwrap(b_obj);
  I a_len = as->len;
  I b_len = bs->len;

  O new = string_make(vm, "", a_len + b_len);

  as = string_unwrap(a_obj);
  bs = string_unwrap(b_obj);
  Str *news = (Str *)(UNBOX(new) + 1);

  memcpy(news->data, as->data, a_len);
  memcpy(news->data + a_len, bs->data, b_len);
  news->data[a_len + b_len] = 0;

  gc_reset(&vm->gc, mark);

  return new;
}
