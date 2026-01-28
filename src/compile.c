#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "compile.h"
#include "debug.h"
#include "gc.h"
#include "object.h"
#include "parser.h"
#include "src/primitive.h"
#include "string.h"
#include "vendor/yar.h"
#include "vm.h"

// clang-format off
struct {
  const char *name;
  U8 opcode[8];
} primitives[] = {
  {"nil",     {OP_NIL, 0}},
  {"dup",     {OP_DUP, 0}},
  {"drop",    {OP_DROP, 0}},
  {"swap",    {OP_SWAP, 0}},
  {"2dup",    {OP_2DUP, 0}},
  {"2drop",   {OP_2DROP, 0}},
  {"2swap",   {OP_2SWAP, 0}},
  {"2over",   {OP_2TOR, OP_2DUP, OP_2FROMR, OP_2SWAP, 0}},
  {"over",    {OP_OVER, 0}},
  {"nip",     {OP_NIP, 0}},
  {"bury",    {OP_BURY, 0}},
  {"dig",     {OP_DIG, 0}},
  {">r",      {OP_TOR, 0}},
  {"r>",      {OP_FROMR, 0}},
  {"2>r",     {OP_2TOR, 0}},
  {"2r>",     {OP_2FROMR, 0}},
  {"if",      {OP_CHOOSE, OP_CALL, 0}},
  {"call",    {OP_CALL, 0}},
  {"compose", {OP_COMPOSE, 0}},
  {"curry",   {OP_CURRY, 0}},
  {"?",       {OP_CHOOSE, 0}},
  {"+",       {OP_ADD, 0}},
  {"-",       {OP_SUB, 0}},
  {"*",       {OP_MUL, 0}},
  {"/",       {OP_DIV, 0}},
  {"%",       {OP_MOD, 0}},
  {"logand",  {OP_LOGAND, 0}},
  {"logor",   {OP_LOGOR, 0}},
  {"logxor",  {OP_LOGXOR, 0}},
  {"lognot",  {OP_LOGNOT, 0}},
  {"=",       {OP_EQ, 0}},
  {"<>",      {OP_NEQ, 0}},
  {"<",       {OP_LT, 0}},
  {">",       {OP_GT, 0}},
  {"<=",      {OP_LTE, 0}},
  {">=",      {OP_GTE, 0}},
  {"and",     {OP_AND, 0}},
  {"or",      {OP_OR, 0}},
  {"^",       {OP_CONCAT, 0}},
  {NULL,      {0}},
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
    if (opcode == OP_DOWORD) {
      I ofs = peek_sleb128(&chunk->items[i + 1], NULL);
      Z next = i + 1 + ofs;
      if (next < chunk->count && chunk->items[next] == OP_RETURN) {
        chunk->items[i] = OP_TAIL_DOWORD;
      }
      i++;
    } else if (opcode == OP_CALL) {
      Z ofs = i + 1;
      if (ofs < chunk->count && chunk->items[ofs] == OP_RETURN) {
        chunk->items[i] = OP_TAIL_CALL;
      }
      i++;
    } else if (opcode == OP_CONST) {
      I ofs = peek_sleb128(&chunk->items[i + 1], NULL);
      i += 1 + ofs;
    } else {
      i++;
    }
  }
}

static I compile_expr(Cm *cm, Ast *node);

static I compile_constant(Cm *cm, O value, I line, I col) {
  I idx = chunk_add_constant(cm->chunk, value);
  chunk_emit_byte_with_line(cm->chunk, OP_CONST, line, col);
  chunk_emit_sleb128(cm->chunk, idx);
  return 1;
}

static I add_sym(Bc *chunk, const char *name, Dt *word) {
  for (Z i = 0; i < chunk->symbols.count; i++) {
    if (strcmp(chunk->symbols.items[i].name, name) == 0)
      return i;
  }
  Z idx = chunk->symbols.count;
  Bs *sym = yar_append(&chunk->symbols);
  sym->name = name;
  sym->resolved = word;
  return idx;
}

static I compile_call(Cm *cm, const char *name, I line, I col) {
  for (Z i = 0; primitives[i].name != NULL; i++) {
    if (strcmp(name, primitives[i].name) == 0) {
      for (Z j = 0; primitives[i].opcode[j] != 0; j++)
        chunk_emit_byte_with_line(cm->chunk, primitives[i].opcode[j], line,
                                  col);
      return 1;
    }
  }

  I prim_idx = prim_find(name);
  if (prim_idx != -1) {
    chunk_emit_byte_with_line(cm->chunk, OP_PRIM, line, col);
    chunk_emit_sleb128(cm->chunk, prim_idx);
    return 1;
  }

  Dt *word = upsert(cm->dictionary, name, NULL);
  if (!word) {
    fprintf(stderr, "compiler error at %ld:%ld: undefined word '%s'\n",
            line + 1, col + 1, name);
    return 0;
  }
  I idx = add_sym(cm->chunk, name, word);
  chunk_emit_byte_with_line(cm->chunk, OP_DOWORD, line, col);
  chunk_emit_sleb128(cm->chunk, idx);
  return 1;
}

static I compile_command(Cm *cm, Ast *node) {
  for (size_t i = 0; i < node->children.count; i++) {
    if (!compile_expr(cm, node->children.items[i]))
      return 0;
  }
  return compile_call(cm, node->name, node->line, node->col);
}

static I compile_definition(Cm *cm, Ast *node) {
  const char *name = arena_strdup(cm->arena, node->name);
  Dt *entry = upsert(cm->dictionary, name, cm->arena);

  Cm inner = {0};
  inner.arena = cm->arena;
  inner.chunk = chunk_new(name);
  inner.vm = cm->vm;
  inner.dictionary = cm->dictionary;

  for (size_t i = 0; i < node->children.count; i++) {
    if (!compile_expr(&inner, node->children.items[i])) {
      chunk_release(inner.chunk);
      return 0;
    }
  }

  chunk_emit_byte_with_line(inner.chunk, OP_RETURN, node->line, node->col);
  optim_tailcall(inner.chunk);

  entry->chunk = inner.chunk;

#if COMPILER_DEBUG
  disassemble(inner.chunk, name, cm->dictionary);
#endif

  return 1;
}

static O compile_quotation_obj(Cm *cm, Ast *node) {
  Cm inner = {0};
  inner.arena = cm->arena;

  inner.chunk = chunk_new("<quotation>");
  inner.vm = cm->vm;
  inner.dictionary = cm->dictionary;

  for (size_t i = 0; i < node->children.count; i++) {
    if (!compile_expr(&inner, node->children.items[i])) {
      chunk_release(inner.chunk);
      return NIL;
    }
  }
  chunk_emit_byte_with_line(inner.chunk, OP_RETURN, node->line, node->col);
  optim_tailcall(inner.chunk);

  Hd *hd = gc_alloc(cm->vm, sizeof(Hd) + sizeof(Bc *));
  hd->type = OBJ_QUOT;
  Bc **chunk_ptr = (Bc **)(hd + 1);
  *chunk_ptr = inner.chunk;

  return BOX(hd);
}

static I compile_quotation(Cm *cm, Ast *node) {
  O obj = compile_quotation_obj(cm, node);
  if (obj == NIL)
    return 0;
  return compile_constant(cm, obj, node->line, node->col);
}

static I compile_pragma(Cm *cm, Ast *node) {
  if (strcmp(node->name, "#load") == 0) {
    if (node->children.count == 0) {
      fprintf(stderr, "compiler error: #load requires argument\n");
      return 0;
    }
    Ast *arg = node->children.items[0];
    if (arg->type != AST_STR) {
      fprintf(stderr, "compiler error: #load requires string\n");
      return 0;
    }

    char *fname = arg->name;
    FILE *f = fopen(fname, "rb");
    if (!f) {
      fprintf(stderr, "compiler error: cannot open file '%s'\n", fname);
      return 0;
    }

    Stream s = {filestream_vtable, f};
    Lx *lx = lexer_make(&s);
    Ast *root = parser_parse(lx);

    I success = 1;
    for (size_t i = 0; i < root->children.count; i++) {
      if (!compile_expr(cm, root->children.items[i])) {
        success = 0;
        break;
      }
    }

    ast_free(root);
    lexer_free(lx);
    fclose(f);
    return success;
  }
  fprintf(stderr, "compiler warning: unknown pragma \"%s\"\n", node->name);
  return 1;
}

static I compile_expr(Cm *cm, Ast *node) {
  if (!node)
    return 0;
  switch (node->type) {
  case AST_INT: {
    O num = NUM(node->int_val);
    return compile_constant(cm, num, node->line, node->col);
  }
  case AST_STR: {
    O obj = string_make(cm->vm, node->name, -1);
    return compile_constant(cm, obj, node->line, node->col);
  }
  case AST_WORD:
    return compile_call(cm, node->name, node->line, node->col);
  case AST_QUOTE:
    return compile_quotation(cm, node);
  case AST_DEF:
    return compile_definition(cm, node);
  case AST_CMD:
    return compile_command(cm, node);
  case AST_PRAGMA:
    return compile_pragma(cm, node);
  case AST_PROGRAM:
    for (size_t i = 0; i < node->children.count; i++) {
      if (!compile_expr(cm, node->children.items[i]))
        return 0;
    }
    return 1;
  default:
    fprintf(stderr, "compiler error: nyi ast type %d\n", (int)node->type);
    return 0;
  }
}

Bc *compile_program(Cm *cm, Ast *ast) {
  if (ast->type == AST_PROGRAM) {
    for (size_t i = 0; i < ast->children.count; i++) {
      if (!compile_expr(cm, ast->children.items[i])) {
        chunk_release(cm->chunk);
        return NULL;
      }
    }
  } else {
    if (!compile_expr(cm, ast)) {
      chunk_release(cm->chunk);
      return NULL;
    }
  }

  chunk_emit_byte(cm->chunk, OP_RETURN);
  optim_tailcall(cm->chunk);
  return cm->chunk;
}
