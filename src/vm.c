#include <setjmp.h>
#include <stdio.h>

#include "arena.h"
#include "chunk.h"
#include "compile.h"
#include "dictionary.h"
#include "gc.h"
#include "object.h"
#include "primitive.h"
#include "userdata.h"
#include "file.h"
#include "string.h"
#include "vm.h"

static I decode_sleb128(U8 **ptr) {
  I result = 0;
  I shift = 0;
  U8 byte;

  do {
    byte = **ptr;
    (*ptr)++;
    result |= (I)(byte & 0x7F) << shift;
    shift += 7;
  } while (byte & 0x80);

  if ((shift < 64) && (byte & 0x40)) {
    result |= -(1LL << shift);
  }

  return result;
}

V vm_init(Vm *vm) {
  vm->sp = vm->stack;
  vm->rsp = vm->rstack;
  vm->tsp = vm->tstack;
  vm->chunk = NULL;
  vm->dictionary = NULL;

  gc_init(&vm->gc);
  arena_init(&vm->arena, 1024 * 1024);

  for (Z i = 0; i < STACK_SIZE; i++) {
    vm->stack[i] = NIL;
    vm->tstack[i] = NIL;
    gc_addroot(&vm->gc, &vm->stack[i]);
    gc_addroot(&vm->gc, &vm->tstack[i]);
  }

  vm->stdin = userdata_make(vm, (void *)stdin, &userdata_file);
  vm->stdout = userdata_make(vm, (void *)stdout, &userdata_file);
  vm->stderr = userdata_make(vm, (void *)stderr, &userdata_file);

  gc_addroot(&vm->gc, &vm->stdin);
  gc_addroot(&vm->gc, &vm->stdout);
  gc_addroot(&vm->gc, &vm->stderr);
}

V vm_deinit(Vm *vm) {
  // Free all definitions
  Dt *dstack[256];
  Dt **dsp = dstack;
  *dsp++ = vm->dictionary;

  while (dsp > dstack) {
    Dt *node = *--dsp;
    if (!node)
      continue;
    if (node->chunk != NULL)
      chunk_release(node->chunk);
    for (I i = 0; i < 4; i++) {
      if (node->child[i] != NULL)
        *dsp++ = node->child[i];
    }
  }

  arena_free(&vm->arena);
  vm->dictionary = NULL;

  // Run final GC pass
  gc_collect(vm);
  gc_deinit(&vm->gc);
}

static V vm_error(Vm *vm, I error, const char *message) {
  I col = -1;
  I line = chunk_get_line(vm->chunk, vm->ip - vm->chunk->items, &col);
  fprintf(stderr, "error at %ld:%ld: %s\n", line + 1, col + 1, message);
  longjmp(vm->error, error);
}

V vm_push(Vm *vm, O o) {
  if (vm->sp >= vm->stack + STACK_SIZE)
    vm_error(vm, VM_ERR_STACK_OVERFLOW, "data stack overflow");
  *vm->sp++ = o;
}
O vm_pop(Vm *vm) {
  if (vm->sp <= vm->stack)
    vm_error(vm, VM_ERR_STACK_UNDERFLOW, "data stack underflow");
  O o = *--vm->sp;
  *vm->sp = NIL;
  return o;
}

V vm_tpush(Vm *vm, O o) {
  if (vm->tsp >= vm->tstack + STACK_SIZE)
    vm_error(vm, VM_ERR_STACK_OVERFLOW, "retain stack overflow");
  *vm->tsp++ = o;
}
O vm_tpop(Vm *vm) {
  if (vm->tsp <= vm->tstack)
    vm_error(vm, VM_ERR_STACK_UNDERFLOW, "retain stack underflow");
  O o = *--vm->tsp;
  *vm->tsp = NIL;
  return o;
}

V vm_rpush(Vm *vm, Bc *chunk, U8 *ip) {
  if (vm->rsp >= vm->rstack + STACK_SIZE)
    vm_error(vm, VM_ERR_STACK_OVERFLOW, "return stack overflow");
  vm->rsp->chunk = chunk;
  vm->rsp->ip = ip;
  vm->rsp++;
}
Fr vm_rpop(Vm *vm) {
  if (vm->rsp <= vm->rstack)
    vm_error(vm, VM_ERR_STACK_UNDERFLOW, "return stack underflow");
  return *--vm->rsp;
}

I vm_run(Vm *vm, Bc *chunk, I offset) {
  I mark = gc_mark(&vm->gc);
  if (setjmp(vm->error) != 0) {
    gc_reset(&vm->gc, mark);
    return 0;
  }

  for (Z i = 0; i < chunk->constants.count; i++)
    gc_addroot(&vm->gc, &chunk->constants.items[i]);

#define BINOP(op)                                                              \
  {                                                                            \
    O b = vm_pop(vm);                                                          \
    O a = vm_pop(vm);                                                          \
    if (!IMM(a) || !IMM(b))                                                    \
      vm_error(vm, VM_ERR_TYPE, "numop on non-numeric objects");               \
    vm_push(vm, NUM(ORD(a) op ORD(b)));                                        \
    break;                                                                     \
  }

#define CMPOP(op)                                                              \
  {                                                                            \
    O b = vm_pop(vm);                                                          \
    O a = vm_pop(vm);                                                          \
    if (!IMM(a) || !IMM(b))                                                    \
      vm_error(vm, VM_ERR_TYPE, "comparison on non-numeric objects");          \
    vm_push(vm, (ORD(a) op ORD(b)) ? NUM(1) : NIL);                            \
    break;                                                                     \
  }

  vm->ip = chunk->items + offset;
  vm->chunk = chunk;

  for (;;) {
    U8 opcode;
    switch (opcode = *vm->ip++) {
    case OP_NOP:
      continue;
    case OP_NIL:
      vm_push(vm, NIL);
      break;
    case OP_CONST: {
      I idx = decode_sleb128(&vm->ip);
      vm_push(vm, vm->chunk->constants.items[idx]);
      break;
    }
    case OP_DROP: {
      (void)vm_pop(vm);
      break;
    }
    case OP_DUP: {
      O obj = vm_pop(vm);
      vm_push(vm, obj);
      vm_push(vm, obj);
      break;
    }
    case OP_SWAP: {
      O b = vm_pop(vm);
      O a = vm_pop(vm);
      vm_push(vm, b);
      vm_push(vm, a);
      break;
    }
    case OP_NIP: {
      /* a b -> b */
      O b = vm_pop(vm);
      (void)vm_pop(vm);
      vm_push(vm, b);
      break;
    }
    case OP_OVER: {
      /* a b -> a b a */
      O b = vm_pop(vm);
      O a = vm_pop(vm);
      vm_push(vm, a);
      vm_push(vm, b);
      vm_push(vm, a);
      break;
    }
    case OP_BURY: {
      /* a b c - c a b */
      O c = vm_pop(vm);
      O b = vm_pop(vm);
      O a = vm_pop(vm);
      vm_push(vm, c);
      vm_push(vm, a);
      vm_push(vm, b);
      break;
    }
    case OP_DIG: {
      /* a b c - b c a */
      O c = vm_pop(vm);
      O b = vm_pop(vm);
      O a = vm_pop(vm);
      vm_push(vm, b);
      vm_push(vm, c);
      vm_push(vm, a);
      break;
    }
    case OP_TOR: {
      vm_tpush(vm, vm_pop(vm));
      break;
    }
    case OP_FROMR: {
      vm_push(vm, vm_tpop(vm));
      break;
    }
    case OP_DOWORD: {
      I idx = decode_sleb128(&vm->ip);
      Dt *word = vm->chunk->symbols.items[idx].resolved;
      if (!word)
        vm_error(vm, VM_ERR_RUNTIME, "word not found");
      vm_rpush(vm, vm->chunk, vm->ip);
      vm->chunk = word->chunk;
      vm->ip = word->chunk->items;
      break;
    }
    case OP_CALL: {
      O quot = vm_pop(vm);
      if (type(quot) == TYPE_QUOT) {
        Bc **ptr = (Bc **)(UNBOX(quot) + 1);
        Bc *chunk = *ptr;
        vm_rpush(vm, vm->chunk, vm->ip);
        vm->chunk = chunk;
        vm->ip = chunk->items;
      } else {
        vm_error(vm, VM_ERR_TYPE, "attempt to call non-quotation object");
      }
      break;
    }
    case OP_TAIL_DOWORD: {
      I idx = decode_sleb128(&vm->ip);
      Dt *word = vm->chunk->symbols.items[idx].resolved;
      if (!word)
        vm_error(vm, VM_ERR_RUNTIME, "word not found");
      vm->chunk = word->chunk;
      vm->ip = word->chunk->items;
      break;
    }
    case OP_TAIL_CALL: {
      O quot = vm_pop(vm);
      if (type(quot) == TYPE_QUOT) {
        Bc **ptr = (Bc **)(UNBOX(quot) + 1);
        Bc *chunk = *ptr;
        vm->chunk = chunk;
        vm->ip = chunk->items;
      } else {
        vm_error(vm, VM_ERR_TYPE, "attempt to call non-quotation object\n");
      }
      break;
    }
    case OP_PRIM: {
      I idx = decode_sleb128(&vm->ip);
      Pr prim = primitives_table[idx];
      I err = prim.fn(vm);
      if (err != 0)
        vm_error(vm, err, "primitive call failed");
      break;
    }
    case OP_RETURN:
      if (vm->rsp != vm->rstack) {
        Fr frame = vm_rpop(vm);
        vm->chunk = frame.chunk;
        vm->ip = frame.ip;
      } else {
        goto done;
      }
      break;
    case OP_CHOOSE: {
      O fals = vm_pop(vm);
      O tru = vm_pop(vm);
      O cond = vm_pop(vm);
      if (cond == NIL) {
        vm_push(vm, fals);
      } else {
        vm_push(vm, tru);
      }
      break;
    }
    case OP_ADD:
      BINOP(+);
    case OP_SUB:
      BINOP(-);
    case OP_MUL:
      BINOP(*);
    case OP_DIV:
      BINOP(/);
    case OP_MOD:
      BINOP(%);
    case OP_LOGAND:
      BINOP(&);
    case OP_LOGOR:
      BINOP(|);
    case OP_LOGXOR:
      BINOP(^);
    case OP_LOGNOT: {
      O o = vm_pop(vm);
      if (!IMM(o))
        vm_error(vm, VM_ERR_TYPE, "numop on non-number");
      vm_push(vm, NUM(~ORD(o)));
      break;
    }
    case OP_EQ:
      CMPOP(==);
    case OP_NEQ:
      CMPOP(!=);
    case OP_LT:
      CMPOP(<);
    case OP_GT:
      CMPOP(>);
    case OP_LTE:
      CMPOP(<=);
    case OP_GTE:
      CMPOP(>=);
    case OP_AND: {
      O b = vm_pop(vm);
      O a = vm_pop(vm);
      if (a == NIL) {
        vm_push(vm, NIL);
      } else {
        vm_push(vm, b);
      }
      break;
    }
    case OP_OR: {
      O b = vm_pop(vm);
      O a = vm_pop(vm);
      if (a == NIL) {
        vm_push(vm, b);
      } else {
        vm_push(vm, a);
      }
      break;
    }
    case OP_CONCAT: {
      O b = vm_pop(vm);
      if (type(b) != TYPE_STR)
        vm_error(vm, VM_ERR_TYPE, "expected string");
      O a = vm_pop(vm);
      if (type(a) != TYPE_STR)
        vm_error(vm, VM_ERR_TYPE, "expected string");
      vm_push(vm, string_concat(vm, a, b));
      break;
    }
    default:
      vm_error(vm, VM_ERR_RUNTIME, "unknown opcode");
    }
  }
done:
  gc_reset(&vm->gc, mark);
  return 1;
}
