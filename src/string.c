#include <string.h>

#include "string.h"

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

O string_concat(Vm *vm, Str *a, Str *b) {
  O new_obj = string_make(vm, "", a->len + b->len);
  Str *new = (Str *)(UNBOX(new_obj) + 1);

  memcpy(new->data, a->data, a->len);
  memcpy(new->data + a->len, b->data, b->len);
  new->data[a->len + b->len] = 0;

  return new_obj;
}
