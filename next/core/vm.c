#include <growl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdnoreturn.h>

#include "opcodes.h"
#include "sleb128.h"

#include <inttypes.h>
#include <stdio.h>

GrowlVM *growl_vm_init(void) {
  GrowlVM *mem = malloc(sizeof(GrowlVM));
  if (mem == NULL) {
    abort();
  }

  growl_arena_init(&mem->from, GROWL_HEAP_SIZE);
  growl_arena_init(&mem->to, GROWL_HEAP_SIZE);
  growl_arena_init(&mem->arena, GROWL_ARENA_SIZE);
  growl_arena_init(&mem->scratch, GROWL_SCRATCH_SIZE);

  mem->sp = mem->wst;
  mem->rsp = mem->rst;
  mem->csp = mem->cst;

  for (size_t i = 0; i < GROWL_STACK_SIZE; ++i) {
    mem->wst[i] = 0;
    mem->rst[i] = 0;
  }

  mem->roots = NULL;
  mem->root_count = 0;
  mem->root_capacity = 0;

  // TODO: initialize compose trampoline

  return mem;
}

void growl_vm_free(GrowlVM *vm) {
  growl_arena_free(&vm->from);
  growl_arena_free(&vm->to);
  growl_arena_free(&vm->arena);
  growl_arena_free(&vm->scratch);
  if (vm->roots != NULL)
    free(vm->roots);
  free(vm);
}

__attribute__((format(printf, 2, 3))) static noreturn void
vm_error(GrowlVM *vm, const char *fmt, ...) {
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
    vm_error(vm, "work stack overflow");
  *vm->sp++ = obj;
}

Growl growl_peek(GrowlVM *vm, size_t depth) {
  if (vm->sp <= vm->wst + depth)
    vm_error(vm, "work stack underflow");
  return vm->sp[-(depth + 1)];
}

Growl growl_pop(GrowlVM *vm) {
  if (vm->sp <= vm->wst)
    vm_error(vm, "work stack underflow");
  Growl obj = *--vm->sp;
  *vm->sp = GROWL_NIL;
  return obj;
}

void growl_rpush(GrowlVM *vm, Growl obj) {
  if (vm->rsp >= vm->rst + GROWL_STACK_SIZE)
    vm_error(vm, "work stack overflow");
  *vm->rsp++ = obj;
}

Growl growl_rpop(GrowlVM *vm) {
  if (vm->rsp <= vm->rst)
    vm_error(vm, "work stack underflow");
  Growl obj = *--vm->rsp;
  *vm->rsp = GROWL_NIL;
  return obj;
}

static void callstack_push(GrowlVM *vm, GrowlQuotation *q, uint8_t *ip) {
  if (vm->csp >= vm->cst + GROWL_CALL_STACK_SIZE)
    vm_error(vm, "call stack overflow");
  vm->csp->quot = q;
  vm->csp->ip = ip;
  vm->csp++;
}

static GrowlFrame callstack_pop(GrowlVM *vm) {
  if (vm->csp <= vm->cst)
    vm_error(vm, "call stack underflow");
  return *--vm->csp;
}

static inline void dispatch(GrowlVM *vm, Growl obj) {
  for (;;) {
    switch (growl_type(obj)) {
    case GROWL_TYPE_QUOTATION: {
      GrowlQuotation *q = (GrowlQuotation *)(GROWL_UNBOX(obj) + 1);
      vm->quotation = q;
      vm->ip = q->data;
      return;
    }
    case GROWL_TYPE_COMPOSE: {
      GrowlCompose *c = (GrowlCompose *)(GROWL_UNBOX(obj) + 1);
      callstack_push(vm, vm->compose_trampoline, vm->compose_trampoline->data);
      vm->csp[-1].next = c->second;
      obj = c->first;
      continue;
    }
    case GROWL_TYPE_CURRY: {
      GrowlCurry *c = (GrowlCurry *)(GROWL_UNBOX(obj) + 1);
      growl_push(vm, c->value);
      obj = c->callable;
      continue;
    }
    default:
      vm_error(vm, "attempt to call non-callable");
    }
  }
}
int vm_doquot(GrowlVM *vm, GrowlQuotation *quot) {
  size_t gc_mark = growl_gc_mark(vm);
  int result = setjmp(vm->error);

  if (result != 0) {
    growl_gc_reset(vm, gc_mark);
    return result;
  }

  GrowlTuple *constants = growl_unwrap_tuple(quot->constants);
  if (constants != NULL) {
    for (size_t i = 0; i < constants->count; ++i) {
      growl_gc_root(vm, &constants->data[i]);
    }
  }

  vm->ip = quot->data;
  vm->quotation = quot;

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
    if (constants != NULL) {
      growl_push(vm, constants->data[idx]);
    } else {
      vm_error(vm, "constant index %" PRIdPTR " out of bounds", idx);
    }
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
  VM_OP(TO_RETAIN) {
    growl_rpush(vm, growl_pop(vm));
    VM_NEXT();
  }
  VM_OP(FROM_RETAIN) {
    growl_push(vm, growl_rpop(vm));
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
  VM_OP(CALL) { // TODO: compose and curry
    Growl obj = growl_pop(vm);
    callstack_push(vm, vm->quotation, vm->ip);
    dispatch(vm, obj);
    VM_NEXT();
  }
  VM_OP(CALL_NEXT) {
    growl_push(vm, vm->next);
    vm->next = GROWL_NIL;
    __attribute__((__fallthrough__));
  }
  VM_OP(TAIL_CALL) {
    Growl obj = growl_pop(vm);
    dispatch(vm, obj);
    VM_NEXT();
  }
  VM_OP(RETURN) {
    if (vm->csp != vm->cst) {
      GrowlFrame frame = callstack_pop(vm);
      vm->quotation = frame.quot;
      vm->ip = frame.ip;
    } else {
      goto done;
    }
    VM_NEXT();
  }
  VM_DEFAULT() { vm_error(vm, "unknown opcode %d", opcode); }
  VM_END()

done:
  growl_gc_reset(vm, gc_mark);
  return 0;
}
