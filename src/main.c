#include <stdio.h>
#include <stdlib.h>

#include "common.h"

#include "chunk.h"
#include "compile.h"
#include "debug.h"
#include "parser.h"
#include "vm.h"

#include "vendor/mpc.h"

I repl(void) {
  Vm vm = {0};
  vm_init(&vm);

  Bc *chunk = chunk_new();

  I idx = chunk_add_constant(chunk, NUM(10));
  chunk_emit_byte(chunk, OP_CONST);
  chunk_emit_sleb128(chunk, idx);
  chunk_emit_byte(chunk, OP_CONST);
  chunk_emit_sleb128(chunk, idx);
  chunk_emit_byte(chunk, OP_ADD);
  chunk_emit_byte(chunk, OP_RETURN);

  disassemble(chunk, "test chunk");
  I res = vm_run(&vm, chunk, 0);

  chunk_release(chunk);
  vm_deinit(&vm);
  return !res;
}

I loadfile(const char *fname) {
  Vm vm = {0};
  vm_init(&vm);

  mpc_result_t res;
  if (!mpc_parse_contents(fname, Program, &res)) {
    mpc_err_print_to(res.error, stderr);
    mpc_err_delete(res.error);
    return 1;
  }

  Bc *chunk = compile_program(&vm.gc, res.output);
  mpc_ast_delete(res.output);

  if (chunk != NULL) {
    disassemble(chunk, fname);
    I res = vm_run(&vm, chunk, 0);
    chunk_release(chunk);
    vm_deinit(&vm);
    return !res;
  } else {
    vm_deinit(&vm);
    return 1;
  }
}

int main(int argc, const char *argv[]) {
  parser_init();
  atexit(parser_deinit);

  switch (argc) {
  case 1:
    return repl();
  case 2:
    return loadfile(argv[1]);
  default:
    fprintf(stderr, "usage: growl [file]\n");
    return 64;
  }
}
