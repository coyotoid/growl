#include <growl.h>

#define FNV_OFFSET_BASIS 14695981039346656037ULL
#define FNV_PRIME 1099511628211ULL

uint64_t growl_hash_combine(uint64_t a, uint64_t b) {
  return a ^ (b + 0x9e3779b97f4a7c15ULL + (a << 6) + (a >> 2));
}

uint64_t growl_hash_bytes(const uint8_t *data, size_t len) {
  uint64_t hash = FNV_OFFSET_BASIS;
  for (size_t i = 0; i < len; i++) {
    hash ^= (uint64_t)data[i];
    hash *= FNV_PRIME;
  }
  return hash;
}

static uint64_t hash_list(GrowlVM *vm, Growl obj) {
  uint64_t hash = FNV_OFFSET_BASIS;
  while (!GROWL_IS_NIL(obj)) {
    GrowlList *list = (GrowlList *)(growl_unbox(vm, obj) + 1);
    uint64_t head = growl_hash(vm, list->head);
    hash = growl_hash_combine(hash, head);
    obj = list->tail;
  }
  return hash;
}

uint64_t growl_hash(GrowlVM *vm, Growl obj) {
  if (GROWL_IS_NIL(obj))
    return 0;

  if (GROWL_IS_NUM(obj)) {
    double d = growl_to_double(obj);
    if (d == 0.0)
      return 0; // 0.0 and -0.0 hash the same
    if (d != d)
      return GROWL_CANON_NAN; // all NaNs hash the same
    return obj;
  }

  GrowlObjectHeader *hdr = growl_unbox(vm, obj);
  switch (hdr->type) {
  case GROWL_TYPE_STRING: {
    GrowlString *str = (GrowlString *)(hdr + 1);
    return growl_hash_bytes((uint8_t *)str->data, str->len);
  }
  case GROWL_TYPE_LIST: {
    return hash_list(vm, obj);
  }
  case GROWL_TYPE_TUPLE: {
    GrowlTuple *tuple = (GrowlTuple *)(hdr + 1);
    uint64_t hash = FNV_OFFSET_BASIS;
    for (size_t i = 0; i < tuple->count; i++)
      hash = growl_hash_combine(hash, growl_hash(vm, tuple->data[i]));
    return hash;
  }
  case GROWL_TYPE_QUOTATION: {
    GrowlQuotation *quot = (GrowlQuotation *)(hdr + 1);
    uint64_t hash = growl_hash_bytes(quot->data, quot->count);
    if (!GROWL_IS_NIL(quot->constants))
      hash = growl_hash_combine(hash, growl_hash(vm, quot->constants));
    return hash;
  }
  case GROWL_TYPE_COMPOSE: {
    GrowlCompose *comp = (GrowlCompose *)(hdr + 1);
    return growl_hash_combine(growl_hash(vm, comp->first), growl_hash(vm, comp->second));
  }
  case GROWL_TYPE_CURRY: {
    GrowlCurry *curry = (GrowlCurry *)(hdr + 1);
    return growl_hash_combine(growl_hash(vm, curry->value), growl_hash(vm, curry->callable));
  }
  default:
    return obj;
  }
}
