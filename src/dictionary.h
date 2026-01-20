#ifndef DICTIONARY_H
#define DICTIONARY_H

#include "arena.h"
#include "chunk.h"

typedef struct Dt Dt;
struct Dt {
  Dt *child[4];
  const char *name;
  U64 hash;
  Bc *chunk;
};

U64 hash64(const char *);
Dt *upsert(Dt **, const char *, Ar *);
Dt *lookup_hash(Dt **, U64);

#endif
