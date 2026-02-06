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

static void push_call(GrowlVM *vm, GrowlQuotation *q, uint8_t *ip) {
  if (vm->csp >= vm->cst + GROWL_CALL_STACK_SIZE)
    vm_error(vm, "call stack overflow");
  vm->csp->quot = q;
  vm->csp->ip = ip;
  vm->csp++;
}
static GrowlFrame pop_call(GrowlVM *vm) {
  if (vm->csp <= vm->cst)
    vm_error(vm, "call stack underflow");
  return *--vm->csp;
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

  for (;;) {
    uint8_t opcode;
    switch (opcode = *vm->ip++) {
    case GOP_NOP:
      break;
    case GOP_PUSH_NIL:
      growl_push(vm, GROWL_NIL);
      break;
    case GOP_PUSH_CONSTANT: {
      intptr_t idx = growl_sleb128_decode(&vm->ip);
      if (constants != NULL) {
        growl_push(vm, constants->data[idx]);
      } else {
        vm_error(vm, "constant index %" PRIdPTR " out of bounds", idx);
      }
      break;
    case GOP_CALL: { // TODO: compose and curry
      Growl obj = growl_pop(vm);
      push_call(vm, vm->quotation, vm->ip);
      GrowlQuotation *obj_quot = growl_unwrap_quotation(obj);
      if (obj_quot == NULL)
        vm_error(vm, "attempt to call non-callable");
      vm->quotation = obj_quot;
      vm->ip = obj_quot->data;
      break;
    }
    case GOP_RETURN:
      if (vm->csp != vm->cst) {
        GrowlFrame frame = pop_call(vm);
        vm->quotation = frame.quot;
        vm->ip = frame.ip;
      } else {
        goto done;
      }
      break;
    }
    default:
      vm_error(vm, "unknown opcode %d", opcode);
    }
  }

done:
  growl_gc_reset(vm, gc_mark);
  return 0;
}
