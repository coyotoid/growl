/* Lifeworld. */

#include <growl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dynarray.h"
#include "opcodes.h"
#include "sleb128.h"
#include "path.h"

#include <libgen.h>
#include <limits.h>
#include <unistd.h>

#define COMPILER_DEBUG 0

typedef struct {
  Growl *data;
  size_t count;
  size_t capacity;
} ObjectList;

typedef struct {
  uint8_t *data;
  size_t count;
  size_t capacity;

  ObjectList constants;
} Chunk;

typedef struct {
  const char *name;
  uint8_t opcodes[8];
} Primitive;

// clang-format off
Primitive primitives[] = {
  {"nil",          {GOP_PUSH_NIL, 0}},
  {"drop",         {GOP_DROP, 0}},
  {"dup",          {GOP_DUP, 0}},
  {"swap",         {GOP_SWAP, 0}},
  {"2drop",        {GOP_2DROP, 0}},
  {"2dup",         {GOP_2DUP, 0}},
  {"2swap",        {GOP_2SWAP, 0}},
  {"nip",          {GOP_NIP, 0}},
  {"over",         {GOP_OVER, 0}},
  {"bury",         {GOP_BURY, 0}},
  {"dig",          {GOP_DIG, 0}},
  {">r",           {GOP_TO_RETAIN, 0}},
  {"r>",           {GOP_FROM_RETAIN, 0}},
  {"?",            {GOP_CHOOSE, 0}},
  {"if",           {GOP_CHOOSE, GOP_CALL, 0}},
  {"call",         {GOP_CALL, 0}},
  {"compose",      {GOP_COMPOSE, 0}},
  {"curry",        {GOP_CURRY, 0}},
  {"dip",          {GOP_DIP, 0}},
  {".",            {GOP_PPRINT, 0}},
  {"+",            {GOP_ADD, 0}},
  {"*",            {GOP_MUL, 0}},
  {"-",            {GOP_SUB, 0}},
  {"/",            {GOP_DIV, 0}},
  {"%",            {GOP_MOD, 0}},
  {"and",          {GOP_AND, 0}},
  {"or",           {GOP_OR, 0}},
  {"=",            {GOP_EQ, 0}},
  {"!=",           {GOP_NEQ, 0}},
  {"<",            {GOP_LT, 0}},
  {"<=",           {GOP_LTE, 0}},
  {">",            {GOP_GT, 0}},
  {">=",           {GOP_GTE, 0}},
  {"&",            {GOP_BAND, 0}},
  {"|",            {GOP_BOR, 0}},
  {"^",            {GOP_BXOR, 0}},
  {"~",            {GOP_BNOT, 0}},
  {"cons",         {GOP_LIST_CONS, 0}},
  {"head",         {GOP_LIST_HEAD, 0}},
  {"tail",         {GOP_LIST_TAIL, 0}},
  {"list/length",  {GOP_LIST_LENGTH, 0}},
  {"list->tuple",  {GOP_LIST_TO_TUPLE}},
  {"tuple/get",    {GOP_TUPLE_GET}},
  {"tuple/set",    {GOP_TUPLE_SET}},
  {"tuple/clone",  {GOP_TUPLE_CLONE}},
  {"tuple/length", {GOP_TUPLE_LENGTH}},
  {NULL,      {0}}
};
// clang-format on

static void emit_byte(GrowlVM *vm, Chunk *chunk, uint8_t byte) {
  *growl_dynarray_push(chunk, &vm->scratch) = byte;
}

static void emit_sleb128(GrowlVM *vm, Chunk *chunk, intptr_t num) {
  int more = 1;
  while (more) {
    uint8_t byte = num & 0x7f;
    num >>= 7;
    if ((num == 0 && !(byte & 0x40)) || (num == -1 && (byte & 0x40))) {
      more = 0;
    } else {
      byte |= 0x80;
    }
    emit_byte(vm, chunk, byte);
  }
}

static size_t add_constant(GrowlVM *vm, Chunk *chunk, Growl value) {
  for (size_t i = 0; i < chunk->constants.count; ++i) {
    if (chunk->constants.data[i] == value)
      return i;
  }
  *growl_dynarray_push(&chunk->constants, &vm->scratch) = value;
  return chunk->constants.count - 1;
}

static int is_number(const char *str, double *out) {
  char *end;
  double val = strtod(str, &end);
  if (*end == '\0' && end != str) {
    *out = val;
    return 1;
  }
  return 0;
}

__attribute__((format(printf, 2, 3))) static void
compile_error(GrowlCompileContext *ctx, const char *fmt, ...) {
  fprintf(stderr, "%s:%d:%d: compile error: ", ctx->file_path,
          ctx->lexer->start_row + 1, ctx->lexer->start_col + 1);
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fprintf(stderr, "\n");
}

static void optimize_tail_calls(Chunk *chunk) {
  size_t i = 0;
  while (i < chunk->count) {
    uint8_t opcode = chunk->data[i];
    size_t start = i++;
    if (opcode == GOP_PUSH_CONSTANT || opcode == GOP_WORD ||
        opcode == GOP_TAIL_WORD) {
      if (i < chunk->count)
        i += growl_sleb128_peek(&chunk->data[i], NULL);
    }
    if (i < chunk->count && chunk->data[i] == GOP_RETURN) {
      if (opcode == GOP_CALL) {
        chunk->data[start] = GOP_TAIL_CALL;
      } else if (opcode == GOP_WORD) {
        chunk->data[start] = GOP_TAIL_WORD;
      }
    }
  }
}

static int compile_token(GrowlCompileContext *ctx, Chunk *chunk);

static int compile_quotation(GrowlCompileContext *ctx, Chunk *chunk) {
  growl_lexer_next(ctx->lexer); // skip '['
  Chunk quot_chunk = {0};

  while (ctx->lexer->kind != ']' && ctx->lexer->kind != GTOK_EOF &&
         ctx->lexer->kind != GTOK_INVALID) {
    if (compile_token(ctx, &quot_chunk)) {
      return 1;
    }
  }
  if (ctx->lexer->kind != ']') {
    compile_error(ctx, "expected ']' to close quotation");
    return 1;
  }

  emit_byte(ctx->vm, &quot_chunk, GOP_RETURN);
  optimize_tail_calls(&quot_chunk);
  Growl quot = growl_make_quotation(ctx->vm, quot_chunk.data, quot_chunk.count,
                                    quot_chunk.constants.data,
                                    quot_chunk.constants.count);
  size_t idx = add_constant(ctx->vm, chunk, quot);
  emit_byte(ctx->vm, chunk, GOP_PUSH_CONSTANT);
  emit_sleb128(ctx->vm, chunk, (intptr_t)idx);
  growl_lexer_next(ctx->lexer);
  return 0;
}

static int compile_string(GrowlCompileContext *ctx, Chunk *chunk) {
  Growl str = growl_wrap_string_tenured(ctx->vm, ctx->lexer->buffer);
  size_t const_idx = add_constant(ctx->vm, chunk, str);
  emit_byte(ctx->vm, chunk, GOP_PUSH_CONSTANT);
  emit_sleb128(ctx->vm, chunk, (intptr_t)const_idx);
  growl_lexer_next(ctx->lexer);
  return 0;
}

static int compile_def(GrowlCompileContext *ctx) {
  growl_lexer_next(ctx->lexer);
  if (ctx->lexer->kind != GTOK_WORD) {
    compile_error(ctx, "expected name after 'def'");
    return 1;
  }

  char *name = growl_arena_strdup(&ctx->vm->scratch, ctx->lexer->buffer);
  growl_lexer_next(ctx->lexer);
  if (ctx->lexer->kind != GTOK_LBRACE) {
    compile_error(ctx, "expected '{' after def name '%s'", name);
    return 1;
  }

  GrowlDictionary *entry =
      growl_dictionary_upsert(&ctx->vm->dictionary, name, &ctx->vm->arena);
  GrowlDefinition *def = growl_dynarray_push(&ctx->vm->defs, &ctx->vm->arena);
  def->name = growl_arena_strdup(&ctx->vm->arena, name);
  def->callable = GROWL_NIL;
  entry->callable = GROWL_NIL;
  entry->index = ctx->vm->defs.count - 1;

  growl_lexer_next(ctx->lexer);
  Chunk fn_chunk = {0};
  while (ctx->lexer->kind != GTOK_RBRACE && ctx->lexer->kind != GTOK_EOF &&
         ctx->lexer->kind != GTOK_INVALID) {
    if (compile_token(ctx, &fn_chunk)) {
      return 1;
    }
  }

  if (ctx->lexer->kind != GTOK_RBRACE) {
    compile_error(ctx, "expected '}' to close def '%s'", name);
    return 1;
  }

  emit_byte(ctx->vm, &fn_chunk, GOP_RETURN);
  optimize_tail_calls(&fn_chunk);
  Growl fn =
      growl_make_quotation(ctx->vm, fn_chunk.data, fn_chunk.count,
                           fn_chunk.constants.data, fn_chunk.constants.count);

#if COMPILER_DEBUG
  GrowlQuotation *quot = growl_unwrap_quotation(ctx->vm, fn);
  fprintf(stderr, "=== %s ===\n", def->name);
  growl_disassemble(ctx->vm, quot);
#endif

  def->callable = fn;
  entry->callable = fn;

  growl_lexer_next(ctx->lexer);
  return 0;
}

static int compile_call(GrowlCompileContext *ctx, Chunk *chunk,
                        const char *name) {
  for (size_t i = 0; primitives[i].name != NULL; i++) {
    if (strcmp(name, primitives[i].name) == 0) {
      for (size_t j = 0; primitives[i].opcodes[j] != 0; j++)
        emit_byte(ctx->vm, chunk, primitives[i].opcodes[j]);
      growl_lexer_next(ctx->lexer);
      return 0;
    }
  }

  GrowlDictionary *entry =
      growl_dictionary_upsert(&ctx->vm->dictionary, name, NULL);
  if (entry == NULL) {
    compile_error(ctx, "undefined word '%s'", name);
    return 1;
  }
  emit_byte(ctx->vm, chunk, GOP_WORD);
  emit_sleb128(ctx->vm, chunk, entry->index);
  growl_lexer_next(ctx->lexer);
  return 0;
}

static int compile_load(GrowlCompileContext *ctx) {
  growl_lexer_next(ctx->lexer);

  if (ctx->lexer->kind != GTOK_STRING) {
    compile_error(ctx, "expected string after 'load'");
    return 1;
  }

  const char *path = ctx->lexer->buffer;
  char *resolved = growl_resolve_module_path(ctx, path, &ctx->vm->scratch);
  if (!resolved) {
    compile_error(ctx, "cannot find module '%s'", path);
    return 1;
  }

  FILE *file = fopen(resolved, "r");
  if (!file) {
    compile_error(ctx, "cannot open '%s'", resolved);
    free(resolved);
    return 1;
  }

  GrowlLexer mod_lexer = {0};
  mod_lexer.file = file;

  char *dir = growl_dirname(path, &ctx->vm->scratch);

  // I'd like to understand why clang-format does 4 spaces for aggregate
  // initialization.
  GrowlCompileContext mod_ctx = {
      .file_path = resolved,
      .file_dir = dir,
      .parent = ctx,
      .lexer = &mod_lexer,
      .vm = ctx->vm,
  };

  int result = 0;
  Growl obj = growl_compile_with_context(&mod_ctx);
  if (obj == GROWL_NIL) {
    return 1;
  }

  GrowlQuotation *q = growl_unwrap_quotation(ctx->vm, obj);
  if (growl_vm_execute(ctx->vm, q) != 0)
    result = 1;

  fclose(file);
  growl_lexer_next(ctx->lexer);

  return result;
}

static int compile_command(GrowlCompileContext *ctx, Chunk *chunk) {
  char *name = growl_arena_strdup(&ctx->vm->scratch, ctx->lexer->buffer);
  name[strlen(name) - 1] = '\0';
  growl_lexer_next(ctx->lexer);
  while (ctx->lexer->kind != GTOK_SEMICOLON && ctx->lexer->kind != GTOK_EOF &&
         ctx->lexer->kind != GTOK_INVALID) {
    if (compile_token(ctx, chunk))
      return 1;
  }
  if (ctx->lexer->kind != GTOK_SEMICOLON) {
    compile_error(ctx, "expected ';' to close command '%s:'", name);
    return 1;
  }
  return compile_call(ctx, chunk, name);
}

static int compile_word(GrowlCompileContext *ctx, Chunk *chunk) {
  char *name = ctx->lexer->buffer;
  size_t len = strlen(name);

  if (strcmp(name, "load") == 0)
    return compile_load(ctx);
  if (strcmp(name, "def") == 0)
    return compile_def(ctx);
  if (len > 1 && name[len - 1] == ':')
    return compile_command(ctx, chunk);

  double value;
  if (is_number(name, &value)) {
    size_t idx = add_constant(ctx->vm, chunk, growl_from_double(value));
    emit_byte(ctx->vm, chunk, GOP_PUSH_CONSTANT);
    emit_sleb128(ctx->vm, chunk, (intptr_t)idx);
    growl_lexer_next(ctx->lexer);
    return 0;
  }

  return compile_call(ctx, chunk, name);
}

static int compile_literal_value(GrowlCompileContext *ctx, Growl *out);

static int parse_list_value(GrowlCompileContext *ctx, Growl *out) {
  growl_lexer_next(ctx->lexer); // skip '('

  ObjectList elems = {0};
  while (ctx->lexer->kind != GTOK_RPAREN && ctx->lexer->kind != GTOK_EOF &&
         ctx->lexer->kind != GTOK_INVALID) {
    Growl *next = growl_dynarray_push(&elems, &ctx->vm->scratch);
    if (compile_literal_value(ctx, next))
      return 1;
  }

  if (ctx->lexer->kind != GTOK_RPAREN) {
    compile_error(ctx, "expected ')' to close list literal");
    return 1;
  }

  Growl lst = GROWL_NIL;
  for (size_t i = elems.count; i > 0; i--)
    lst = growl_cons_tenured(ctx->vm, elems.data[i - 1], lst);

  *out = lst;
  growl_lexer_next(ctx->lexer); // skip ')'
  return 0;
}

static int parse_tuple_value(GrowlCompileContext *ctx, Growl *out) {
  growl_lexer_next(ctx->lexer); // skip '#'
  if (ctx->lexer->kind != GTOK_LPAREN) {
    compile_error(ctx, "expected '(' after '#'");
    return 1;
  }
  growl_lexer_next(ctx->lexer); // skip '('

  ObjectList elems = {0};
  while (ctx->lexer->kind != GTOK_RPAREN && ctx->lexer->kind != GTOK_EOF &&
         ctx->lexer->kind != GTOK_INVALID) {
    Growl *next = growl_dynarray_push(&elems, &ctx->vm->scratch);
    if (compile_literal_value(ctx, next))
      return 1;
  }

  if (ctx->lexer->kind != GTOK_RPAREN) {
    compile_error(ctx, "expected ')' to close list literal");
    return 1;
  }

  Growl obj = growl_make_tuple_tenured(ctx->vm, elems.count);
  GrowlTuple *tup = growl_unwrap_tuple(ctx->vm, obj);

  for (size_t i = 0; i < elems.count; i++)
    tup->data[i] = elems.data[i];

  *out = obj;
  growl_lexer_next(ctx->lexer);
  return 0;
}

static int compile_literal_value(GrowlCompileContext *ctx, Growl *out) {
  switch (ctx->lexer->kind) {
  case GTOK_WORD: {
    double val;
    if (is_number(ctx->lexer->buffer, &val)) {
      *out = growl_from_double(val);
      growl_lexer_next(ctx->lexer);
      return 0;
    }
    if (strcmp(ctx->lexer->buffer, "nil") == 0) {
      *out = GROWL_NIL;
      growl_lexer_next(ctx->lexer);
      return 0;
    }
    compile_error(ctx, "expected literal value, got word '%s'",
                  ctx->lexer->buffer);
    return 1;
  }
  case GTOK_STRING:
    *out = growl_wrap_string_tenured(ctx->vm, ctx->lexer->buffer);
    growl_lexer_next(ctx->lexer);
    return 0;
  case GTOK_LPAREN:
    return parse_list_value(ctx, out);
  case GTOK_HASH:
    return parse_tuple_value(ctx, out);
  default:
    compile_error(ctx, "expected literal value");
    return 1;
  }
}

static int compile_list_literal(GrowlCompileContext *ctx, Chunk *chunk) {
  Growl lst;
  if (parse_list_value(ctx, &lst))
    return 1;
  size_t idx = add_constant(ctx->vm, chunk, lst);
  emit_byte(ctx->vm, chunk, GOP_PUSH_CONSTANT);
  emit_sleb128(ctx->vm, chunk, (intptr_t)idx);
  return 0;
}

static int compile_tuple_literal(GrowlCompileContext *ctx, Chunk *chunk) {
  Growl tup;
  if (parse_tuple_value(ctx, &tup))
    return 1;
  size_t idx = add_constant(ctx->vm, chunk, tup);
  emit_byte(ctx->vm, chunk, GOP_PUSH_CONSTANT);
  emit_sleb128(ctx->vm, chunk, (intptr_t)idx);
  return 0;
}

static int compile_token(GrowlCompileContext *ctx, Chunk *chunk) {
  switch (ctx->lexer->kind) {
  case GTOK_WORD:
    return compile_word(ctx, chunk);
  case GTOK_STRING:
    return compile_string(ctx, chunk);
  case GTOK_LBRACKET:
    return compile_quotation(ctx, chunk);
  case GTOK_LPAREN:
    return compile_list_literal(ctx, chunk);
  case GTOK_HASH:
    return compile_tuple_literal(ctx, chunk);
  case GTOK_SEMICOLON:
  case GTOK_RPAREN:
  case GTOK_RBRACKET:
  case GTOK_RBRACE:
    compile_error(ctx, "unexpected token '%c'", ctx->lexer->kind);
    return 1;
  case GTOK_INVALID:
    compile_error(ctx, "invalid token");
    return 1;
  default:
    compile_error(ctx, "unhandled token type '%c'", ctx->lexer->kind);
    return 1;
  }
}

Growl growl_compile_with_context(GrowlCompileContext *ctx) {
  Chunk chunk = {0};
  growl_lexer_next(ctx->lexer);
  while (ctx->lexer->kind != GTOK_EOF) {
    if (compile_token(ctx, &chunk))
      return GROWL_NIL;
  }
  emit_byte(ctx->vm, &chunk, GOP_RETURN);
  optimize_tail_calls(&chunk);
  return growl_make_quotation(ctx->vm, chunk.data, chunk.count,
                              chunk.constants.data, chunk.constants.count);
}

Growl growl_compile(GrowlVM *vm, GrowlLexer *lexer, const char *path,
                    const char *dirname) {
  GrowlCompileContext ctx = {0};
  ctx.vm = vm;
  ctx.lexer = lexer;
  ctx.file_path = path;
  ctx.file_dir = dirname;
  return growl_compile_with_context(&ctx);
}
