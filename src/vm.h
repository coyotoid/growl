#ifndef VM_H
#define VM_H

#include "common.h"

#include "chunk.h"
#include "gc.h"
#include "object.h"

enum {
  OP_NOP = 0,
  OP_CONST,       // Push constant to stack
  OP_JUMP,        // Relative jump
  OP_JUMP_IF_NIL, // Relative jump if top-of-stack is nil
  OP_DOWORD,
  OP_CALL,
  OP_RETURN,
};

#define STACK_SIZE 256

typedef struct Vm {
  Gc gc;
  O stack[256], *sp;
  U rstack[256], *rsp;
  U8 *ip;
} Vm;

V vm_init(Vm *);
V vm_push(Vm *, O);
O vm_pop(Vm *);
O vm_peek(Vm *);
V vm_run(Vm *, Bc *, I);
#endif
