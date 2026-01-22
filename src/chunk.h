#ifndef CHUNK_H
#define CHUNK_H

#define CHUNK_DEBUG DEBUG

#include "common.h"
#include "object.h"

typedef struct Bl {
  Z offset;
  I row;
  I col;
} Bl;

typedef struct Bc {
  I ref;
  const char *name;
  U8 *items;
  Z count, capacity;
  struct {
    O *items;
    Z count, capacity;
  } constants;
  struct {
    Bl *items;
    Z count, capacity;
  } lines;
} Bc;

Bc *chunk_new(const char *);
V chunk_acquire(Bc *);
V chunk_release(Bc *);

V chunk_emit_byte(Bc *, U8);
V chunk_emit_sleb128(Bc *, I);
I chunk_add_constant(Bc *, O);

V chunk_emit_byte_with_line(Bc *, U8, I, I);
I chunk_get_line(Bc *, Z, I*);

#endif
