#include <growl.h>

GrowlTable *growl_unwrap_table(Growl obj) {
  if (obj == 0 || GROWL_IMM(obj))
    return NULL;
  GrowlObjectHeader *hdr = GROWL_UNBOX(obj);
  if (hdr->type != GROWL_TYPE_TABLE)
    return NULL;
  return (GrowlTable *)(hdr + 1);
}

