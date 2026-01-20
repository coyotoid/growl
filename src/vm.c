#include <stdio.h>

#include "arena.h"
#include "chunk.h"
#include "compile.h"
#include "dictionary.h"
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
}

V vm_deinit(Vm *vm) {
  gc_collect(vm);
  gc_deinit(&vm->gc);
  arena_free(&vm->arena);
  vm->dictionary = NULL;
}

V vm_push(Vm *vm, O o) { *vm->sp++ = o; }
O vm_pop(Vm *vm) {
  O o = *--vm->sp;
  *vm->sp = NIL;
  return o;
}
O vm_peek(Vm *vm) { return *(vm->sp - 1); }

V vm_rtpush(Vm *vm, O o) { *vm->tsp++ = o; }
O vm_rtpop(Vm *vm) {
  O o = *--vm->tsp;
  *vm->tsp = NIL;
  return o;
}

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

#define CMPOP(op)                                                              \
  {                                                                            \
    O b = vm_pop(vm);                                                          \
    O a = vm_pop(vm);                                                          \
    if (!IMM(a) || !IMM(b)) {                                                  \
      fprintf(stderr, "vm: arithmetic on non-number objects\n");               \
      return 0;                                                                \
    }                                                                          \
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
      vm_rtpush(vm, vm_pop(vm));
      break;
    }
    case OP_FROMR: {
      vm_push(vm, vm_rtpop(vm));
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
    case OP_DOWORD: {
      I hash = decode_sleb128(&vm->ip);
      Dt *word = lookup_hash(&vm->dictionary, hash);
      if (!word) {
        fprintf(stderr, "vm: word not found (hash = %lx)\n", hash);
        return 0;
      }
      vm_rpush(vm, vm->chunk, vm->ip);
      vm->chunk = word->chunk;
      vm->ip = word->chunk->items;
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
    case OP_TAIL_CALL: {
      I ofs = decode_sleb128(&vm->ip);
      // Tail call: reuse current frame, just jump
      vm->ip = chunk->items + ofs;
      break;
    }
    case OP_TAIL_DOWORD: {
      I hash = decode_sleb128(&vm->ip);
      Dt *word = lookup_hash(&vm->dictionary, hash);
      if (!word) {
        fprintf(stderr, "vm: word not found (hash = %lx)\n", hash);
        return 0;
      }
      // Tail call: reuse current frame
      vm->chunk = word->chunk;
      vm->ip = word->chunk->items;
      break;
    }
    case OP_TAIL_APPLY: {
      O quot = vm_pop(vm);
      if (type(quot) == TYPE_QUOT) {
        Bc **ptr = (Bc **)(UNBOX(quot) + 1);
        Bc *chunk = *ptr;
        // Tail call: reuse current frame
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
