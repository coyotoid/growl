#include <growl.h>
#include <string.h>

uint32_t growl_type(Growl obj) {
  if (obj == GROWL_NIL)
    return GROWL_TYPE_NIL;
  if (GROWL_IMM(obj))
    return GROWL_TYPE_NUMBER;
  GrowlObjectHeader *hdr = GROWL_UNBOX(obj);
  return hdr->type;
}

int growl_equals(Growl a, Growl b) {
  if (a == b)
    return 1;
  uint32_t type_a = growl_type(a);
  uint32_t type_b = growl_type(b);
  if (type_a != type_b)
    return 0;
  switch (type_a) {
  case GROWL_TYPE_NIL:
  case GROWL_TYPE_NUMBER:
    // Already checked by pointer equality
    return 0;
  case GROWL_TYPE_STRING: {
    GrowlString *str_a = growl_unwrap_string(a);
    GrowlString *str_b = growl_unwrap_string(b);
    if (str_a->len != str_b->len)
      return 0;
    return memcmp(str_a->data, str_b->data, str_a->len) == 0;
  }
  case GROWL_TYPE_LIST: {
    GrowlList *list_a = (GrowlList *)(GROWL_UNBOX(a) + 1);
    GrowlList *list_b = (GrowlList *)(GROWL_UNBOX(b) + 1);
    return growl_equals(list_a->head, list_b->head) &&
           growl_equals(list_a->tail, list_b->tail);
  }
  case GROWL_TYPE_TUPLE: {
    GrowlTuple *tuple_a = growl_unwrap_tuple(a);
    GrowlTuple *tuple_b = growl_unwrap_tuple(b);
    if (tuple_a->count != tuple_b->count)
      return 0;
    for (size_t i = 0; i < tuple_a->count; i++) {
      if (!growl_equals(tuple_a->data[i], tuple_b->data[i]))
        return 0;
    }
    return 1;
  }
  case GROWL_TYPE_QUOTATION: {
    GrowlQuotation *quot_a = (GrowlQuotation *)(GROWL_UNBOX(a) + 1);
    GrowlQuotation *quot_b = (GrowlQuotation *)(GROWL_UNBOX(b) + 1);
    if (quot_a->count != quot_b->count)
      return 0;
    if (memcmp(quot_a->data, quot_b->data, quot_a->count) != 0)
      return 0;
    return growl_equals(quot_a->constants, quot_b->constants);
  }
  case GROWL_TYPE_COMPOSE: {
    GrowlCompose *comp_a = (GrowlCompose *)(GROWL_UNBOX(a) + 1);
    GrowlCompose *comp_b = (GrowlCompose *)(GROWL_UNBOX(b) + 1);
    return growl_equals(comp_a->first, comp_b->first) &&
           growl_equals(comp_a->second, comp_b->second);
  }
  case GROWL_TYPE_CURRY: {
    GrowlCurry *curry_a = (GrowlCurry *)(GROWL_UNBOX(a) + 1);
    GrowlCurry *curry_b = (GrowlCurry *)(GROWL_UNBOX(b) + 1);
    return growl_equals(curry_a->value, curry_b->value) &&
           growl_equals(curry_a->callable, curry_b->callable);
  }
  case GROWL_TYPE_TABLE:
    return 0;
  default:
    return 0;
  }
}
