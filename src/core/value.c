#include <growl.h>
#include <string.h>

uint32_t growl_type(GrowlVM *vm, Growl obj) {
  if (GROWL_IS_NIL(obj))
    return GROWL_TYPE_NIL;
  if (GROWL_IS_NUM(obj))
    return GROWL_TYPE_NUMBER;
  GrowlObjectHeader *hdr = growl_unbox(vm, obj);
  return hdr->type;
}

int growl_equals(GrowlVM *vm, Growl a, Growl b) {
  if (a == b) {
    // NaN != NaN even when bit-identical
    if (GROWL_IS_NUM(a) && a == GROWL_CANON_NAN)
      return 0;
    return 1;
  }
  uint32_t type_a = growl_type(vm, a);
  uint32_t type_b = growl_type(vm, b);
  if (type_a != type_b)
    return 0;
  switch (type_a) {
  case GROWL_TYPE_NIL:
    return 1;
  case GROWL_TYPE_NUMBER: {
    // 0.0 == -0.0 (different bits, same value)
    double da = growl_to_double(a), db = growl_to_double(b);
    return da == db;
  }
  case GROWL_TYPE_STRING: {
    GrowlString *str_a = growl_unwrap_string(vm, a);
    GrowlString *str_b = growl_unwrap_string(vm, b);
    if (str_a->len != str_b->len)
      return 0;
    return memcmp(str_a->data, str_b->data, str_a->len) == 0;
  }
  case GROWL_TYPE_LIST: {
    GrowlList *list_a = (GrowlList *)(growl_unbox(vm, a) + 1);
    GrowlList *list_b = (GrowlList *)(growl_unbox(vm, b) + 1);
    return growl_equals(vm, list_a->head, list_b->head) &&
           growl_equals(vm, list_a->tail, list_b->tail);
  }
  case GROWL_TYPE_TUPLE: {
    GrowlTuple *tuple_a = growl_unwrap_tuple(vm, a);
    GrowlTuple *tuple_b = growl_unwrap_tuple(vm, b);
    if (tuple_a->count != tuple_b->count)
      return 0;
    for (size_t i = 0; i < tuple_a->count; i++) {
      if (!growl_equals(vm, tuple_a->data[i], tuple_b->data[i]))
        return 0;
    }
    return 1;
  }
  case GROWL_TYPE_QUOTATION: {
    GrowlQuotation *quot_a = (GrowlQuotation *)(growl_unbox(vm, a) + 1);
    GrowlQuotation *quot_b = (GrowlQuotation *)(growl_unbox(vm, b) + 1);
    if (quot_a->count != quot_b->count)
      return 0;
    if (memcmp(quot_a->data, quot_b->data, quot_a->count) != 0)
      return 0;
    return growl_equals(vm, quot_a->constants, quot_b->constants);
  }
  case GROWL_TYPE_COMPOSE: {
    GrowlCompose *comp_a = (GrowlCompose *)(growl_unbox(vm, a) + 1);
    GrowlCompose *comp_b = (GrowlCompose *)(growl_unbox(vm, b) + 1);
    return growl_equals(vm, comp_a->first, comp_b->first) &&
           growl_equals(vm, comp_a->second, comp_b->second);
  }
  case GROWL_TYPE_CURRY: {
    GrowlCurry *curry_a = (GrowlCurry *)(growl_unbox(vm, a) + 1);
    GrowlCurry *curry_b = (GrowlCurry *)(growl_unbox(vm, b) + 1);
    return growl_equals(vm, curry_a->value, curry_b->value) &&
           growl_equals(vm, curry_a->callable, curry_b->callable);
  }
  case GROWL_TYPE_TABLE:
    return 0;
  default:
    return 0;
  }
}
