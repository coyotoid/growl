#include "object.h"

I type(O o) {
  if (o == NIL)
    return TYPE_NIL;
  if (IMM(o))
    return TYPE_NUM;
  Hd *h = UNBOX(o);
  return h->type;
}
