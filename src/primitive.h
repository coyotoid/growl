#ifndef PRIMITIVE_H
#define PRIMITIVE_H

#include "vm.h"

typedef struct Pr {
  const char *name;
  I (*fn)(Vm *);
} Pr;

extern Pr primitives_table[];
I prim_find(const char *name);

#endif
