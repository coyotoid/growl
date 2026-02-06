#include <growl.h>

GrowlTuple *growl_unwrap_tuple(Growl obj) {
  if (obj == 0 || GROWL_IMM(obj))
    return NULL;
  GrowlObjectHeader *hdr = GROWL_UNBOX(obj);
  if (hdr->type != GROWL_TUPLE)
    return NULL;
  return (GrowlTuple *)(hdr + 1);
}
