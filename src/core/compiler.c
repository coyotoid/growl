#include <growl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "opcodes.h"
#include "sleb128.h"
#include "dynarray.h"

#define COMPILER_DEBUG 0

typedef struct {
  Growl *data;
  size_t count;
  size_t capacity;
} ConstantTable;

typedef struct {
  uint8_t *data;
  size_t count;
  size_t capacity;

  ConstantTable constants;
} Chunk;

typedef struct {
  const char *name;
  uint8_t opcodes[8];
} Primitive;

// clang-format off
Primitive primitives[] = {
  {"nil",     {GOP_PUSH_NIL, 0}},
  {"drop",    {GOP_DROP, 0}},
  {"dup",     {GOP_DUP, 0}},
  {"swap",    {GOP_SWAP, 0}},
  {"2drop",   {GOP_2DROP, 0}},
  {"2dup",    {GOP_2DUP, 0}},
  {"2swap",   {GOP_2SWAP, 0}},
  {"nip",     {GOP_NIP, 0}},
  {"over",    {GOP_OVER, 0}},
  {"bury",    {GOP_BURY, 0}},
  {"dig",     {GOP_DIG, 0}},
  {">r",      {GOP_TO_RETAIN, 0}},
  {"r>",      {GOP_FROM_RETAIN, 0}},
  {"?",       {GOP_CHOOSE, 0}},
  {"if",      {GOP_CHOOSE, GOP_CALL, 0}},
  {"call",    {GOP_CALL, 0}},
  {"compose", {GOP_COMPOSE, 0}},
  {"curry",   {GOP_CURRY, 0}},
  {"dip",     {GOP_DIP, 0}},
  {".",       {GOP_PPRINT, 0}},
  {"+",       {GOP_ADD, 0}},
  {"*",       {GOP_MUL, 0}},
  {"-",       {GOP_SUB, 0}},
  {"/",       {GOP_DIV, 0}},
  {"%",       {GOP_MOD, 0}},
  {"and",     {GOP_AND, 0}},
  {"or",      {GOP_OR, 0}},
  {"=",       {GOP_EQ, 0}},
  {"!=",      {GOP_NEQ, 0}},
  {"<",       {GOP_LT, 0}},
  {"<=",      {GOP_LTE, 0}},
  {">",       {GOP_GT, 0}},
  {">=",      {GOP_GTE, 0}},
  {"&",       {GOP_BAND, 0}},
  {"|",       {GOP_BOR, 0}},
  {"^",       {GOP_BXOR, 0}},
  {"~",       {GOP_BNOT, 0}},
  {NULL,      {0}}
};
// clang-format on

static void emit_byte(GrowlVM *vm, Chunk *chunk, uint8_t byte) {
  *push(chunk, &vm->scratch) = byte;
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
  *push(&chunk->constants, &vm->scratch) = value;
  return chunk->constants.count - 1;
}

static int is_integer(const char *str, long *out) {
  char *end;
  long val = strtol(str, &end, 0);
  if (*end == '\0' && end != str) {
    *out = val;
    return 1;
  }
  return 0;
}

__attribute__((format(printf, 2, 3))) static void
compile_error(GrowlLexer *lexer, const char *fmt, ...) {
  fprintf(stderr, "%d:%d: compile error: ", lexer->start_row + 1,
          lexer->start_col + 1);
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

static int compile_token(GrowlVM *vm, GrowlLexer *lexer, Chunk *chunk);

static int compile_quotation(GrowlVM *vm, GrowlLexer *lexer, Chunk *chunk) {
  growl_lexer_next(lexer); // skip '['
  Chunk quot_chunk = {0};

  while (lexer->kind != ']' && lexer->kind != GTOK_EOF &&
         lexer->kind != GTOK_INVALID) {
    if (compile_token(vm, lexer, &quot_chunk)) {
      return 1;
    }
  }
  if (lexer->kind != ']') {
    compile_error(lexer, "expected ']' to close quotation");
    return 1;
  }

  emit_byte(vm, &quot_chunk, GOP_RETURN);
  optimize_tail_calls(&quot_chunk);
  Growl quot = growl_make_quotation(vm, quot_chunk.data, quot_chunk.count,
                                    quot_chunk.constants.data,
                                    quot_chunk.constants.count);
  size_t idx = add_constant(vm, chunk, quot);
  emit_byte(vm, chunk, GOP_PUSH_CONSTANT);
  emit_sleb128(vm, chunk, (intptr_t)idx);
  growl_lexer_next(lexer);
  return 0;
}

static int compile_string(GrowlVM *vm, GrowlLexer *lexer, Chunk *chunk) {
  Growl str = growl_wrap_string_tenured(vm, lexer->buffer);
  size_t const_idx = add_constant(vm, chunk, str);
  emit_byte(vm, chunk, GOP_PUSH_CONSTANT);
  emit_sleb128(vm, chunk, (intptr_t)const_idx);
  growl_lexer_next(lexer);
  return 0;
}

static int compile_def(GrowlVM *vm, GrowlLexer *lexer) {
  growl_lexer_next(lexer);
  if (lexer->kind != GTOK_WORD) {
    compile_error(lexer, "expected name after 'def'");
    return 1;
  }

  char *name = growl_arena_strdup(&vm->scratch, lexer->buffer);
  growl_lexer_next(lexer);
  if (lexer->kind != GTOK_LBRACE) {
    compile_error(lexer, "expected '{' after def name '%s'", name);
    return 1;
  }

  // Add a forward declaration to the dictionary so the word can reference itself
  GrowlDictionary *entry =
      growl_dictionary_upsert(&vm->dictionary, name, &vm->arena);
  GrowlDefinition *def = push(&vm->defs, &vm->arena);
  def->name = growl_arena_strdup(&vm->arena, name);
  def->callable = GROWL_NIL; // Placeholder, will be filled in after compilation
  entry->callable = GROWL_NIL;
  entry->index = vm->defs.count - 1;

  growl_lexer_next(lexer);
  Chunk fn_chunk = {0};
  while (lexer->kind != GTOK_RBRACE && lexer->kind != GTOK_EOF &&
         lexer->kind != GTOK_INVALID) {
    if (compile_token(vm, lexer, &fn_chunk)) {
      return 1;
    }
  }

  if (lexer->kind != GTOK_RBRACE) {
    compile_error(lexer, "expected '}' to close def '%s'", name);
    return 1;
  }

  emit_byte(vm, &fn_chunk, GOP_RETURN);
  optimize_tail_calls(&fn_chunk);
  Growl fn =
      growl_make_quotation(vm, fn_chunk.data, fn_chunk.count,
                           fn_chunk.constants.data, fn_chunk.constants.count);

#if COMPILER_DEBUG
  GrowlQuotation *quot = growl_unwrap_quotation(fn);
  fprintf(stderr, "=== %s ===\n", def->name);
  growl_disassemble(vm, quot);
#endif

  // Now update the definition with the compiled quotation
  def->callable = fn;
  entry->callable = fn;

  growl_lexer_next(lexer);
  return 0;
}

static int compile_call(GrowlVM *vm, GrowlLexer *lexer, const char *name,
                        Chunk *chunk) {
  for (size_t i = 0; primitives[i].name != NULL; i++) {
    if (strcmp(name, primitives[i].name) == 0) {
      for (size_t j = 0; primitives[i].opcodes[j] != 0; j++)
        emit_byte(vm, chunk, primitives[i].opcodes[j]);
      growl_lexer_next(lexer);
      return 0;
    }
  }

  GrowlDictionary *entry = growl_dictionary_upsert(&vm->dictionary, name, NULL);
  if (entry == NULL) {
    compile_error(lexer, "undefined word '%s'", name);
    return 1;
  }
  emit_byte(vm, chunk, GOP_WORD);
  emit_sleb128(vm, chunk, entry->index);
  growl_lexer_next(lexer);
  return 0;
}

static int compile_command(GrowlVM *vm, GrowlLexer *lexer, Chunk *chunk) {
  char *name = growl_arena_strdup(&vm->scratch, lexer->buffer);
  name[strlen(name) - 1] = '\0';
  growl_lexer_next(lexer);
  while (lexer->kind != GTOK_SEMICOLON && lexer->kind != GTOK_EOF &&
         lexer->kind != GTOK_INVALID) {
    if (compile_token(vm, lexer, chunk)) {
      return 1;
    }
  }
  if (lexer->kind != GTOK_SEMICOLON) {
    compile_error(lexer, "expected ';' to close command '%s:'", name);
    return 1;
  }
  return compile_call(vm, lexer, name, chunk);
}

static int compile_word(GrowlVM *vm, GrowlLexer *lexer, Chunk *chunk) {
  char *text = lexer->buffer;
  size_t len = strlen(text);

  if (strcmp(text, "load") == 0) {
    // TODO: loading source files
    compile_error(lexer, "'load' nyi");
    return 1;
  }

  // Compile a definition
  if (strcmp(text, "def") == 0) {
    return compile_def(vm, lexer);
  }

  // Compile a command: word: args... ;
  if (len > 1 && text[len - 1] == ':') {
    return compile_command(vm, lexer, chunk);
  }

  // Compile an integer value
  long value;
  if (is_integer(text, &value)) {
    size_t idx = add_constant(vm, chunk, GROWL_NUM(value));
    emit_byte(vm, chunk, GOP_PUSH_CONSTANT);
    emit_sleb128(vm, chunk, (intptr_t)idx);
    growl_lexer_next(lexer);
    return 0;
  }

  return compile_call(vm, lexer, text, chunk);
}

static int compile_token(GrowlVM *vm, GrowlLexer *lexer, Chunk *chunk) {
  switch (lexer->kind) {
  case GTOK_WORD:
    return compile_word(vm, lexer, chunk);
  case GTOK_STRING:
    return compile_string(vm, lexer, chunk);
  case GTOK_LBRACKET:
    return compile_quotation(vm, lexer, chunk);
  case GTOK_SEMICOLON:
  case GTOK_RPAREN:
  case GTOK_RBRACKET:
  case GTOK_RBRACE:
    compile_error(lexer, "unexpected token '%c'", lexer->kind);
    return 1;
  case GTOK_INVALID:
    compile_error(lexer, "invalid token");
    return 1;
  default:
    compile_error(lexer, "unhandled token type '%c'", lexer->kind);
    return 1;
  }
}

Growl growl_compile(GrowlVM *vm, GrowlLexer *lexer) {
  Chunk chunk = {0};
  growl_lexer_next(lexer);
  while (lexer->kind != GTOK_EOF) {
    if (compile_token(vm, lexer, &chunk))
      return GROWL_NIL;
  }
  emit_byte(vm, &chunk, GOP_RETURN);
  optimize_tail_calls(&chunk);
  return growl_make_quotation(vm, chunk.data, chunk.count, chunk.constants.data,
                              chunk.constants.count);
}
