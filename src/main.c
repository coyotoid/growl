#include <growl.h>
#include <math.h>

int main(void) {
  GrowlVM *vm = growl_vm_init();
  growl_register_file_library(vm);
  GrowlLexer lexer = {0};
  lexer.file = stdin;

  Growl obj = growl_compile(vm, &lexer);
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
  return 0;
}
