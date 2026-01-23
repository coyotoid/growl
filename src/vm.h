#ifndef VM_H
#define VM_H

#include <setjmp.h>

#include "common.h"

#include "arena.h"
#include "chunk.h"
#include "dictionary.h"
#include "gc.h"
#include "object.h"

enum {
  OP_NOP = 0,
  OP_CONST, // Push constant to stack
  OP_NIL,   // Push constant to stack
  OP_DROP,
  OP_DUP,
  OP_SWAP,
  OP_NIP,
  OP_OVER,
  OP_BURY,
  OP_DIG,
  OP_TOR,    // Push from stack to retain stack
  OP_FROMR,  // Push from retain stack to stack
  OP_DOWORD, // Call word from dictionary by name hash
  OP_CALL,
  OP_TAIL_DOWORD, // Tail call to dictionary word (reuses current frame)
  OP_TAIL_CALL,   // Tail call to quotation (reuses current frame)
  OP_PRIM,
  OP_COMPOSE,
  OP_CURRY,
  OP_RETURN,
  OP_CHOOSE,
  OP_ADD,
  OP_SUB,
  OP_MUL,
  OP_DIV,
  OP_MOD,
  OP_EQ,
  OP_NEQ,
  OP_LOGAND,
  OP_LOGOR,
  OP_LOGXOR,
  OP_LOGNOT,
  OP_LT,
  OP_GT,
  OP_LTE,
  OP_GTE,
  OP_AND,
  OP_OR,
  OP_CONCAT,
  OP_CALL_NEXT,
};

#define STACK_SIZE 256

typedef struct Fr {
  Bc *chunk;
  U8 *ip;
  O obj;
} Fr;

typedef struct Vm {
  Gc gc;
  O stack[STACK_SIZE], *sp;
  O tstack[STACK_SIZE], *tsp;
  Fr rstack[STACK_SIZE], *rsp;
  U8 *ip;
  Bc *chunk;
  Dt *dictionary;
  Ar arena;
  jmp_buf error;
  Bc *trampoline;
  O next_call;

  // These objects need to stay as roots!
  O stdin, stdout, stderr;
} Vm;

enum {
  VM_ERR_STACK_OVERFLOW = 1,
  VM_ERR_STACK_UNDERFLOW,
  VM_ERR_TYPE,
  VM_ERR_RUNTIME
};

V vm_init(Vm *);
V vm_deinit(Vm *);
I vm_run(Vm *, Bc *, I);

V vm_push(Vm *, O);
O vm_pop(Vm *);
V vm_tpush(Vm *, O);
O vm_tpop(Vm *);

#endif
