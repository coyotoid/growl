#include <growl.h>
#include <inttypes.h>

void growl_print(GrowlVM *vm, Growl value) { growl_print_to(vm, stdout, value); }

void growl_println(GrowlVM *vm, Growl value) {
  growl_print_to(vm, stdout, value);
  putchar('\n');
}

static void print_escaped(FILE *file, const char *data, size_t len) {
  putc('"', file);
  for (size_t i = 0; i < len; ++i) {
    switch (data[i]) {
    case '\0':
      putc('\\', file);
      putc('0', file);
      break;
    case '\t':
      putc('\\', file);
      putc('t', file);
      break;
    case '\n':
      putc('\\', file);
      putc('n', file);
      break;
    case '\r':
      putc('\\', file);
      putc('r', file);
      break;
    case '\b':
      putc('\\', file);
      putc('b', file);
      break;
    case '\v':
      putc('\\', file);
      putc('v', file);
      break;
    case '\f':
      putc('\\', file);
      putc('f', file);
      break;
    case '\x1b':
      putc('\\', file);
      putc('e', file);
      break;
    case '\\':
      putc('\\', file);
      putc('\\', file);
      break;
    case '"':
      putc('\\', file);
      putc('"', file);
      break;
    default:
      putc(data[i], file);
      break;
    }
  }
  putc('"', file);
}

void growl_print_to(GrowlVM *vm, FILE *file, Growl value) {
  if (GROWL_IS_NIL(value)) {
    fprintf(file, "nil");
  } else if (GROWL_IS_NUM(value)) {
    fprintf(file, "%g", growl_to_double(value));
  } else {
    GrowlObjectHeader *hdr = growl_unbox(vm, value);
    switch (hdr->type) {
    case GROWL_TYPE_STRING: {
      GrowlString *str = (GrowlString *)(hdr + 1);
      print_escaped(file, str->data, str->len);
      break;
    }
    default:
      fprintf(file, "<object type=%" PRIu32 " @ %p>", hdr->type, (void *)hdr);
      break;
    }
  }
}
