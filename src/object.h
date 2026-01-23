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
  OBJ_FWD = 2,
  OBJ_QUOT,
  OBJ_STR,
  OBJ_USERDATA,
};

enum {
  TYPE_NIL = 0,
  TYPE_NUM = 1,
  TYPE_FWD = OBJ_FWD,
  TYPE_QUOT = OBJ_QUOT,
  TYPE_STR = OBJ_STR,
  TYPE_USERDATA = OBJ_USERDATA,
};

typedef uintptr_t O;

/** Object header */
typedef struct Hd {
  U32 size, type;
} Hd;

I type(O);

#endif
