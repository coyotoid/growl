#include "vm.h"
#include "gc.h"

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
  gc_init(&vm->gc);

  for (Z i = 0; i < STACK_SIZE; i++) {
    vm->stack[i] = NIL;
    gc_addroot(&vm->gc, &vm->stack[i]);
  }
}

V vm_push(Vm *vm, O o) { *vm->sp++ = o; }
O vm_pop(Vm *vm) { return *--vm->sp; }
O vm_peek(Vm *vm) { return *(vm->sp - 1); }

V vm_run(Vm *vm, Bc *chunk, I offset) {
  I mark = gc_mark(&vm->gc);
  for (Z i = 0; i < chunk->constants.count; i++)
    gc_addroot(&vm->gc, &chunk->constants.items[i]);

  vm->ip = chunk->items + offset;
  for (;;) {
    U8 opcode;
    switch (opcode = *vm->ip++) {
    case OP_NOP:
      break;
    case OP_RETURN:
      return;
    case OP_CONST: {
      I idx = decode_sleb128(&vm->ip);
      vm_push(vm, chunk->constants.items[idx]);
      break;
    }
    }
  }

  gc_reset(&vm->gc, mark);
}
