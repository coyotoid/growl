#include <inttypes.h>
#include <stdio.h>

#include "object.h"
#include "print.h"
#include "string.h"
#include "userdata.h"

static V print_string(Str *s) {
  putchar('"');
  for (Z i = 0; i < s->len; i++) {
    unsigned char c = s->data[i];
    switch (c) {
    case '\t':
      printf("\\t");
      break;
    case '\n':
      printf("\\n");
      break;
    case '\r':
      printf("\\r");
      break;
    case '\b':
      printf("\\b");
      break;
    case '\v':
      printf("\\v");
      break;
    case '\f':
      printf("\\f");
      break;
    case '\0':
      printf("\\0");
      break;
    case '\x1b':
      printf("\\e");
      break;
    case '\\':
      printf("\\\\");
      break;
    case '\"':
      printf("\\\"");
      break;
    default:
      if (c < 32 || c > 126) {
        printf("\\x%02x;", c);
      } else {
        putchar(c);
      }
    }
  }
  putchar('"');
}

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
    case OBJ_COMPOSE:
      printf("<composed>");
      break;
    case OBJ_CURRY:
      printf("<curried>");
      break;
    case OBJ_STR: {
      Str *s = string_unwrap(o);
      print_string(s);
      break;
    }
    case OBJ_USERDATA: {
      Ud *ud = (Ud *)(hdr + 1);
      printf("<#userdata %s@%p>", ud->kind->name, ud->data);
      break;
    }
    default:
      printf("<#obj type=%ld ptr=%p>", type(o), (void *)o);
    }
  }
}

V println(O o) {
  print(o);
  putchar('\n');
}
