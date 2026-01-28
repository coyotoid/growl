#include "object.h"

I type(O o) {
  if (o == NIL)
    return OBJ_NIL;
  if (IMM(o))
    return OBJ_NUM;
  Hd *h = UNBOX(o);
  return h->type;
}
