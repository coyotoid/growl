#include <stdio.h>
#include <stdlib.h>

#include "src/gc.h"
#include "src/object.h"
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

I prim_file_stdin(Vm *vm) {
  vm_push(vm, vm->stdin);
  return 0;
}

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
    fprintf(stderr, "expected file object\n");
    return VM_ERR_TYPE;
  };

  Str *str = string_unwrap(string_obj);
  if (str == NULL) {
    fprintf(stderr, "expected string\n");
    return VM_ERR_TYPE;
  }

  fwrite(str->data, sizeof(char), str->len, (FILE *)file_ud->data);
  return 0;
}

I prim_file_fgetline(Vm *vm) {
  O file_obj = vm_pop(vm);
  I mark = gc_mark(&vm->gc);
  gc_addroot(&vm->gc, &file_obj);

  Ud *file_ud = userdata_unwrap(file_obj, &userdata_file);
  if (file_ud == NULL) {
    fprintf(stderr, "expected file object\n");
    return VM_ERR_TYPE;
  }

  char *lineptr = NULL;
  size_t size;
  I len = getline(&lineptr, &size, (FILE *)file_ud->data);
  if (len == -1) {
    vm_push(vm, NIL);
  } else {
    vm_push(vm, string_make(vm, lineptr, len));
  }
  free(lineptr);

  gc_reset(&vm->gc, mark);
  return 0;
}

static V finalizer(V *data) {
  FILE *f = (FILE *)data;
  if (f && f != stdin && f != stdout && f != stderr)
    fclose(f);
}
