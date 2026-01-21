#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "compile.h"
#include "debug.h"
#include "parser.h"
#include "vm.h"

#include "vendor/linenoise.h"
#include "vendor/mpc.h"

#define REPL_BUFFER_SIZE 4096

I repl(void) {
  Vm vm = {0};
  vm_init(&vm);

  char *line;
  while ((line = linenoise("growl> ")) != NULL) {
    mpc_result_t res;
    if (!mpc_parse("<repl>", line, Program, &res)) {
      mpc_err_print_to(res.error, stderr);
      mpc_err_delete(res.error);
      continue;
    }
    Cm cm = {0};
    compiler_init(&cm, &vm, "<repl>");
    Bc *chunk = compile_program(&cm, res.output);
    mpc_ast_delete(res.output);
    if (chunk != NULL) {
      vm_run(&vm, chunk, 0);
      chunk_release(chunk);
      linenoiseHistoryAdd(line);
    }
    compiler_deinit(&cm);
    linenoiseFree(line);
  }
  vm_deinit(&vm);
  return 0;
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

  Cm cm = {0};
  compiler_init(&cm, &vm, fname);

  Bc *chunk = compile_program(&cm, res.output);
  mpc_ast_delete(res.output);

  if (chunk != NULL) {
#if COMPILER_DEBUG
    disassemble(chunk, fname, &vm.dictionary);
#endif
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
