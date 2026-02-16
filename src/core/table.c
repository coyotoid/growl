#include <growl.h>

GrowlTable *growl_unwrap_table(GrowlVM *vm, Growl obj) {
  if (GROWL_IS_NIL(obj) || GROWL_IS_NUM(obj))
    return NULL;
  GrowlObjectHeader *hdr = growl_unbox(vm, obj);
  if (hdr->type != GROWL_TYPE_TABLE)
    return NULL;
  return (GrowlTable *)(hdr + 1);
}
