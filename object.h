#ifndef OBJECT_H
#define OBJECT_H

#include "common.h"

#define NIL ((O)0)
#define BOX(x) ((O)(x))
#define UNBOX(x) ((Hd *)(x))
#define IMM(x) ((O)(x) & (O)1)
#define NUM(x) (((O)((intptr_t)(x) << 1)) | (O)1)
#define ORD(x) ((O)(x) >> 1)

enum {
  TYPE_FWD,
};

typedef uintptr_t O;

/** Object header */
typedef struct Hd {
  U32 size, type;
} Hd;

#endif
