#include <assert.h>
#include <growl.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdnoreturn.h>

#include "opcodes.h"
#include "sleb128.h"

#include <inttypes.h>
#include <stdio.h>

GrowlVM *growl_vm_init(void) {
  GrowlVM *vm = calloc(1, sizeof(GrowlVM));
  if (vm == NULL) {
    abort();
  }

  growl_arena_init(&vm->from, GROWL_HEAP_SIZE);
  growl_arena_init(&vm->to, GROWL_HEAP_SIZE);
  growl_arena_init(&vm->tenured, GROWL_HEAP_SIZE);
  growl_arena_init(&vm->scratch, GROWL_SCRATCH_SIZE);
  growl_arena_init(&vm->arena, GROWL_SCRATCH_SIZE);

  vm->dictionary = NULL;

  vm->sp = vm->wst;
  vm->rsp = vm->rst;
  vm->csp = vm->cst;

  vm->roots = NULL;
  vm->root_count = 0;
  vm->root_capacity = 0;

  static uint8_t compose_code[] = {GOP_CALL_NEXT};
  Growl compose_tramp = growl_make_quotation(vm, compose_code, 1, NULL, 0);
  vm->compose_trampoline =
      (GrowlQuotation *)(growl_unbox(vm, compose_tramp) + 1);

  static uint8_t return_code[] = {GOP_RETURN};
  Growl return_tramp = growl_make_quotation(vm, return_code, 1, NULL, 0);
  vm->return_trampoline = (GrowlQuotation *)(growl_unbox(vm, return_tramp) + 1);

  static uint8_t dip_code[] = {GOP_PUSH_NEXT, GOP_RETURN};
  Growl dip_tramp = growl_make_quotation(vm, dip_code, 2, NULL, 0);
  vm->dip_trampoline = (GrowlQuotation *)(growl_unbox(vm, dip_tramp) + 1);

  return vm;
}

void growl_vm_free(GrowlVM *vm) {
  growl_arena_free(&vm->from);
  growl_arena_free(&vm->to);
  growl_arena_free(&vm->tenured);
  growl_arena_free(&vm->scratch);
  growl_arena_free(&vm->arena);
  if (vm->roots != NULL)
    free(vm->roots);
  free(vm);
}

__attribute__((format(printf, 2, 3))) noreturn void
growl_vm_error(GrowlVM *vm, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  fprintf(stderr, "vm: ");
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);
  longjmp(vm->error, -1);
}

void growl_push(GrowlVM *vm, Growl obj) {
  if (vm->sp >= vm->wst + GROWL_STACK_SIZE)
    growl_vm_error(vm, "work stack overflow");
  *vm->sp++ = obj;
}

Growl growl_peek(GrowlVM *vm, size_t depth) {
  if (vm->sp <= vm->wst + depth)
    growl_vm_error(vm, "work stack underflow");
  return vm->sp[-(depth + 1)];
}

Growl growl_pop(GrowlVM *vm) {
  if (vm->sp <= vm->wst)
    growl_vm_error(vm, "work stack underflow");
  Growl obj = *--vm->sp;
  *vm->sp = GROWL_NIL;
  return obj;
}

void growl_rpush(GrowlVM *vm, Growl obj) {
  if (vm->rsp >= vm->rst + GROWL_STACK_SIZE)
    growl_vm_error(vm, "work stack overflow");
  *vm->rsp++ = obj;
}

Growl growl_rpop(GrowlVM *vm) {
  if (vm->rsp <= vm->rst)
    growl_vm_error(vm, "work stack underflow");
  Growl obj = *--vm->rsp;
  *vm->rsp = GROWL_NIL;
  return obj;
}

static void callstack_push(GrowlVM *vm, GrowlQuotation *q, uint8_t *ip) {
  if (vm->csp >= vm->cst + GROWL_CALL_STACK_SIZE)
    growl_vm_error(vm, "call stack overflow");
  vm->csp->quot = q;
  vm->csp->ip = ip;
  vm->csp->next = GROWL_NIL;
  vm->csp++;
}

static GrowlFrame callstack_pop(GrowlVM *vm) {
  if (vm->csp <= vm->cst)
    growl_vm_error(vm, "call stack underflow");
  return *--vm->csp;
}

static inline void dispatch(GrowlVM *vm, Growl obj) {
  for (;;) {
    switch (growl_type(vm, obj)) {
    case GROWL_TYPE_QUOTATION: {
      GrowlQuotation *q = (GrowlQuotation *)(growl_unbox(vm, obj) + 1);
      vm->current_quotation = q;
      vm->ip = q->data;
      return;
    }
    case GROWL_TYPE_COMPOSE: {
      GrowlCompose *c = (GrowlCompose *)(growl_unbox(vm, obj) + 1);
      callstack_push(vm, vm->compose_trampoline, vm->compose_trampoline->data);
      vm->csp[-1].next = c->second;
      obj = c->first;
      continue;
    }
    case GROWL_TYPE_CURRY: {
      GrowlCurry *c = (GrowlCurry *)(growl_unbox(vm, obj) + 1);
      growl_push(vm, c->value);
      obj = c->callable;
      continue;
    }
    case GROWL_TYPE_ALIEN: {
      GrowlAlien *alien = (GrowlAlien *)(growl_unbox(vm, obj) + 1);
      if (alien->type && alien->type->call) {
        alien->type->call(vm, alien->data);
        if (vm->csp != vm->cst) {
          GrowlFrame frame = callstack_pop(vm);
          vm->current_quotation = frame.quot;
          vm->ip = frame.ip;
          vm->next = frame.next;
        } else {
          vm->current_quotation = vm->return_trampoline;
          vm->ip = vm->return_trampoline->data;
        }
        return;
      }
      __attribute__((fallthrough));
    }
    default:
      growl_vm_error(vm, "attempt to call non-callable");
    }
  }
}

int growl_vm_execute(GrowlVM *vm, GrowlQuotation *quot) {
  size_t gc_mark = growl_gc_mark(vm);
  int result = setjmp(vm->error);

  if (result != 0) {
    growl_gc_reset(vm, gc_mark);
    return result;
  }

  vm->ip = quot->data;
  vm->current_quotation = quot;

  // clang-format off
#define VM_START() for (;;) { uint8_t opcode; switch(opcode = *vm->ip++) {
#define VM_END() }}
#define VM_DEFAULT() default:
#define VM_OP(op) case GOP_## op:
#define VM_NEXT() break
  // clang-format on

  VM_START()
  VM_OP(NOP) VM_NEXT();
  VM_OP(PUSH_NIL) {
    growl_push(vm, GROWL_NIL);
    VM_NEXT();
  }
  VM_OP(PUSH_CONSTANT) {
    intptr_t idx = growl_sleb128_decode(&vm->ip);
    GrowlTuple *constants =
        growl_unwrap_tuple(vm, vm->current_quotation->constants);
    if (constants != NULL) {
      if (idx >= 0 && (size_t)idx < constants->count) {
        growl_push(vm, constants->data[idx]);
      } else {
        growl_vm_error(vm, "constant index %" PRIdPTR " out of bounds", idx);
      }
    } else {
      growl_vm_error(vm, "attempt to index invalid constant table");
    }
    VM_NEXT();
  }
  VM_OP(PUSH_NEXT) {
    growl_push(vm, vm->next);
    vm->next = GROWL_NIL;
    VM_NEXT();
  }
  VM_OP(DROP) {
    (void)growl_pop(vm);
    VM_NEXT();
  }
  VM_OP(DUP) {
    growl_push(vm, growl_peek(vm, 0));
    VM_NEXT();
  }
  VM_OP(SWAP) {
    Growl b = growl_pop(vm);
    Growl a = growl_pop(vm);
    growl_push(vm, b);
    growl_push(vm, a);
    VM_NEXT();
  }
  VM_OP(2DROP) {
    (void)growl_pop(vm);
    (void)growl_pop(vm);
    VM_NEXT();
  }
  VM_OP(2DUP) {
    growl_push(vm, growl_peek(vm, 1));
    growl_push(vm, growl_peek(vm, 1));
    VM_NEXT();
  }
  VM_OP(2SWAP) {
    Growl d = growl_pop(vm);
    Growl c = growl_pop(vm);
    Growl b = growl_pop(vm);
    Growl a = growl_pop(vm);
    growl_push(vm, c);
    growl_push(vm, d);
    growl_push(vm, a);
    growl_push(vm, b);
    VM_NEXT();
  }
  VM_OP(NIP) {
    Growl b = growl_pop(vm);
    (void)growl_pop(vm);
    growl_push(vm, b);
    VM_NEXT();
  }
  VM_OP(OVER) {
    growl_push(vm, growl_peek(vm, 1));
    VM_NEXT();
  }
  VM_OP(BURY) {
    Growl c = growl_pop(vm);
    Growl b = growl_pop(vm);
    Growl a = growl_pop(vm);
    growl_push(vm, c);
    growl_push(vm, a);
    growl_push(vm, b);
    VM_NEXT();
  }
  VM_OP(DIG) {
    Growl c = growl_pop(vm);
    Growl b = growl_pop(vm);
    Growl a = growl_pop(vm);
    growl_push(vm, b);
    growl_push(vm, c);
    growl_push(vm, a);
    VM_NEXT();
  }
  VM_OP(TO_RETAIN) {
    growl_rpush(vm, growl_pop(vm));
    VM_NEXT();
  }
  VM_OP(FROM_RETAIN) {
    growl_push(vm, growl_rpop(vm));
    VM_NEXT();
  }
  VM_OP(CHOOSE) {
    Growl f = growl_pop(vm);
    Growl t = growl_pop(vm);
    Growl cond = growl_pop(vm);
    if (cond != GROWL_NIL) {
      growl_push(vm, t);
    } else {
      growl_push(vm, f);
    }
    VM_NEXT();
  }
  VM_OP(CALL) {
    Growl obj = growl_pop(vm);
    callstack_push(vm, vm->current_quotation, vm->ip);
    dispatch(vm, obj);
    VM_NEXT();
  }
  VM_OP(CALL_NEXT) {
    Growl callable = vm->next;
    vm->next = GROWL_NIL;
    dispatch(vm, callable);
    VM_NEXT();
  }
  VM_OP(TAIL_CALL) {
    Growl obj = growl_pop(vm);
    dispatch(vm, obj);
    VM_NEXT();
  }
  VM_OP(WORD) {
    intptr_t idx = growl_sleb128_decode(&vm->ip);
    GrowlDefinition *def = &vm->defs.data[idx];
    Growl word = def->callable;
    callstack_push(vm, vm->current_quotation, vm->ip);
    dispatch(vm, word);
    VM_NEXT();
  }
  VM_OP(TAIL_WORD) {
    intptr_t idx = growl_sleb128_decode(&vm->ip);
    GrowlDefinition *def = &vm->defs.data[idx];
    Growl word = def->callable;
    dispatch(vm, word);
    VM_NEXT();
  }
  VM_OP(RETURN) {
    if (vm->csp != vm->cst) {
      GrowlFrame frame = callstack_pop(vm);
      vm->current_quotation = frame.quot;
      vm->ip = frame.ip;
      vm->next = frame.next;
    } else {
      goto done;
    }
    VM_NEXT();
  }
  VM_OP(COMPOSE) {
    Growl second = growl_pop(vm);
    Growl first = growl_pop(vm);
    Growl composed = growl_compose(vm, first, second);
    if (composed == GROWL_NIL)
      growl_vm_error(vm, "attempt to compose with a non-callable");
    growl_push(vm, composed);
    VM_NEXT();
  }
  VM_OP(CURRY) {
    Growl callable = growl_pop(vm);
    Growl value = growl_pop(vm);
    Growl curried = growl_curry(vm, value, callable);
    if (curried == GROWL_NIL)
      growl_vm_error(vm, "attempt to curry with a non-callable");
    growl_push(vm, curried);
    VM_NEXT();
  }
  VM_OP(DIP) {
    Growl callable = growl_pop(vm);
    Growl x = growl_pop(vm);
    callstack_push(vm, vm->current_quotation, vm->ip);
    callstack_push(vm, vm->dip_trampoline, vm->dip_trampoline->data);
    vm->csp[-1].next = x;
    dispatch(vm, callable);
    VM_NEXT();
  }
  VM_OP(PPRINT) {
    growl_println(vm, growl_pop(vm));
    VM_NEXT();
  }

#define VM_BINOP(name, op)                                                     \
  case GOP_##name: {                                                           \
    Growl b = growl_pop(vm);                                                   \
    Growl a = growl_pop(vm);                                                   \
    if (GROWL_IS_NUM(b) && GROWL_IS_NUM(a)) {                                  \
      growl_push(vm,                                                           \
                 growl_from_double(growl_to_double(a) op growl_to_double(b))); \
    } else {                                                                   \
      growl_vm_error(vm, #op ": numeric op on non-numbers");                         \
    }                                                                          \
    VM_NEXT();                                                                 \
  }

  VM_BINOP(ADD, +);
  VM_BINOP(MUL, *);
  VM_BINOP(SUB, -);
  VM_BINOP(DIV, /);
  VM_OP(MOD) {
    Growl b = growl_pop(vm);
    Growl a = growl_pop(vm);
    if (GROWL_IS_NUM(b) && GROWL_IS_NUM(a)) {
      growl_push(
          vm, growl_from_double(fmod(growl_to_double(a), growl_to_double(b))));
    } else {
      growl_vm_error(vm, "%%: numeric op on non-numbers");
    }
    VM_NEXT();
  }
#define VM_BITOP(name, op)                                                     \
  case GOP_##name: {                                                           \
    Growl b = growl_pop(vm);                                                   \
    Growl a = growl_pop(vm);                                                   \
    if (GROWL_IS_NUM(b) && GROWL_IS_NUM(a)) {                                  \
      int32_t ia = (int32_t)growl_to_double(a);                                \
      int32_t ib = (int32_t)growl_to_double(b);                                \
      growl_push(vm, growl_from_double((double)(ia op ib)));                   \
    } else {                                                                   \
      growl_vm_error(vm, #op ": numeric op on non-numbers");                         \
    }                                                                          \
    VM_NEXT();                                                                 \
  }

  VM_BITOP(BAND, &);
  VM_BITOP(BOR, |);
  VM_BITOP(BXOR, ^);
  VM_OP(BNOT) {
    Growl a = growl_pop(vm);
    if (GROWL_IS_NUM(a)) {
      int32_t ia = (int32_t)growl_to_double(a);
      growl_push(vm, growl_from_double((double)(~ia)));
    } else {
      growl_vm_error(vm, "~: numeric op on non-numbers");
    }
    VM_NEXT();
  }
  VM_OP(AND) {
    Growl b = growl_pop(vm);
    Growl a = growl_pop(vm);
    if (a == GROWL_NIL) {
      growl_push(vm, a);
    } else {
      growl_push(vm, b);
    }
    VM_NEXT();
  }
  VM_OP(OR) {
    Growl b = growl_pop(vm);
    Growl a = growl_pop(vm);
    if (a == GROWL_NIL) {
      growl_push(vm, b);
    } else {
      growl_push(vm, a);
    }
    VM_NEXT();
  }
  VM_OP(EQ) {
    Growl b = growl_pop(vm);
    Growl a = growl_pop(vm);
    int equals = growl_equals(vm, a, b);
    if (equals) {
      growl_push(vm, growl_from_double(1.0));
    } else {
      growl_push(vm, GROWL_NIL);
    }
    VM_NEXT();
  }
  VM_OP(NEQ) {
    Growl b = growl_pop(vm);
    Growl a = growl_pop(vm);
    int equals = growl_equals(vm, a, b);
    if (!equals) {
      growl_push(vm, growl_from_double(1.0));
    } else {
      growl_push(vm, GROWL_NIL);
    }
    VM_NEXT();
  }

#define VM_CMPOP(name, op)                                                     \
  case GOP_##name: {                                                           \
    Growl b = growl_pop(vm);                                                   \
    Growl a = growl_pop(vm);                                                   \
    if (GROWL_IS_NUM(b) && GROWL_IS_NUM(a)) {                                  \
      if (growl_to_double(a) op growl_to_double(b)) {                          \
        growl_push(vm, growl_from_double(1.0));                                \
      } else {                                                                 \
        growl_push(vm, GROWL_NIL);                                             \
      }                                                                        \
    } else {                                                                   \
      growl_vm_error(vm, #op ": comparison on non-numbers");                         \
    }                                                                          \
    VM_NEXT();                                                                 \
  }

  VM_CMPOP(LT, <);
  VM_CMPOP(LTE, <=);
  VM_CMPOP(GT, >);
  VM_CMPOP(GTE, >=);

  VM_OP(LIST_CONS) {
    Growl tail = growl_pop(vm);
    Growl head = growl_pop(vm);
    growl_push(vm, growl_cons(vm, head, tail));
    VM_NEXT();
  }

  VM_OP(LIST_HEAD) {
    Growl value = growl_pop(vm);
    if (GROWL_IS_NIL(value))
      growl_vm_error(vm, "head: attempt to get head of nil");
    if (GROWL_IS_NUM(value))
      growl_vm_error(vm, "head: attempt to get head of number");
    GrowlObjectHeader *hdr = growl_unbox(vm, value);
    if (hdr->type == GROWL_TYPE_LIST) {
      GrowlList *lst = (GrowlList *)(hdr + 1);
      growl_push(vm, lst->head);
    } else {
      growl_vm_error(vm, "head: attempt to get head of non-list");
    }
    VM_NEXT();
  }

  VM_OP(LIST_TAIL) {
    Growl value = growl_pop(vm);
    if (GROWL_IS_NIL(value))
      growl_vm_error(vm, "tail: attempt to get tail of nil");
    if (GROWL_IS_NUM(value))
      growl_vm_error(vm, "tail: attempt to get tail of number");
    GrowlObjectHeader *hdr = growl_unbox(vm, value);
    if (hdr->type == GROWL_TYPE_LIST) {
      GrowlList *lst = (GrowlList *)(hdr + 1);
      growl_push(vm, lst->tail);
    } else {
      growl_vm_error(vm, "tail: attempt to get tail of non-list");
    }
    VM_NEXT();
  }

  VM_OP(LIST_LENGTH) {
    Growl value = growl_pop(vm);
    if (growl_type(vm, value) != GROWL_TYPE_LIST)
      growl_vm_error(vm, "list/length: can't get length of non-list");
    size_t len = growl_list_length(vm, value);
    growl_push(vm, growl_from_double(len));
    VM_NEXT();
  }

  VM_OP(LIST_TO_TUPLE) {
    Growl list = growl_pop(vm);
    if (growl_type(vm, list) != GROWL_TYPE_LIST)
      growl_vm_error(vm, "list->tuple: can't convert non-list to tuple");
    growl_push(vm, growl_list_to_tuple(vm, list));
    VM_NEXT();
  }

  VM_OP(TUPLE_GET) {
    Growl obj = growl_pop(vm);
    Growl n = growl_pop(vm);
    if (growl_type(vm, n) != GROWL_TYPE_NUMBER)
      growl_vm_error(vm, "tuple/get: expected a number for index");
    if (growl_type(vm, obj) != GROWL_TYPE_TUPLE)
      growl_vm_error(vm, "tuple/get: can't index a non-tuple");
    GrowlTuple *tup = growl_unwrap_tuple(vm, obj);
    assert(tup != NULL);
    intptr_t index = (intptr_t)(growl_to_double(n));
    if (index < 0 || index >= (intptr_t)tup->count)
      growl_vm_error(vm, "tuple/get: index out of range");
    growl_push(vm, tup->data[index]);
    VM_NEXT();
  }

  VM_OP(TUPLE_SET) {
    Growl obj = growl_pop(vm);
    Growl n = growl_pop(vm);
    Growl elt = growl_pop(vm);
    if (growl_type(vm, n) != GROWL_TYPE_NUMBER)
      growl_vm_error(vm, "tuple/set: expected a number for index");
    if (growl_type(vm, obj) != GROWL_TYPE_TUPLE)
      growl_vm_error(vm, "tuple/set: attempt to index a non-tuple");
    GrowlTuple *tup = growl_unwrap_tuple(vm, obj);
    assert(tup != NULL);
    intptr_t index = (intptr_t)(growl_to_double(n));
    if (index < 0 || index >= (intptr_t)tup->count)
      growl_vm_error(vm, "tuple/set: index out of range");
    tup->data[index] = elt;
    VM_NEXT();
  }

  VM_OP(TUPLE_CLONE) {
    Growl obj = growl_pop(vm);
    size_t mark = growl_gc_mark(vm);
    growl_gc_root(vm, &obj);

    GrowlTuple *tuple = growl_unwrap_tuple(vm, obj);
    if (tuple == NULL)
      growl_vm_error(vm, "tuple/clone: can't clone non-tuple");
    Growl new = growl_make_tuple(vm, tuple->count);
    GrowlTuple *new_tuple = growl_unwrap_tuple(vm, new);
    tuple = growl_unwrap_tuple(vm, obj);
    for (size_t i = 0; i < tuple->count; i++) {
      new_tuple->data[i] = tuple->data[i];
    }
    growl_push(vm, new);
    growl_gc_reset(vm, mark);
    VM_NEXT();
  }

  VM_OP(TUPLE_LENGTH) {
    Growl obj = growl_pop(vm);
    GrowlTuple *tuple = growl_unwrap_tuple(vm, obj);
    if (tuple == NULL)
      growl_vm_error(vm, "tuple/length: can't get length of non-tuple");
    growl_push(vm, growl_from_double(tuple->count));
    VM_NEXT();
  }

  VM_DEFAULT() { growl_vm_error(vm, "unknown opcode %d", opcode); }
  VM_END()

done:
  growl_gc_reset(vm, gc_mark);
  return 0;
}
