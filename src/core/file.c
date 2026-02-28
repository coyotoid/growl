#include <growl.h>
#include <stdio.h>

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

static Growl stderr_obj = GROWL_NIL;
static void native_file_stderr(GrowlVM *vm) {
  if (stderr_obj == GROWL_NIL) {
    GrowlObjectHeader *hdr = growl_gc_alloc_tenured(
        vm, sizeof(GrowlObjectHeader) + sizeof(GrowlAlien));
    hdr->type = GROWL_TYPE_ALIEN;
    GrowlAlien *stderr_alien = (GrowlAlien *)(hdr + 1);
    stderr_alien->data = stderr;
    stderr_alien->type = &alien_file_type;
    stderr_obj = growl_box_tenured(vm, hdr);
  }
  growl_push(vm, stderr_obj);
}

static Growl stdin_obj = GROWL_NIL;
static void native_file_stdin(GrowlVM *vm) {
  if (stdin_obj == GROWL_NIL) {
    GrowlObjectHeader *hdr = growl_gc_alloc_tenured(
        vm, sizeof(GrowlObjectHeader) + sizeof(GrowlAlien));
    hdr->type = GROWL_TYPE_ALIEN;
    GrowlAlien *stdin_alien = (GrowlAlien *)(hdr + 1);
    stdin_alien->data = stdin;
    stdin_alien->type = &alien_file_type;
    stdin_obj = growl_box_tenured(vm, hdr);
  }
  growl_push(vm, stdin_obj);
}

static void native_file_write(GrowlVM *vm) {
  Growl file_obj = growl_pop(vm);
  Growl string_obj = growl_pop(vm);

  GrowlAlien *file_alien = growl_unwrap_alien(vm, file_obj, &alien_file_type);
  if (file_alien == NULL)
    growl_vm_error(vm, "file/write: expected file object");

  GrowlString *str = growl_unwrap_string(vm, string_obj);
  if (str == NULL)
    growl_vm_error(vm, "file/write: expected string");

  fwrite(str->data, sizeof(char), str->len, file_alien->data);
}

static void native_file_putc(GrowlVM *vm) {
  Growl file_obj = growl_pop(vm);
  Growl char_obj = growl_pop(vm);

  GrowlAlien *file_alien = growl_unwrap_alien(vm, file_obj, &alien_file_type);
  if (file_alien == NULL)
    growl_vm_error(vm, "file/putc: expected file object");
  if (!GROWL_IS_NUM(char_obj))
    growl_vm_error(vm, "file/putc: expected number");

  fputc(growl_to_double(char_obj), file_alien->data);
}

static void native_file_getc(GrowlVM *vm) {
  Growl file_obj = growl_pop(vm);
  GrowlAlien *file_alien = growl_unwrap_alien(vm, file_obj, &alien_file_type);
  if (file_alien == NULL)
    growl_vm_error(vm, "file/getc: expected file object");
  int chr = fgetc(file_alien->data);
  if (chr == EOF) {
    growl_push(vm, GROWL_NIL);
  } else {
    growl_push(vm, growl_from_double(chr));
  }
}

void growl_register_file_library(GrowlVM *vm) {
  growl_register_native(vm, "file/stdout", native_file_stdout);
  growl_register_native(vm, "file/stderr", native_file_stderr);
  growl_register_native(vm, "file/stdin", native_file_stdin);
  growl_register_native(vm, "file/write", native_file_write);
  growl_register_native(vm, "file/putc", native_file_putc);
  growl_register_native(vm, "file/getc", native_file_getc);
}
