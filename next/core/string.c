#include <growl.h>
#include <string.h>

Growl growl_make_string(GrowlVM *vm, size_t len) {
  size_t size = sizeof(GrowlObjectHeader) + sizeof(GrowlString) + len;
  GrowlObjectHeader *hdr = growl_gc_alloc(vm, size);
  hdr->type = GROWL_TYPE_STRING;
  GrowlString *str = (GrowlString *)(hdr + 1);
  str->len = len;
  memset(str->data, 0, len);
  return GROWL_BOX(hdr);
}

Growl growl_wrap_string(GrowlVM *vm, const char *cstr) {
  size_t len = strlen(cstr);
  size_t size = sizeof(GrowlObjectHeader) + sizeof(GrowlString) + len + 1;
  GrowlObjectHeader *hdr = growl_gc_alloc(vm, size);
  hdr->type = GROWL_TYPE_STRING;
  GrowlString *str = (GrowlString *)(hdr + 1);
  str->len = len;
  memcpy(str->data, cstr, len);
  str->data[len] = 0;
  return GROWL_BOX(hdr);
}

GrowlString *growl_unwrap_string(Growl obj) {
  if (obj == 0 || GROWL_IMM(obj))
    return NULL;
  GrowlObjectHeader *hdr = GROWL_UNBOX(obj);
  if (hdr->type != GROWL_TYPE_STRING)
    return NULL;
  return (GrowlString *)(hdr + 1);
}
