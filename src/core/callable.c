#include <growl.h>
#include <string.h>

int growl_callable(GrowlVM *vm, Growl obj) {
  if (GROWL_IS_NIL(obj) || GROWL_IS_NUM(obj))
    return 0;
  GrowlObjectHeader *hdr = growl_unbox(vm, obj);
  switch (hdr->type) {
  case GROWL_TYPE_QUOTATION:
  case GROWL_TYPE_COMPOSE:
  case GROWL_TYPE_CURRY:
    return 1;
  case GROWL_TYPE_ALIEN: {
    GrowlAlien *alien = (GrowlAlien *)(hdr + 1);
    return alien->type && alien->type->call != NULL;
  }
  default:
    return 0;
  }
}

Growl growl_make_quotation(GrowlVM *vm, const uint8_t *code, size_t code_size,
                           const Growl *constants, size_t constants_size) {
  Growl constants_obj;

  if (constants_size == 0) {
    constants_obj = GROWL_NIL;
  } else {
    size_t constants_obj_size = sizeof(GrowlObjectHeader) + sizeof(GrowlTuple) +
                                constants_size * sizeof(Growl);
    GrowlObjectHeader *constants_hdr =
        growl_gc_alloc_tenured(vm, constants_obj_size);
    constants_hdr->type = GROWL_TYPE_TUPLE;
    GrowlTuple *constants_tuple = (GrowlTuple *)(constants_hdr + 1);

    constants_tuple->count = constants_size;
    for (size_t i = 0; i < constants_size; ++i) {
      constants_tuple->data[i] = constants[i];
    }
    constants_obj = growl_box_tenured(vm, constants_hdr);
  }

  size_t quotation_obj_size =
      sizeof(GrowlObjectHeader) + sizeof(GrowlQuotation) + code_size;
  GrowlObjectHeader *quotation_hdr =
      growl_gc_alloc_tenured(vm, quotation_obj_size);
  quotation_hdr->type = GROWL_TYPE_QUOTATION;
  GrowlQuotation *quotation = (GrowlQuotation *)(quotation_hdr + 1);

  quotation->constants = constants_obj;
  quotation->count = code_size;
  memcpy(quotation->data, code, code_size);

  return growl_box_tenured(vm, quotation_hdr);
}

GrowlQuotation *growl_unwrap_quotation(GrowlVM *vm, Growl obj) {
  if (GROWL_IS_NIL(obj) || GROWL_IS_NUM(obj))
    return NULL;
  GrowlObjectHeader *hdr = growl_unbox(vm, obj);
  if (hdr->type != GROWL_TYPE_QUOTATION)
    return NULL;
  return (GrowlQuotation *)(hdr + 1);
}

Growl growl_compose(GrowlVM *vm, Growl first, Growl second) {
  if (!growl_callable(vm, first))
    return GROWL_NIL;
  if (!growl_callable(vm, second))
    return GROWL_NIL;
  size_t mark = growl_gc_mark(vm);
  growl_gc_root(vm, &first);
  growl_gc_root(vm, &second);
  size_t size = sizeof(GrowlObjectHeader) + sizeof(GrowlCompose);
  GrowlObjectHeader *hdr = growl_gc_alloc(vm, size);
  hdr->type = GROWL_TYPE_COMPOSE;
  GrowlCompose *comp = (GrowlCompose *)(hdr + 1);
  comp->first = first;
  comp->second = second;
  growl_gc_reset(vm, mark);
  return growl_box_nursery(vm, hdr);
}

GrowlCompose *growl_unwrap_compose(GrowlVM *vm, Growl obj) {
  if (GROWL_IS_NIL(obj) || GROWL_IS_NUM(obj))
    return NULL;
  GrowlObjectHeader *hdr = growl_unbox(vm, obj);
  if (hdr->type != GROWL_TYPE_COMPOSE)
    return NULL;
  return (GrowlCompose *)(hdr + 1);
}

Growl growl_curry(GrowlVM *vm, Growl value, Growl callable) {
  if (!growl_callable(vm, callable))
    return GROWL_NIL;
  size_t mark = growl_gc_mark(vm);
  growl_gc_root(vm, &value);
  growl_gc_root(vm, &callable);
  size_t size = sizeof(GrowlObjectHeader) + sizeof(GrowlCurry);
  GrowlObjectHeader *hdr = growl_gc_alloc(vm, size);
  hdr->type = GROWL_TYPE_CURRY;
  GrowlCurry *comp = (GrowlCurry *)(hdr + 1);
  comp->value = value;
  comp->callable = callable;
  growl_gc_reset(vm, mark);
  return growl_box_nursery(vm, hdr);
}

GrowlCurry *growl_unwrap_curry(GrowlVM *vm, Growl obj) {
  if (GROWL_IS_NIL(obj) || GROWL_IS_NUM(obj))
    return NULL;
  GrowlObjectHeader *hdr = growl_unbox(vm, obj);
  if (hdr->type != GROWL_TYPE_CURRY)
    return NULL;
  return (GrowlCurry *)(hdr + 1);
}
