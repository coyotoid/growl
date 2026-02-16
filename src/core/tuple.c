#include <growl.h>

GrowlTuple *growl_unwrap_tuple(GrowlVM *vm, Growl obj) {
  if (GROWL_IS_NIL(obj) || GROWL_IS_NUM(obj))
    return NULL;
  GrowlObjectHeader *hdr = growl_unbox(vm, obj);
  if (hdr->type != GROWL_TYPE_TUPLE)
    return NULL;
  return (GrowlTuple *)(hdr + 1);
}
