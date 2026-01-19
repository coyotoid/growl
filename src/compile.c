#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "compile.h"
#include "debug.h"
#include "gc.h"
#include "object.h"
#include "vm.h"

#include "vendor/mpc.h"

// clang-format off
struct {
  const char *name;
  U8 opcode;
} primitives[] = {
  {"+", OP_ADD},
  {"call", OP_APPLY},
  {NULL, 0},
};
// clang-format on

static I compile_expr(Cm *cm, mpc_ast_t *curr, mpc_ast_trav_t **next);
static I compile_constant(Cm *cm, O value) {
  I idx = chunk_add_constant(cm->chunk, value);
  chunk_emit_byte(cm->chunk, OP_CONST);
  chunk_emit_sleb128(cm->chunk, idx);
  return 1;
}

static I compile_quotation(Cm *cm, mpc_ast_t *curr, mpc_ast_trav_t **next) {
  Cm inner = {0};
  inner.chunk = chunk_new();
  inner.gc = cm->gc;
  inner.dictionary = cm->dictionary;

  (void)mpc_ast_traverse_next(next); // skip opening bracket
  curr = mpc_ast_traverse_next(next);
  while (curr != NULL) {
    if (strcmp(curr->tag, "char") == 0 && strcmp(curr->contents, "]") == 0)
      break;
    I res = compile_expr(&inner, curr, next);
    if (!res)
      return res;
    curr = mpc_ast_traverse_next(next);
  }
  chunk_emit_byte(inner.chunk, OP_RETURN);

  Hd *hd = gc_alloc(cm->gc, sizeof(Hd) + sizeof(Bc *));
  hd->type = OBJ_QUOT;
  Bc **chunk_ptr = (Bc **)(hd + 1);
  *chunk_ptr = inner.chunk;

  O quot = BOX(hd);
  compile_constant(cm, quot);

  return 1;
}

static I compile_expr(Cm *cm, mpc_ast_t *curr, mpc_ast_trav_t **next) {
  if (strstr(curr->tag, "expr|number") != NULL) {
    I num = strtol(curr->contents, NULL, 0);
    return compile_constant(cm, NUM(num));
  } else if (strstr(curr->tag, "expr|word") != NULL) {
    for (Z i = 0; primitives[i].name != NULL; i++) {
      if (strcmp(curr->contents, primitives[i].name) == 0) {
        chunk_emit_byte(cm->chunk, primitives[i].opcode);
        return 1;
      }
    }
    fprintf(stderr, "compiler: dictionary nyi\n");
    return 0;
  } else if (strstr(curr->tag, "expr|quotation") != NULL) {
    return compile_quotation(cm, curr, next);
  } else {
    fprintf(stderr, "compiler: \"%s\" nyi\n", curr->tag);
    return 0;
  }

  return 1;
}

static I compile_ast(Cm *cm, mpc_ast_t *curr, mpc_ast_trav_t **next) {
  (void)mpc_ast_traverse_next(next);
  curr = mpc_ast_traverse_next(next);
  while (curr != NULL) {
    if (strcmp(curr->tag, "regex") == 0 && strcmp(curr->contents, "") == 0)
      break;
    I res = compile_expr(cm, curr, next);
    if (!res)
      return res;
    curr = mpc_ast_traverse_next(next);
  }

  return 1;
}

Bc *compile_program(Gc *gc, mpc_ast_t *ast) {
  Cm cm = {0};
  cm.chunk = chunk_new();
  cm.gc = gc;

  mpc_ast_trav_t *next = mpc_ast_traverse_start(ast, mpc_ast_trav_order_pre);
  mpc_ast_t *curr = mpc_ast_traverse_next(&next); // Begin traversal

  if (!compile_ast(&cm, curr, &next)) {
    chunk_release(cm.chunk);
    return NULL;
  }

  Bc *chunk = cm.chunk;
  chunk_emit_byte(chunk, OP_RETURN);
  return chunk;
}
