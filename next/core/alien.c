#include <growl.h>

Growl growl_make_alien(GrowlVM *vm, GrowlAlienType *type, void *data) {
  size_t size = sizeof(GrowlObjectHeader) + sizeof(GrowlAlien);
  GrowlObjectHeader *hdr = growl_gc_alloc(vm, size);
  hdr->type = GROWL_TYPE_ALIEN;
  GrowlAlien *alien = (GrowlAlien *)(hdr + 1);
  alien->type = type;
  alien->data = data;
  return GROWL_BOX(hdr);
}

Growl growl_make_alien_tenured(GrowlVM *vm, GrowlAlienType *type, void *data) {
  size_t size = sizeof(GrowlObjectHeader) + sizeof(GrowlAlien);
  GrowlObjectHeader *hdr = growl_gc_alloc_tenured(vm, size);
  hdr->type = GROWL_TYPE_ALIEN;
  GrowlAlien *alien = (GrowlAlien *)(hdr + 1);
  alien->type = type;
  alien->data = data;
  return GROWL_BOX(hdr);
}

GrowlAlien *growl_unwrap_alien(Growl obj, GrowlAlienType *type) {
  if (obj == GROWL_NIL || GROWL_IMM(obj))
    return NULL;
  GrowlObjectHeader *hdr = GROWL_UNBOX(obj);
  if (hdr->type != GROWL_TYPE_ALIEN)
    return NULL;
  GrowlAlien *alien = (GrowlAlien *)(hdr + 1);
  if (alien->type != type)
    return NULL;
  return alien;
}
