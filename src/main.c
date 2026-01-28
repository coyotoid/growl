#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "compile.h"
#include "debug.h"
#include "parser.h"
#include "vm.h"

#include "vendor/linenoise.h"

#define REPL_BUFFER_SIZE 4096

I repl(void) {
  Vm vm = {0};
  vm_init(&vm);

  char *line;
  while ((line = linenoise("growl> ")) != NULL) {
    Buf b = { line, (int)strlen(line), 0, -1 };
    Stream s = { bufstream_vtable, &b };

    Lx *lx = lexer_make(&s);
    Ast *root = parser_parse(lx);

    Cm cm = {0};
    compiler_init(&cm, &vm, "<repl>");
    Bc *chunk = compile_program(&cm, root);
    ast_free(root);
    lexer_free(lx);

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

  FILE *f = fopen(fname, "rb");
  if (!f) {
      fprintf(stderr, "error: cannot open file '%s'\n", fname);
      return 1;
  }

  Stream s = { filestream_vtable, f };
  Lx *lx = lexer_make(&s);
  Ast *root = parser_parse(lx);

  Cm cm = {0};
  compiler_init(&cm, &vm, fname);

  Bc *chunk = compile_program(&cm, root);
  ast_free(root);
  lexer_free(lx);
  fclose(f);

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
