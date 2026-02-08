#include <growl.h>

uint32_t growl_type(Growl obj) {
  if (obj == GROWL_NIL)
    return GROWL_TYPE_NIL;
  if (GROWL_IMM(obj))
    return GROWL_TYPE_NUMBER;
  GrowlObjectHeader *hdr = GROWL_UNBOX(obj);
  return hdr->type;
}

int growl_equals(Growl a, Growl b) {
  if (a != b)
    return 0;
  if (growl_type(a) != growl_type(b))
    return 0;
  return 1;
}
