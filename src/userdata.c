#include "userdata.h"
#include "gc.h"

O userdata_make(Vm *vm, V *data, Ut *kind) {
  Z size = sizeof(Hd) + sizeof(Ud);
  Hd *hdr = gc_alloc(vm, size);
  hdr->type = OBJ_USERDATA;
  Ud *ud = (Ud *)(hdr + 1);
  ud->kind = kind;
  ud->data = data;
  return BOX(hdr);
}

Ud *userdata_unwrap(O o, Ut *kind) {
  if (o == NIL || IMM(o))
    return NULL;
  Hd *hdr = UNBOX(o);
  if (hdr->type != OBJ_USERDATA)
    return NULL;
  Ud *ud = (Ud *)(hdr + 1);
  if (ud->kind != kind)
    return NULL;
  return ud;
}
