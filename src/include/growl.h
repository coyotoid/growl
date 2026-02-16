#ifndef GROWL_H
#define GROWL_H

#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdnoreturn.h>
#include <string.h>

typedef uint64_t Growl;

/* NaN-boxing: all tagged values have bits 62:50 set (quiet NaN + marker) */
#define GROWL_QNAN UINT64_C(0x7FFC000000000000)
#define GROWL_NIL (GROWL_QNAN)

/* Type tags (bits 49:48) */
#define GROWL_TAG_PTR 2

/* Canonical NaN for IEEE 754 doubles */
#define GROWL_CANON_NAN UINT64_C(0x7FF8000000000000)

/* Arena tags (bits 47:46) */
#define GROWL_ARENA_NURSERY 0
#define GROWL_ARENA_TENURED 1

/* Type checks */
#define GROWL_IS_NIL(x) ((x) == GROWL_NIL)
#define GROWL_IS_NUM(x) (((x) & GROWL_QNAN) != GROWL_QNAN)
#define GROWL_IS_PTR(x)                                                        \
  (((x) & UINT64_C(0xFFFF000000000000)) ==                                    \
   (GROWL_QNAN | (UINT64_C(2) << 48)))

/* Double encoding via memcpy (type-punning safe) */
static inline Growl growl_from_double(double d) {
  uint64_t bits;
  memcpy(&bits, &d, sizeof(bits));
  if ((bits & GROWL_QNAN) == GROWL_QNAN)
    bits = GROWL_CANON_NAN;
  return bits;
}

static inline double growl_to_double(Growl v) {
  double d;
  memcpy(&d, &v, sizeof(d));
  return d;
}

/* Pointer construction/extraction */
#define GROWL_MKPTR(arena, offset)                                             \
  (GROWL_QNAN | (UINT64_C(2) << 48) | ((uint64_t)(arena) << 46) |            \
   ((uint64_t)(offset) & UINT64_C(0x3FFFFFFFFFFF)))
#define GROWL_PTR_ARENA(x) (((x) >> 46) & UINT64_C(3))
#define GROWL_PTR_OFFSET(x) ((x) & UINT64_C(0x3FFFFFFFFFFF))

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
typedef struct GrowlModule GrowlModule;
typedef struct GrowlCompileContext GrowlCompileContext;
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

uint32_t growl_type(GrowlVM *vm, Growl obj);
int growl_equals(GrowlVM *vm, Growl a, Growl b);

uint64_t growl_hash_combine(uint64_t a, uint64_t b);
uint64_t growl_hash_bytes(const uint8_t *data, size_t len);
uint64_t growl_hash(GrowlVM *vm, Growl obj);

void growl_print_to(GrowlVM *vm, FILE *file, Growl value);
void growl_print(GrowlVM *vm, Growl value);
void growl_println(GrowlVM *vm, Growl value);

struct GrowlString {
  size_t len;
  char data[];
};

Growl growl_make_string(GrowlVM *vm, size_t len);
Growl growl_wrap_string(GrowlVM *vm, const char *cstr);
Growl growl_wrap_string_tenured(GrowlVM *vm, const char *cstr);
GrowlString *growl_unwrap_string(GrowlVM *vm, Growl obj);

struct GrowlList {
  Growl head, tail;
};

struct GrowlTuple {
  size_t count;
  Growl data[];
};

GrowlTuple *growl_unwrap_tuple(GrowlVM *vm, Growl obj);

struct GrowlTable {};

GrowlTable *growl_unwrap_table(GrowlVM *vm, Growl obj);

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

int growl_callable(GrowlVM *vm, Growl obj);
Growl growl_make_quotation(GrowlVM *vm, const uint8_t *code, size_t code_size,
                           const Growl *constants, size_t constants_size);
GrowlQuotation *growl_unwrap_quotation(GrowlVM *vm, Growl obj);
Growl growl_compose(GrowlVM *vm, Growl first, Growl second);
GrowlCompose *growl_unwrap_compose(GrowlVM *vm, Growl obj);
Growl growl_curry(GrowlVM *vm, Growl value, Growl callable);
GrowlCurry *growl_unwrap_curry(GrowlVM *vm, Growl obj);

struct GrowlAlienType {
  const char *name;
  void (*call)(GrowlVM *, void *);
  void (*finalizer)(void *);
};

struct GrowlAlien {
  GrowlAlienType *type;
  void *data;
};

Growl growl_make_alien(GrowlVM *vm, GrowlAlienType *type, void *data);
Growl growl_make_alien_tenured(GrowlVM *vm, GrowlAlienType *type, void *data);
GrowlAlien *growl_unwrap_alien(GrowlVM *vm, Growl obj, GrowlAlienType *type);
void growl_register_native(GrowlVM *vm, const char *name,
                           void (*fn)(GrowlVM *));

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
#define GROWL_SCRATCH_SIZE (1024 * 1024)

struct GrowlFrame {
  GrowlQuotation *quot;
  uint8_t *ip;
  Growl next;
};

struct GrowlDefinition {
  const char *name;
  Growl callable;
};

struct GrowlDefinitionTable {
  GrowlDefinition *data;
  size_t count, capacity;
};

struct GrowlDictionary {
  GrowlDictionary *child[4];
  const char *name;
  Growl callable;
  size_t index;
};

struct GrowlModule {
  char *resolved_path;
  GrowlModule *next;
};

struct GrowlCompileContext {
  GrowlCompileContext *parent;
  GrowlVM *vm;
  GrowlLexer *lexer;
  const char *name;
  const char *file_path;
  const char *file_dir;
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
  GrowlQuotation *return_trampoline;
  GrowlQuotation *dip_trampoline;
  Growl next;

  Growl **roots;
  size_t root_count, root_capacity;

  jmp_buf error;
};

static inline GrowlObjectHeader *growl_unbox(GrowlVM *vm, Growl val) {
  uint64_t offset = GROWL_PTR_OFFSET(val);
  switch (GROWL_PTR_ARENA(val)) {
  case GROWL_ARENA_NURSERY:
    return (GrowlObjectHeader *)(vm->from.start + offset);
  case GROWL_ARENA_TENURED:
    return (GrowlObjectHeader *)(vm->tenured.start + offset);
  default:
    return NULL;
  }
}

static inline Growl growl_box_nursery(GrowlVM *vm, GrowlObjectHeader *hdr) {
  uint64_t offset = (uint64_t)((uint8_t *)hdr - vm->from.start);
  return GROWL_MKPTR(GROWL_ARENA_NURSERY, offset);
}

static inline Growl growl_box_tenured(GrowlVM *vm, GrowlObjectHeader *hdr) {
  uint64_t offset = (uint64_t)((uint8_t *)hdr - vm->tenured.start);
  return GROWL_MKPTR(GROWL_ARENA_TENURED, offset);
}

GrowlVM *growl_vm_init(void);
void growl_vm_free(GrowlVM *vm);
GrowlObjectHeader *growl_gc_alloc(GrowlVM *vm, size_t size);
GrowlObjectHeader *growl_gc_alloc_tenured(GrowlVM *vm, size_t size);
void growl_gc_collect(GrowlVM *vm);
void growl_gc_root(GrowlVM *vm, Growl *ptr);
size_t growl_gc_mark(GrowlVM *vm);
void growl_gc_reset(GrowlVM *vm, size_t mark);
void growl_push(GrowlVM *vm, Growl obj);
Growl growl_peek(GrowlVM *vm, size_t depth);
Growl growl_pop(GrowlVM *vm);
void growl_rpush(GrowlVM *vm, Growl obj);
Growl growl_rpop(GrowlVM *vm);
noreturn void growl_vm_error(GrowlVM *vm, const char *fmt, ...);
int growl_vm_execute(GrowlVM *vm, GrowlQuotation *quot);

/** Compiler */
Growl growl_compile_with_context(GrowlCompileContext *ctx);
Growl growl_compile(GrowlVM *vm, GrowlLexer *lexer, const char *path,
                    const char *dirname);
void growl_disassemble(GrowlVM *vm, GrowlQuotation *quot);

/** Extra libraries */
void growl_register_file_library(GrowlVM *vm);

#endif // GROWL_H
