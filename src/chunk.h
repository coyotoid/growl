#ifndef CHUNK_H
#define CHUNK_H

#include "common.h"
#include "object.h"

/** Bytecode chunk */
typedef struct Bc {
  U8 *items;
  Z count, capacity;
  struct {
    O *items;
    Z count, capacity;
  } constants;
} Bc;

V chunk_emit_byte(Bc *, U8);
V chunk_emit_sleb128(Bc *, I);
I chunk_add_constant(Bc *, O);
V chunk_free(Bc *);

#endif
