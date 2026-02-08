#ifndef GROWL_H
#define GROWL_H

#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef uintptr_t Growl;

#define GROWL_NIL ((Growl)(0))
#define GROWL_BOX(x) ((Growl)(x))
#define GROWL_UNBOX(x) ((GrowlObjectHeader *)(x))
#define GROWL_IMM(x) ((Growl)(x) & (Growl)1)
#define GROWL_NUM(x) (((Growl)((intptr_t)(x) << 1)) | (Growl)1)
#define GROWL_ORD(x) ((intptr_t)(x) >> 1)

typedef struct GrowlObjectHeader GrowlObjectHeader;
typedef struct GrowlString GrowlString;
typedef struct GrowlList GrowlList;
typedef struct GrowlTuple GrowlTuple;
typedef struct GrowlTable GrowlTable;
typedef struct GrowlQuotation GrowlQuotation;
typedef struct GrowlCompose GrowlCompose;
typedef struct GrowlCurry GrowlCurry;
typedef struct GrowlAlienType GrowlAlienType;
typedef struct GrowlAlien GrowlAlien;
typedef struct GrowlLexer GrowlLexer;
typedef struct GrowlArena GrowlArena;
typedef struct GrowlFrame GrowlFrame;
typedef struct GrowlDictionary GrowlDictionary;
typedef struct GrowlDefinition GrowlDefinition;
typedef struct GrowlDefinitionTable GrowlDefinitionTable;
typedef struct GrowlVM GrowlVM;

enum {
  GROWL_TYPE_NIL,
  GROWL_TYPE_NUMBER,
  GROWL_TYPE_STRING,
  GROWL_TYPE_LIST,
  GROWL_TYPE_TUPLE,
  GROWL_TYPE_TABLE,
  GROWL_TYPE_QUOTATION,
  GROWL_TYPE_COMPOSE,
  GROWL_TYPE_CURRY,
  GROWL_TYPE_ALIEN,
};

struct GrowlObjectHeader {
  size_t size;
  uint32_t type;
};

uint32_t growl_type(Growl obj);
uint64_t growl_hash_combine(uint64_t a, uint64_t b);
uint64_t growl_hash_bytes(const uint8_t *data, size_t len);
uint64_t growl_hash(Growl obj);

void growl_print_to(FILE *file, Growl value);
void growl_print(Growl value);
void growl_println(Growl value);

int growl_equals(Growl a, Growl b);

struct GrowlString {
  size_t len;
  char data[];
};

Growl growl_make_string(GrowlVM *vm, size_t len);
Growl growl_wrap_string(GrowlVM *vm, const char *cstr);
GrowlString *growl_unwrap_string(Growl obj);

struct GrowlList {
  Growl head, tail;
};

struct GrowlTuple {
  size_t count;
  Growl data[];
};

GrowlTuple *growl_unwrap_tuple(Growl obj);

struct GrowlTable {};

GrowlTable *growl_unwrap_table(Growl obj);
GrowlTable *growl_table_upsert(GrowlVM *vm, GrowlTable **table, Growl key);

struct GrowlQuotation {
  size_t count;
  Growl constants;
  uint8_t data[];
};

struct GrowlCompose {
  Growl first, second;
};

struct GrowlCurry {
  Growl value, callable;
};

int growl_callable(Growl obj);
Growl growl_make_quotation(GrowlVM *vm, const uint8_t *code, size_t code_size,
                           const Growl *constants, size_t constants_size);
GrowlQuotation *growl_unwrap_quotation(Growl obj);
Growl growl_compose(GrowlVM *vm, Growl first, Growl second);
GrowlCompose *growl_unwrap_compose(Growl obj);
Growl growl_curry(GrowlVM *vm, Growl value, Growl callable);
GrowlCurry *growl_unwrap_curry(Growl obj);

struct GrowlAlienType {
  const char *name;
  void (*finalizer)(void *);
};

struct GrowlAlien {
  GrowlAlienType *type;
  void *data;
};

Growl growl_make_alien(GrowlVM *vm, GrowlAlienType *type, void *data);
GrowlAlien *growl_unwrap_alien(Growl obj, GrowlAlienType *type);

/** Lexer */
enum {
  GTOK_INVALID = -1,
  GTOK_EOF = 0,
  GTOK_WORD = 'a',
  GTOK_STRING = '"',
  GTOK_SEMICOLON = ';',
  GTOK_LPAREN = '(',
  GTOK_RPAREN = ')',
  GTOK_LBRACKET = '[',
  GTOK_RBRACKET = ']',
  GTOK_LBRACE = '{',
  GTOK_RBRACE = '}',
};

#define GROWL_LEXER_BUFSIZE 256

struct GrowlLexer {
  int kind;
  int cursor;
  int current_row, current_col;
  int start_row, start_col;
  FILE *file;
  char buffer[GROWL_LEXER_BUFSIZE];
};

int growl_lexer_next(GrowlLexer *lexer);

struct GrowlArena {
  uint8_t *start, *end;
  uint8_t *free;
};

void growl_arena_init(GrowlArena *arena, size_t size);
void growl_arena_free(GrowlArena *arena);
void *growl_arena_alloc(GrowlArena *arena, size_t size, size_t align,
                        size_t count);
char *growl_arena_strdup(GrowlArena *ar, const char *str);

#define growl_arena_new(a, t, n)                                               \
  (t *)growl_arena_alloc(a, sizeof(t), _Alignof(t), n)

#define GROWL_STACK_SIZE 128
#define GROWL_CALL_STACK_SIZE 64
#define GROWL_HEAP_SIZE (4 * 1024 * 1024)
#define GROWL_ARENA_SIZE (2 * 1024 * 1024)
#define GROWL_SCRATCH_SIZE (1024 * 1024)

struct GrowlFrame {
  GrowlQuotation *quot;
  uint8_t *ip;
  Growl next;
};

struct GrowlDefinition {
  const char *name;
  GrowlQuotation *quotation;
};

struct GrowlDefinitionTable {
  GrowlDefinition *data;
  size_t count, capacity;
};

struct GrowlDictionary {
  GrowlDictionary *child[4];
  const char *name;
  GrowlQuotation *quotation;
  size_t index;
};

GrowlDictionary *growl_dictionary_upsert(GrowlDictionary **dict,
                                         const char *name, GrowlArena *perm);

struct GrowlVM {
  GrowlArena from, to;
  GrowlArena tenured;
  GrowlArena scratch;
  GrowlArena arena;

  GrowlDictionary *dictionary;
  GrowlDefinitionTable defs;

  GrowlQuotation *current_quotation;
  uint8_t *ip;
  Growl wst[GROWL_STACK_SIZE], *sp;
  Growl rst[GROWL_STACK_SIZE], *rsp;
  GrowlFrame cst[GROWL_CALL_STACK_SIZE], *csp;

  GrowlQuotation *compose_trampoline;
  Growl compose_next;

  Growl **roots;
  size_t root_count, root_capacity;

  jmp_buf error;
};

GrowlVM *growl_vm_init(void);
void growl_vm_free(GrowlVM *vm);
GrowlObjectHeader *growl_gc_alloc(GrowlVM *vm, size_t size);
GrowlObjectHeader *growl_gc_alloc_tenured(GrowlVM *vm, size_t size);
void growl_gc_collect(GrowlVM *vm);
void growl_gc_root(GrowlVM *vm, Growl *ptr);
size_t growl_gc_mark(GrowlVM *vm);
void growl_gc_reset(GrowlVM *vm, size_t mark);
int growl_vm_execute(GrowlVM *vm, GrowlQuotation *quot);

/** Compiler */
Growl growl_compile(GrowlVM *vm, GrowlLexer *lexer);
void growl_disassemble(GrowlVM *vm, GrowlQuotation *quot);

#endif // GROWL_H
