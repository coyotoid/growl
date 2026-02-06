#include <growl.h>
#include <string.h>

int growl_callable(Growl obj) {
  if (obj == GROWL_NIL || GROWL_IMM(obj))
    return 0;
  GrowlObjectHeader *hdr = GROWL_UNBOX(obj);
  switch (hdr->type) {
  case GROWL_QUOTATION:
  case GROWL_COMPOSE:
  case GROWL_CURRY:
    return 1;
  default:
    return 0;
  }
}

Growl growl_make_quotation(GrowlVM *vm, const uint8_t *code, size_t code_size,
                           const Growl *constants, size_t constants_size) {
  size_t constants_obj_size = sizeof(GrowlObjectHeader) + sizeof(GrowlTuple) +
                              constants_size * sizeof(Growl);
  GrowlObjectHeader *constants_hdr =
      growl_gc_alloc_tenured(vm, constants_obj_size);
  constants_hdr->type = GROWL_TUPLE;
  GrowlTuple *constants_tuple = (GrowlTuple *)(constants_hdr + 1);

  constants_tuple->count = constants_size;
  for (size_t i = 0; i < constants_size; ++i) {
    constants_tuple->data[i] = constants[i];
  }

  size_t quotation_obj_size =
      sizeof(GrowlObjectHeader) + sizeof(GrowlQuotation) + code_size;
  GrowlObjectHeader *quotation_hdr =
      growl_gc_alloc_tenured(vm, quotation_obj_size);
  quotation_hdr->type = GROWL_QUOTATION;
  GrowlQuotation *quotation = (GrowlQuotation *)(quotation_hdr + 1);

  quotation->constants = GROWL_BOX(constants_hdr);
  quotation->count = code_size;
  memcpy(quotation->data, code, code_size);

  return GROWL_BOX(quotation_hdr);
}

GrowlQuotation *growl_unwrap_quotation(Growl obj) {
  if (obj == GROWL_NIL || GROWL_IMM(obj))
    return NULL;
  GrowlObjectHeader *hdr = GROWL_UNBOX(obj);
  if (hdr->type != GROWL_QUOTATION)
    return NULL;
  return (GrowlQuotation *)(hdr + 1);
}

Growl growl_compose(GrowlVM *vm, Growl first, Growl second) {
  if (!growl_callable(first))
    return GROWL_NIL;
  if (!growl_callable(second))
    return GROWL_NIL;
  size_t size = sizeof(GrowlObjectHeader) + sizeof(GrowlCompose);
  GrowlObjectHeader *hdr = growl_gc_alloc(vm, size);
  hdr->type = GROWL_COMPOSE;
  GrowlCompose *comp = (GrowlCompose *)(hdr + 1);
  comp->first = first;
  comp->second = second;
  return GROWL_BOX(hdr);
}

GrowlCompose *growl_unwrap_compose(Growl obj) {
  if (obj == GROWL_NIL || GROWL_IMM(obj))
    return NULL;
  GrowlObjectHeader *hdr = GROWL_UNBOX(obj);
  if (hdr->type != GROWL_COMPOSE)
    return NULL;
  return (GrowlCompose *)(hdr + 1);
}

Growl growl_curry(GrowlVM *vm, Growl value, Growl callable) {
  if (!growl_callable(callable))
    return GROWL_NIL;
  size_t size = sizeof(GrowlObjectHeader) + sizeof(GrowlCurry);
  GrowlObjectHeader *hdr = growl_gc_alloc(vm, size);
  hdr->type = GROWL_CURRY;
  GrowlCurry *comp = (GrowlCurry *)(hdr + 1);
  comp->value = value;
  comp->callable = callable;
  return GROWL_BOX(hdr);
}

GrowlCurry *growl_unwrap_curry(Growl obj) {
  if (obj == GROWL_NIL || GROWL_IMM(obj))
    return NULL;
  GrowlObjectHeader *hdr = GROWL_UNBOX(obj);
  if (hdr->type != GROWL_CURRY)
    return NULL;
  return (GrowlCurry *)(hdr + 1);
}
