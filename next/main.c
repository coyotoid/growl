#include "core/opcodes.h"
#include <growl.h>

static uint8_t code[] = {
    GOP_PUSH_NIL,
    GOP_RETURN,
};

int main(void) {
  GrowlVM *vm = growl_vm_init();

  Growl quot_obj = growl_make_quotation(vm, code, sizeof(code), NULL, 0);
  GrowlQuotation *quot = (GrowlQuotation *)(GROWL_UNBOX(quot_obj) + 1);
  vm_doquot(vm, quot);

  growl_gc_collect(vm);
  growl_vm_free(vm);
}
