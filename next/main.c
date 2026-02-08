#include "core/opcodes.h"
#include <growl.h>

int main(void) {
  GrowlVM *vm = growl_vm_init();
  GrowlLexer lexer = {0};
  lexer.file = stdin;

  Growl obj = growl_compile(vm, &lexer);
  if (obj != GROWL_NIL) {
    GrowlQuotation *quot = growl_unwrap_quotation(obj);
    growl_disassemble(vm, quot);
    growl_vm_execute(vm, quot);

    printf("Stack:");
    for (Growl *p = vm->wst; p < vm->sp; p++) {
      putchar(' ');
      growl_print(*p);
    }
    putchar('\n');
  }

  growl_gc_collect(vm);
  growl_vm_free(vm);
  return 0;
}
