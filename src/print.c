#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "print.h"
#include "string.h"
#include "vendor/mpc.h"

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
      char *escaped = malloc(s->len + 1);
      memcpy(escaped, s->data, s->len);
      escaped[s->len] = 0;
      escaped = mpcf_escape(escaped);
      printf("\"%s\"", escaped);
      free(escaped);
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
