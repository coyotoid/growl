#include <growl.h>

static void file_finalize(void *data) {
  FILE *f = data;
  if (f && f != stdin && f != stdout && f != stderr)
    fclose(f);
}

// clang-format off
static GrowlAlienType alien_file_type = {
  .name = "file",
  .finalizer = file_finalize,
  .call = NULL,
};
// clang-format on

static Growl stdout_obj = GROWL_NIL;
static void native_file_stdout(GrowlVM *vm) {
  if (stdout_obj == GROWL_NIL) {
    GrowlObjectHeader *hdr = growl_gc_alloc_tenured(
        vm, sizeof(GrowlObjectHeader) + sizeof(GrowlAlien));
    hdr->type = GROWL_TYPE_ALIEN;
    GrowlAlien *stdout_alien = (GrowlAlien *)(hdr + 1);
    stdout_alien->data = stdout;
    stdout_alien->type = &alien_file_type;
    stdout_obj = growl_box_tenured(vm, hdr);
  }
  growl_push(vm, stdout_obj);
}

static void native_file_write(GrowlVM *vm) {
  Growl file_obj = growl_pop(vm);
  Growl string_obj = growl_pop(vm);

  GrowlAlien *file_alien = growl_unwrap_alien(vm, file_obj, &alien_file_type);
  if (file_alien == NULL)
    growl_vm_error(vm, "expected file object");

  GrowlString *str = growl_unwrap_string(vm, string_obj);
  if (str == NULL)
    growl_vm_error(vm, "expected string");

  fwrite(str->data, sizeof(char), str->len, file_alien->data);
}

void growl_register_file_library(GrowlVM *vm) {
  growl_register_native(vm, "file/stdout", native_file_stdout);
  growl_register_native(vm, "file/write", native_file_write);
}
