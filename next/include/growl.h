#ifndef GROWL_H
#define GROWL_H

#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>

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
typedef struct GrowlQuotation GrowlQuotation;
typedef struct GrowlCompose GrowlCompose;
typedef struct GrowlCurry GrowlCurry;
typedef struct GrowlAlienType GrowlAlienType;
typedef struct GrowlAlien GrowlAlien;
typedef struct GrowlGCArena GrowlGCArena;
typedef struct GrowlFrame GrowlFrame;
typedef struct GrowlVM GrowlVM;

enum {
  GROWL_TYPE_NIL,
  GROWL_TYPE_NUMBER,
  GROWL_TYPE_STRING,
  GROWL_TYPE_LIST,
  GROWL_TYPE_TUPLE,
  GROWL_TYPE_QUOTATION,
  GROWL_TYPE_COMPOSE,
  GROWL_TYPE_CURRY,
  GROWL_TYPE_ALIEN,
};

uint32_t growl_type(Growl obj);

struct GrowlObjectHeader {
  size_t size;
  uint32_t type;
};

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

struct GrowlGCArena {
  uint8_t *start, *end;
  uint8_t *free;
};

void growl_arena_init(GrowlGCArena *arena, size_t size);
void growl_arena_free(GrowlGCArena *arena);
void *growl_arena_alloc(GrowlGCArena *arena, size_t size, size_t align,
                        size_t count);
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

struct GrowlVM {
  GrowlGCArena from, to;
  GrowlGCArena arena;
  GrowlGCArena scratch;

  GrowlQuotation *quotation;
  uint8_t *ip;
  Growl wst[GROWL_STACK_SIZE], *sp;
  Growl rst[GROWL_STACK_SIZE], *rsp;
  GrowlFrame cst[GROWL_CALL_STACK_SIZE], *csp;

  GrowlQuotation *compose_trampoline;
  Growl next;

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
int vm_doquot(GrowlVM *vm, GrowlQuotation *quot);

#endif // GROWL_H
