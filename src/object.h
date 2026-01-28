#ifndef OBJECT_H
#define OBJECT_H

#include "common.h"

#define NIL ((O)0)
#define BOX(x) ((O)(x))
#define UNBOX(x) ((Hd *)(x))
#define IMM(x) ((O)(x) & (O)1)
#define NUM(x) (((O)((intptr_t)(x) << 1)) | (O)1)
#define ORD(x) ((intptr_t)(x) >> 1)

enum {
  OBJ_NIL = 0,
  OBJ_NUM = 1,
  OBJ_FWD = 2,
  OBJ_QUOT,
  OBJ_COMPOSE,
  OBJ_CURRY,
  OBJ_STR,
  OBJ_ARRAY,
  OBJ_USERDATA,
};

typedef uintptr_t O;

/** Object header */
typedef struct Hd {
  U32 size, type;
} Hd;

/** Composition */
typedef struct Qo {
  O first, second;
} Qo;

/** Curry */
typedef struct Qc {
  O value, callable;
} Qc; //

I type(O);
static inline I callable(O o) {
  I t = type(o);
  return t == OBJ_QUOT || t == OBJ_COMPOSE || t == OBJ_CURRY;
}

#endif
