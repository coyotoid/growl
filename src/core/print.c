#include <growl.h>
#include <inttypes.h>
#include <math.h>

#define GROWL_INTMAX_DOUBLE 9007199254740992.0
#define GROWL_INTMIN_DOUBLE (-9007199254740992.0)

void growl_print(GrowlVM *vm, Growl value) {
  growl_print_to(vm, stdout, value);
}

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
    double val = growl_to_double(value);
    if (val == floor(val) && val <= GROWL_INTMAX_DOUBLE &&
        val >= GROWL_INTMIN_DOUBLE) {
      fprintf(file, "%.0f", val);
    } else {
      fprintf(file, "%g", val);
    }
  } else {
    GrowlObjectHeader *hdr = growl_unbox(vm, value);
    switch (hdr->type) {
    case GROWL_TYPE_STRING: {
      GrowlString *str = (GrowlString *)(hdr + 1);
      print_escaped(file, str->data, str->len);
      break;
    }
    case GROWL_TYPE_LIST: {
      fprintf(file, "(");
      Growl cur = value;
      for (;;) {
        if (GROWL_IS_NIL(cur))
          break;
        if (!GROWL_IS_PTR(cur)) {
          fprintf(file, ". ");
          growl_print_to(vm, file, cur);
          break;
        }
        GrowlObjectHeader *node_hdr = growl_unbox(vm, cur);
        if (node_hdr->type != GROWL_TYPE_LIST) {
          fprintf(file, ". ");
          growl_print_to(vm, file, cur);
          break;
        }
        GrowlList *node = (GrowlList *)(node_hdr + 1);
        growl_print_to(vm, file, node->head);
        cur = node->tail;
        if (!GROWL_IS_NIL(cur))
          putc(' ', file);
      }
      fprintf(file, ")");
      break;
    }
    case GROWL_TYPE_TUPLE: {
      fprintf(file, "#(");
      GrowlObjectHeader *hdr = growl_unbox(vm, value);
      GrowlTuple *tupl = (GrowlTuple *)(hdr + 1);
      for (size_t i = 0; i < tupl->count; i++) {
        growl_print_to(vm, file, tupl->data[i]);
        if (i < tupl->count - 1)
          putc(' ', file);
      }
      fprintf(file, ")");
      break;
    }
    default:
      fprintf(file, "<object type=%" PRIu32 " @ %p>", hdr->type, (void *)hdr);
      break;
    }
  }
}
