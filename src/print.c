#include <inttypes.h>
#include <stdio.h>

#include "object.h"
#include "print.h"

V print(O o) {
  if (o == NIL) {
    printf("nil");
  } else if (IMM(o)) {
    printf("%" PRIdPTR, ORD(o));
  } else {
    switch (type(o)) {
    case TYPE_QUOT:
      printf("<quotation>");
      break;
    default:
      printf("<obj type=%ld ptr=%p>", type(o), (void *)o);
    }
  }
}

V println(O o) {
  print(o);
  putchar('\n');
}
