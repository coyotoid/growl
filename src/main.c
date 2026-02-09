#include <growl.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, const char *argv[]) {
  if (argc == 1) {
    fprintf(stderr, "usage: %s file.grr\n", argv[0]);
    exit(1);
  }

  GrowlVM *vm = growl_vm_init();
  growl_register_file_library(vm);

  GrowlLexer lexer = {0};
  const char *filename = argv[1];
  char *dirname_ = strdup(filename);
  dirname_ = dirname(dirname_);

  lexer.file = fopen(argv[1], "r");

  Growl obj = growl_compile(vm, &lexer, filename, dirname_);
  if (obj != GROWL_NIL) {
    GrowlQuotation *quot = growl_unwrap_quotation(obj);
    if (!growl_vm_execute(vm, quot)) {
      if (vm->sp != vm->wst) {
        fprintf(stderr, "Stack:");
        for (Growl *p = vm->wst; p < vm->sp; p++) {
          putc(' ', stderr);
          growl_print_to(stderr, *p);
        }
        putchar('\n');
      }
    }
  }

  growl_gc_collect(vm);
  growl_vm_free(vm);
  fclose(lexer.file);
  return 0;
}
