#include <growl.h>

Growl growl_make_alien(GrowlVM *vm, GrowlAlienType *type, void *data) {
  size_t size = sizeof(GrowlObjectHeader) + sizeof(GrowlAlien);
  GrowlObjectHeader *hdr = growl_gc_alloc(vm, size);
  hdr->type = GROWL_TYPE_ALIEN;
  GrowlAlien *alien = (GrowlAlien *)(hdr + 1);
  alien->type = type;
  alien->data = data;
  return growl_box_nursery(vm, hdr);
}

Growl growl_make_alien_tenured(GrowlVM *vm, GrowlAlienType *type, void *data) {
  size_t size = sizeof(GrowlObjectHeader) + sizeof(GrowlAlien);
  GrowlObjectHeader *hdr = growl_gc_alloc_tenured(vm, size);
  hdr->type = GROWL_TYPE_ALIEN;
  GrowlAlien *alien = (GrowlAlien *)(hdr + 1);
  alien->type = type;
  alien->data = data;
  return growl_box_tenured(vm, hdr);
}

GrowlAlien *growl_unwrap_alien(GrowlVM *vm, Growl obj, GrowlAlienType *type) {
  if (GROWL_IS_NIL(obj) || GROWL_IS_NUM(obj))
    return NULL;
  GrowlObjectHeader *hdr = growl_unbox(vm, obj);
  if (hdr->type != GROWL_TYPE_ALIEN)
    return NULL;
  GrowlAlien *alien = (GrowlAlien *)(hdr + 1);
  if (type && alien->type != type)
    return NULL;
  return alien;
}
