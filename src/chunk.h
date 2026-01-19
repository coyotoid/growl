#ifndef CHUNK_H
#define CHUNK_H

#define CHUNK_DEBUG 0

#include "common.h"
#include "object.h"

/** Bytecode chunk */
typedef struct Bc {
  I ref;
  U8 *items;
  Z count, capacity;
  struct {
    O *items;
    Z count, capacity;
  } constants;
} Bc;

Bc *chunk_new(V);
V chunk_acquire(Bc *);
V chunk_release(Bc *);

V chunk_emit_byte(Bc *, U8);
V chunk_emit_sleb128(Bc *, I);
I chunk_add_constant(Bc *, O);

#endif
