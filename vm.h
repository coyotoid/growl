#ifndef VM_H
#define VM_H

#include "common.h"

#include "chunk.h"
#include "gc.h"
#include "object.h"

enum {
  OP_NOP = 0,
  OP_RETURN,
  OP_CONST,
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
