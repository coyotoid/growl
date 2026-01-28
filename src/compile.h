#include "common.h"

#include "arena.h"
#include "chunk.h"
#include "gc.h"
#include "vm.h"
#include "parser.h"

#define COMPILER_DEBUG 0

/** Compiler context */
typedef struct Cm {
  Vm *vm; // Parent context
  Ar *arena;
  Bc *chunk;
  Dt **dictionary;
} Cm;

V compiler_init(Cm *, Vm *, const char *);
V compiler_deinit(Cm *);
Bc *compile_program(Cm *, Ast *);
