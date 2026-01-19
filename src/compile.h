#include "common.h"

#include "chunk.h"
#include "gc.h"

#include "vendor/mpc.h"

/** Compiler dictionary */
typedef struct Cd Cd;
struct Cd {
  Cd *child[4];
  const char *name;
  Z offset;
};

/** Compiler context */
typedef struct Cm {
  Gc *gc;
  Bc *chunk;
  Cd *dictionary;
} Cm;

// The chunk returned by `compile_program` is owned by the caller.
Bc *compile_program(Gc *, mpc_ast_t *);
