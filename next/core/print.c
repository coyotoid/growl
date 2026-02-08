#include <growl.h>
#include <inttypes.h>

void growl_print(Growl value) { growl_print_to(stdout, value); }

void growl_println(Growl value) {
  growl_print_to(stdout, value);
  putchar('\n');
}

static void print_escaped(const char *data, size_t len) {
  putchar('"');
  for (size_t i = 0; i < len; ++i) {
    switch (data[i]) {
    case '\0':
      putchar('\\');
      putchar('0');
      break;
    case '\t':
      putchar('\\');
      putchar('t');
      break;
    case '\n':
      putchar('\\');
      putchar('n');
      break;
    case '\r':
      putchar('\\');
      putchar('r');
      break;
    case '\b':
      putchar('\\');
      putchar('b');
      break;
    case '\v':
      putchar('\\');
      putchar('v');
      break;
    case '\f':
      putchar('\\');
      putchar('f');
      break;
    case '\x1b':
      putchar('\\');
      putchar('e');
      break;
    case '\\':
      putchar('\\');
      putchar('\\');
      break;
    case '"':
      putchar('\\');
      putchar('"');
      break;
    default:
      putchar(data[i]);
      break;

    }
  }
  putchar('"');
}

void growl_print_to(FILE *file, Growl value) {
  if (value == GROWL_NIL) {
    fprintf(file, "nil");
  } else if (GROWL_IMM(value)) {
    fprintf(file, "%" PRIdPTR, GROWL_ORD(value));
  } else {
    GrowlObjectHeader *hdr = GROWL_UNBOX(value);
    switch (hdr->type) {
    case GROWL_TYPE_STRING: {
      GrowlString *str = (GrowlString *)(hdr + 1);
      print_escaped(str->data, str->len);
      break;
    }
    default:
      fprintf(file, "<object type=%" PRIu32 " @ %p>", hdr->type, hdr);
      break;
    }
  }
}
