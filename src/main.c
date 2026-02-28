#include <growl.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>

// TODO: repl...?

int main(int argc, const char *argv[]) {
  if (argc == 1) {
    fprintf(stderr, "usage: %s file.grr\n", argv[0]);
    exit(1);
  }

  GrowlVM *vm = growl_vm_init();
  growl_register_file_library(vm);

  GrowlLexer lexer = {0};

  const char *filename;
  char *dirname_;

  if (strcmp(argv[1], "-") == 0) {
    filename = "<stdin>";
    dirname_ = ".";
    lexer.file = stdin;
  } else {
    filename = argv[1];
    dirname_ = dirname(strdup(filename));
    lexer.file = fopen(argv[1], "r");
  }

  if (lexer.file == NULL) {
    fprintf(stderr, "growl: failed to open `%s'\n", argv[1]);
    exit(1);
  }

  Growl obj = growl_compile(vm, &lexer, filename, dirname_);
  if (obj != GROWL_NIL) {
    GrowlQuotation *quot = growl_unwrap_quotation(vm, obj);
    if (!growl_vm_execute(vm, quot)) {
      if (vm->sp != vm->wst) {
        fprintf(stderr, "Stack:");
        for (Growl *p = vm->wst; p < vm->sp; p++) {
          putc(' ', stderr);
          growl_print_to(vm, stderr, *p);
        }
        putchar('\n');
      }
    }
  }

  growl_gc_collect(vm);
  growl_vm_free(vm);
  if (lexer.file != stdin)
    fclose(lexer.file);
  return 0;
}
