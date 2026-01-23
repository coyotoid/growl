#include <stdio.h>

#include "string.h"
#include "userdata.h"
#include "vm.h"

static V finalizer(V *data);

// clang-format off
Ut userdata_file = {
  .name = "file",
  .finalizer = finalizer
};
// clang-format on

I prim_file_stdout(Vm *vm) {
  vm_push(vm, vm->stdout);
  return 0;
}

I prim_file_stderr(Vm *vm) {
  vm_push(vm, vm->stderr);
  return 0;
}

I prim_file_fprint(Vm *vm) {
  O file_obj = vm_pop(vm);
  O string_obj = vm_pop(vm);

  Ud *file_ud = userdata_unwrap(file_obj, &userdata_file);
  if (file_ud == NULL) {
    fprintf(stderr, "expected file object");
    return VM_ERR_TYPE;
  };

  Str *str = string_unwrap(string_obj);
  if (str == NULL) {
    fprintf(stderr, "expected string");
    return VM_ERR_TYPE;
  }

  fwrite(str->data, sizeof(char), str->len, (FILE *)file_ud->data);
  return 0;
}

static V finalizer(V *data) {
  FILE *f = (FILE *)data;
  if (f && f != stdin && f != stdout && f != stderr)
    fclose(f);
}
