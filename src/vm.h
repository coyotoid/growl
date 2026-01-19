#ifndef VM_H
#define VM_H

#include "common.h"

#include "chunk.h"
#include "gc.h"
#include "object.h"

enum {
  OP_NOP = 0,
  OP_CONST,       // Push constant to stack
  OP_DROP,
  OP_DUP,
  OP_SWAP,
  OP_JUMP,        // Relative jump
  OP_JUMP_IF_NIL, // Relative jump if top-of-stack is nil
  OP_CALL,
  OP_APPLY,
  OP_RETURN,
  OP_ADD,
};

#define STACK_SIZE 256

typedef struct Fr {
  Bc *chunk;
  U8 *ip;
} Fr;

typedef struct Vm {
  Gc gc;
  O stack[256], *sp;
  Fr rstack[256], *rsp;
  U8 *ip;
  Bc *chunk;
} Vm;

V vm_init(Vm *);
V vm_deinit(Vm *);

V vm_push(Vm *, O);
O vm_pop(Vm *);
O vm_peek(Vm *);

I vm_run(Vm *, Bc *, I);
#endif
