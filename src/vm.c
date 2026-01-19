#include <stdio.h>

#include "gc.h"
#include "object.h"
#include "print.h"
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
  vm->chunk = NULL;
  gc_init(&vm->gc);

  for (Z i = 0; i < STACK_SIZE; i++) {
    vm->stack[i] = NIL;
    gc_addroot(&vm->gc, &vm->stack[i]);
  }
}

V vm_deinit(Vm *vm) { gc_deinit(&vm->gc); }

V vm_push(Vm *vm, O o) { *vm->sp++ = o; }
O vm_pop(Vm *vm) { return *--vm->sp; }
O vm_peek(Vm *vm) { return *(vm->sp - 1); }

V vm_rpush(Vm *vm, Bc *chunk, U8 *ip) {
  vm->rsp->chunk = chunk;
  vm->rsp->ip = ip;
  vm->rsp++;
}
Fr vm_rpop(Vm *vm) { return *--vm->rsp; }

I vm_run(Vm *vm, Bc *chunk, I offset) {
  I mark = gc_mark(&vm->gc);
  for (Z i = 0; i < chunk->constants.count; i++)
    gc_addroot(&vm->gc, &chunk->constants.items[i]);

#define BINOP(op)                                                              \
  {                                                                            \
    O b = vm_pop(vm);                                                          \
    O a = vm_pop(vm);                                                          \
    if (!IMM(a) || !IMM(b)) {                                                  \
      fprintf(stderr, "vm: arithmetic on non-number objects\n");               \
      return 0;                                                                \
    }                                                                          \
    vm_push(vm, NUM(ORD(a) op ORD(b)));                                        \
    break;                                                                     \
  }

  vm->ip = chunk->items + offset;
  vm->chunk = chunk;

  for (;;) {
    U8 opcode;
    switch (opcode = *vm->ip++) {
    case OP_NOP:
      continue;
    case OP_CONST: {
      I idx = decode_sleb128(&vm->ip);
      vm_push(vm, vm->chunk->constants.items[idx]);
      break;
    }
    case OP_JUMP: {
      I ofs = decode_sleb128(&vm->ip);
      vm->ip += ofs;
      break;
    }
    case OP_JUMP_IF_NIL: {
      I ofs = decode_sleb128(&vm->ip);
      if (vm_pop(vm) == NIL)
        vm->ip += ofs;
      break;
    }
    case OP_CALL: {
      I ofs = decode_sleb128(&vm->ip);
      vm_rpush(vm, vm->chunk, vm->ip);
      vm->ip = chunk->items + ofs;
      break;
    }
    case OP_APPLY: {
      O quot = vm_pop(vm);
      if (type(quot) == TYPE_QUOT) {
        Bc **ptr = (Bc **)(UNBOX(quot) + 1);
        Bc *chunk = *ptr;
        vm_rpush(vm, vm->chunk, vm->ip);
        vm->chunk = chunk;
        vm->ip = chunk->items;
      } else {
        fprintf(stderr, "vm: attempt to apply non-quotation object\n");
        return 0;
      }
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
    case OP_ADD:
      BINOP(+);
    default:
      fprintf(stderr, "unknown opcode %d\n", opcode);
      return 0;
    }
  }

done:
  gc_reset(&vm->gc, mark);
  if (vm->sp != vm->stack) {
    for (O *i = vm->stack; i < vm->sp; i++) {
      print(*i);
      putchar(' ');
    }
    putchar('\n');
  }
  return 1;
}
