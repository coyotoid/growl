#include <inttypes.h>
#include <stdio.h>

#include "object.h"
#include "string.h"
#include "print.h"

V print(O o) {
  if (o == NIL) {
    printf("nil");
  } else if (IMM(o)) {
    printf("%" PRIdPTR, ORD(o));
  } else {
    Hd *hdr = UNBOX(o);
    switch (hdr->type) {
    case OBJ_QUOT:
      printf("<quotation>");
      break;
    case OBJ_STR: {
      Str *s = string_unwrap(o);
      printf("\"%.*s\"", (int)s->len, s->data);
      break;
    }
    default:
      printf("<obj type=%ld ptr=%p>", type(o), (void *)o);
    }
  }
}

V println(O o) {
  print(o);
  putchar('\n');
}
