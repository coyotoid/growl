#include <growl.h>

Growl growl_make_tuple(GrowlVM *vm, size_t count) {
  size_t total_size = sizeof(GrowlObjectHeader) + sizeof(GrowlTuple) + sizeof(Growl) * count;
  GrowlObjectHeader *hdr = growl_gc_alloc(vm, total_size);
  hdr->type = GROWL_TYPE_TUPLE;
  GrowlTuple *tuple = (GrowlTuple *)(hdr + 1);
  tuple->count = count;
  for (size_t i = 0; i < count; i++)
    tuple->data[i] = GROWL_NIL;
  return growl_box_nursery(vm, hdr);
}

Growl growl_make_tuple_tenured(GrowlVM *vm, size_t count) {
  size_t total_size = sizeof(GrowlObjectHeader) + sizeof(GrowlTuple) + sizeof(Growl) * count;
  GrowlObjectHeader *hdr = growl_gc_alloc_tenured(vm, total_size);
  hdr->type = GROWL_TYPE_TUPLE;
  GrowlTuple *tuple = (GrowlTuple *)(hdr + 1);
  tuple->count = count;
  for (size_t i = 0; i < count; i++)
    tuple->data[i] = GROWL_NIL;
  return growl_box_tenured(vm, hdr);
}

GrowlTuple *growl_unwrap_tuple(GrowlVM *vm, Growl obj) {
  if (GROWL_IS_NIL(obj) || GROWL_IS_NUM(obj))
    return NULL;
  GrowlObjectHeader *hdr = growl_unbox(vm, obj);
  if (hdr->type != GROWL_TYPE_TUPLE)
    return NULL;
  return (GrowlTuple *)(hdr + 1);
}
