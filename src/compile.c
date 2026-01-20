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
  {"nil", OP_NIL},
  {"dup", OP_DUP},
  {"drop", OP_DROP},
  {"swap", OP_SWAP},
  {"over", OP_OVER},
  {"nip", OP_NIP},
  {"bury", OP_BURY},
  {"dig", OP_DIG},
  {">r", OP_TOR},
  {"r>", OP_FROMR},
  {"call", OP_APPLY},
  {"?", OP_CHOOSE},
  {"+", OP_ADD},
  {"-", OP_SUB},
  {"*", OP_MUL},
  {"/", OP_DIV},
  {"%", OP_MOD},
  {"=", OP_EQ},
  {"<>", OP_NEQ},
  {"<", OP_LT},
  {">", OP_GT},
  {"<=", OP_LTE},
  {">=", OP_GTE},
  {NULL, 0},
};
// clang-format on

V compiler_init(Cm *cm, Vm *vm, const char *name) {
  cm->vm = vm;
  cm->arena = &vm->arena;
  cm->dictionary = &vm->dictionary;
  cm->chunk = chunk_new(name);
}

V compiler_deinit(Cm *cm) { cm->dictionary = NULL; }

static I peek_sleb128(U8 *ptr, I *out_value) {
  I result = 0;
  I shift = 0;
  U8 byte;
  I bytes = 0;

  do {
    byte = ptr[bytes];
    bytes++;
    result |= (I)(byte & 0x7F) << shift;
    shift += 7;
  } while (byte & 0x80);

  if ((shift < 64) && (byte & 0x40)) {
    result |= -(1LL << shift);
  }

  if (out_value)
    *out_value = result;
  return bytes;
}

static V optim_tailcall(Bc *chunk) {
  Z i = 0;
  while (i < chunk->count) {
    U8 opcode = chunk->items[i];
    if (opcode == OP_CALL) {
      I ofs = peek_sleb128(&chunk->items[i + 1], NULL);
      Z next = i + 1 + ofs;
      if (next < chunk->count && chunk->items[next] == OP_RETURN) {
        chunk->items[i] = OP_TAIL_CALL;
      }
      i++;
    } else if (opcode == OP_DOWORD) {
      I ofs = peek_sleb128(&chunk->items[i + 1], NULL);
      Z next = i + 1 + ofs;
      if (next < chunk->count && chunk->items[next] == OP_RETURN) {
        chunk->items[i] = OP_TAIL_DOWORD;
      }
      i++;
    } else if (opcode == OP_APPLY) {
      Z ofs = i + 1;
      if (ofs < chunk->count && chunk->items[ofs] == OP_RETURN) {
        chunk->items[i] = OP_TAIL_APPLY;
      }
      i++;
    } else if (opcode == OP_CONST || opcode == OP_JUMP ||
               opcode == OP_JUMP_IF_NIL) {
      I ofs = peek_sleb128(&chunk->items[i + 1], NULL);
      i += 1 + ofs;
    } else {
      i++;
    }
  }
}

static I compile_expr(Cm *cm, mpc_ast_t *curr, mpc_ast_trav_t **next);
static I compile_ast(Cm *cm, mpc_ast_t *curr, mpc_ast_trav_t **next);

static I compile_constant(Cm *cm, O value) {
  I idx = chunk_add_constant(cm->chunk, value);
  chunk_emit_byte(cm->chunk, OP_CONST);
  chunk_emit_sleb128(cm->chunk, idx);
  return 1;
}

static I compile_call(Cm *cm, const char *name) {
  for (Z i = 0; primitives[i].name != NULL; i++) {
    if (strcmp(name, primitives[i].name) == 0) {
      chunk_emit_byte(cm->chunk, primitives[i].opcode);
      return 1;
    }
  }
  Dt *word = upsert(cm->dictionary, name, NULL);
  if (!word) {
    fprintf(stderr, "compiler: undefined word '%s'\n", name);
    return 0;
  }
  chunk_emit_byte(cm->chunk, OP_DOWORD);
  chunk_emit_sleb128(cm->chunk, (I)word->hash);
  return 1;
}

static I compile_command(Cm *cm, mpc_ast_t *curr, mpc_ast_trav_t **next) {
  curr = mpc_ast_traverse_next(next);
  const char *name = curr->contents;
  (void)mpc_ast_traverse_next(next);
  curr = mpc_ast_traverse_next(next);
  while (curr != NULL) {
    if (strcmp(curr->tag, "char") == 0 && strcmp(curr->contents, ";") == 0)
      break;
    I res = compile_expr(cm, curr, next);
    if (!res)
      return 0;
    curr = mpc_ast_traverse_next(next);
  }
  compile_call(cm, name);
  return 1;
}

static I compile_definition(Cm *cm, mpc_ast_t *curr, mpc_ast_trav_t **next) {
  (void)mpc_ast_traverse_next(next); // skip 'def'
  curr = mpc_ast_traverse_next(next);
  const char *name = arena_strdup(cm->arena, curr->contents);
  (void)mpc_ast_traverse_next(next); // skip '{'

  Dt *entry = upsert(cm->dictionary, name, cm->arena);

  Cm inner = {0};
  inner.arena = cm->arena;
  inner.chunk = chunk_new(name);
  inner.vm = cm->vm;
  inner.dictionary = cm->dictionary;

  curr = mpc_ast_traverse_next(next);
  while (curr != NULL) {
    if (strcmp(curr->tag, "char") == 0 && strcmp(curr->contents, "}") == 0)
      break;
    if (!compile_expr(&inner, curr, next)) {
      chunk_release(inner.chunk);
      return 0;
    }
    curr = mpc_ast_traverse_next(next);
  }

  chunk_emit_byte(inner.chunk, OP_RETURN);
  optim_tailcall(inner.chunk);

  entry->chunk = inner.chunk;

#if COMPILER_DEBUG
  disassemble(inner.chunk, name, cm->dictionary);
#endif

  return 1;
}

static O compile_quotation_obj(Cm *cm, mpc_ast_t *curr, mpc_ast_trav_t **next) {
  Cm inner = {0};
  inner.arena = cm->arena;
  inner.chunk = chunk_new("<quotation>");
  inner.vm = cm->vm;
  inner.dictionary = cm->dictionary;

  (void)mpc_ast_traverse_next(next);
  curr = mpc_ast_traverse_next(next);
  while (curr != NULL) {
    if (strcmp(curr->tag, "char") == 0 && strcmp(curr->contents, "]") == 0)
      break;
    I res = compile_expr(&inner, curr, next);
    if (!res) {
      chunk_release(inner.chunk);
      return res;
    }
    curr = mpc_ast_traverse_next(next);
  }
  chunk_emit_byte(inner.chunk, OP_RETURN);
  optim_tailcall(inner.chunk);

  Hd *hd = gc_alloc(cm->vm, sizeof(Hd) + sizeof(Bc *));
  hd->type = OBJ_QUOT;
  Bc **chunk_ptr = (Bc **)(hd + 1);
  *chunk_ptr = inner.chunk;

  return BOX(hd);
}

static I compile_quotation(Cm *cm, mpc_ast_t *curr, mpc_ast_trav_t **next) {
  return compile_constant(cm, compile_quotation_obj(cm, curr, next));
}

static I compile_expr(Cm *cm, mpc_ast_t *curr, mpc_ast_trav_t **next) {
  if (strstr(curr->tag, "expr|number") != NULL) {
    I num = strtol(curr->contents, NULL, 0);
    return compile_constant(cm, NUM(num));
  } else if (strstr(curr->tag, "expr|word") != NULL) {
    return compile_call(cm, curr->contents);
  } else if (strstr(curr->tag, "expr|quotation") != NULL) {
    return compile_quotation(cm, curr, next);
  } else if (strstr(curr->tag, "expr|def") != NULL) {
    return compile_definition(cm, curr, next);
  } else if (strstr(curr->tag, "expr|command") != NULL) {
    return compile_command(cm, curr, next);
  } else if (strstr(curr->tag, "expr|comment") != NULL) {
    return 1;
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

Bc *compile_program(Cm *cm, mpc_ast_t *ast) {
  mpc_ast_trav_t *next = mpc_ast_traverse_start(ast, mpc_ast_trav_order_pre);
  mpc_ast_t *curr = mpc_ast_traverse_next(&next); // Begin traversal

  if (!compile_ast(cm, curr, &next)) {
    chunk_release(cm->chunk);
    return NULL;
  }

  Bc *chunk = cm->chunk;
  chunk_emit_byte(chunk, OP_RETURN);
  optim_tailcall(chunk);
  return chunk;
}
