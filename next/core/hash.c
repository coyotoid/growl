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

static uint64_t hash_list(Growl obj) {
  uint64_t hash = FNV_OFFSET_BASIS;
  while (obj != GROWL_NIL) {
    GrowlList *list = (GrowlList *)(GROWL_UNBOX(obj) + 1);
    uint64_t head = growl_hash(list->head);
    hash = growl_hash_combine(hash, head);
    obj = list->tail;
  }
  return hash;
}

uint64_t growl_hash(Growl obj) {
  if (obj == GROWL_NIL)
    return 0;

  if (GROWL_IMM(obj))
    return obj;

  GrowlObjectHeader *hdr = GROWL_UNBOX(obj);
  switch (hdr->type) {
  case GROWL_TYPE_STRING: {
    GrowlString *str = (GrowlString *)(hdr + 1);
    return growl_hash_bytes((uint8_t *)str->data, str->len);
  }
  case GROWL_TYPE_LIST: {
    return hash_list(obj);
  }
  case GROWL_TYPE_TUPLE: {
    GrowlTuple *tuple = (GrowlTuple *)(hdr + 1);
    uint64_t hash = FNV_OFFSET_BASIS;
    for (size_t i = 0; i < tuple->count; i++)
      hash = growl_hash_combine(hash, growl_hash(tuple->data[i]));
    return hash;
  }
  case GROWL_TYPE_QUOTATION: {
    GrowlQuotation *quot = (GrowlQuotation *)(hdr + 1);
    uint64_t hash = growl_hash_bytes(quot->data, quot->count);
    if (quot->constants != GROWL_NIL)
      hash = growl_hash_combine(hash, growl_hash(quot->constants));
    return hash;
  }
  case GROWL_TYPE_COMPOSE: {
    GrowlCompose *comp = (GrowlCompose *)(hdr + 1);
    return growl_hash_combine(growl_hash(comp->first), growl_hash(comp->second));
  }
  case GROWL_TYPE_CURRY: {
    GrowlCurry *curry = (GrowlCurry *)(hdr + 1);
    return growl_hash_combine(growl_hash(curry->value), growl_hash(curry->callable));
  }
  default:
    return obj;
  }
}
