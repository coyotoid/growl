#include <stdio.h>
#include <string.h>

#include "primitive.h"
#include "print.h"
#include "string.h"
#include "vm.h"

#include "file.h"

// Pretty-printing primitives
static I prim_pprint(Vm *vm) {
  println(vm_pop(vm));
  return 0;
}

static I prim_printstack(Vm *vm) {
  printf("Stk:");
  for (O *p = vm->stack; p < vm->sp; p++) {
    putchar(' ');
    print(*p);
  }
  putchar('\n');
  return 0;
}

// clang-format off
Pr primitives_table[] = {
  {".", prim_pprint},
  {".s", prim_printstack},
  {"stdout", prim_file_stdout},
  {"stderr", prim_file_stderr},
  {"fprint", prim_file_fprint},
  {NULL, NULL},
};
// clang-format on

I prim_find(const char *name) {
  for (Z i = 0; primitives_table[i].name != NULL; i++) {
    if (strcmp(primitives_table[i].name, name) == 0)
      return i;
  }
  return -1;
}
