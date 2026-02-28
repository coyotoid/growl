#include <growl.h>

Growl growl_cons(GrowlVM *vm, Growl head, Growl tail) {
  size_t mark = growl_gc_mark(vm);
  growl_gc_root(vm, &head);
  growl_gc_root(vm, &tail);

  size_t size = sizeof(GrowlObjectHeader) + sizeof(GrowlList);
  GrowlObjectHeader *hdr = growl_gc_alloc(vm, size);
  hdr->type = GROWL_TYPE_LIST;
  GrowlList *lst = (GrowlList *)(hdr + 1);
  lst->head = head;
  lst->tail = tail;

  growl_gc_reset(vm, mark);
  return growl_box_nursery(vm, hdr);
}

Growl growl_cons_tenured(GrowlVM *vm, Growl head, Growl tail) {
  size_t size = sizeof(GrowlObjectHeader) + sizeof(GrowlList);
  GrowlObjectHeader *hdr = growl_gc_alloc_tenured(vm, size);
  hdr->type = GROWL_TYPE_LIST;
  GrowlList *lst = (GrowlList *)(hdr + 1);
  lst->head = head;
  lst->tail = tail;
  return growl_box_tenured(vm, hdr);
}

GrowlList *growl_unwrap_list(GrowlVM *vm, Growl obj) {
  if (GROWL_IS_NIL(obj) || GROWL_IS_NUM(obj))
    return NULL;
  GrowlObjectHeader *hdr = growl_unbox(vm, obj);
  if (hdr->type != GROWL_TYPE_LIST)
    return NULL;
  return (GrowlList *)(hdr + 1);
}

size_t growl_list_length(GrowlVM *vm, Growl lst) {
  size_t len = 0;
  while (!GROWL_IS_NIL(lst)) {
    GrowlObjectHeader *hdr = growl_unbox(vm, lst);
    GrowlList *node = (GrowlList *)(hdr + 1);
    lst = node->tail;
    len++;
  }
  return len;
}

Growl growl_list_to_tuple(GrowlVM *vm, Growl lst) {
  size_t mark = growl_gc_mark(vm);
  growl_gc_root(vm, &lst);

  size_t len = growl_list_length(vm, lst);
  size_t i = 0;

  size_t tuple_size =
      sizeof(GrowlObjectHeader) + sizeof(GrowlTuple) + sizeof(Growl) * len;
  GrowlObjectHeader *hdr = growl_gc_alloc(vm, tuple_size);
  hdr->type = GROWL_TYPE_TUPLE;
  GrowlTuple *tuple = (GrowlTuple *)(hdr + 1);
  tuple->count = len;

  while (!GROWL_IS_NIL(lst)) {
    GrowlObjectHeader *node_hdr = growl_unbox(vm, lst);
    GrowlList *node = (GrowlList *)(node_hdr + 1);
    lst = node->tail;
    tuple->data[i++] = node->head;
  }

  growl_gc_reset(vm, mark);
  return growl_box_nursery(vm, hdr);
}
