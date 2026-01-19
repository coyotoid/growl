#include <stdio.h>
#include <stdlib.h>

#include "common.h"

#include "chunk.h"
#include "gc.h"
#include "parser.h"
#include "vendor/mpc.h"
#include "vm.h"

void dump(const V *data, Z size) {
  char ascii[17];
  Z i, j;
  ascii[16] = '\0';
  for (i = 0; i < size; ++i) {
    printf("%02X ", ((unsigned char *)data)[i]);
    if (((unsigned char *)data)[i] >= ' ' &&
        ((unsigned char *)data)[i] <= '~') {
      ascii[i % 16] = ((unsigned char *)data)[i];
    } else {
      ascii[i % 16] = '.';
    }
    if ((i + 1) % 8 == 0 || i + 1 == size) {
      printf(" ");
      if ((i + 1) % 16 == 0) {
        printf("|  %s \n", ascii);
      } else if (i + 1 == size) {
        ascii[(i + 1) % 16] = '\0';
        if ((i + 1) % 16 <= 8) {
          printf(" ");
        }
        for (j = (i + 1) % 16; j < 16; ++j) {
          printf("   ");
        }
        printf("|  %s \n", ascii);
      }
    }
  }
}

I repl(void) {
  Bc chunk = {0};
  Vm vm = {0};

  vm_init(&vm);

  I idx = chunk_add_constant(&chunk, NUM(10));
  chunk_emit_byte(&chunk, OP_CONST);
  chunk_emit_sleb128(&chunk, idx);
  chunk_emit_byte(&chunk, OP_RETURN);

  vm_run(&vm, &chunk, 0);

  return 0;
}

I loadfile(const char *fname) {
  Gc gc = {0};
  gc_init(&gc);

  mpc_result_t res;
  if (!mpc_parse_contents(fname, Program, &res)) {
    mpc_err_print_to(res.error, stderr);
    mpc_err_delete(res.error);
    gc_deinit(&gc);
    return 1;
  }

  mpc_ast_print(res.output);
  mpc_ast_delete(res.output);
  gc_deinit(&gc);
  return 0;
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
