#include "common.h"

#include "arena.h"
#include "chunk.h"
#include "gc.h"
#include "vm.h"

#include "vendor/mpc.h"

#define COMPILER_DEBUG 1

/** Compiler context */
typedef struct Cm {
  Vm *vm; // Parent context
  Ar *arena;
  Bc *chunk;
  Dt **dictionary;
} Cm;

V compiler_init(Cm *, Vm *, const char *);
V compiler_deinit(Cm *);

// Hash function for word names
U64 hash64(const char *);

// Dictionary lookup
Dt *upsert(Dt **, const char *, Ar *);

// The chunk returned by `compile_program` is owned by the caller.
Bc *compile_program(Cm *, mpc_ast_t *);
