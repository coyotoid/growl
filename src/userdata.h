#ifndef USERDATA_H
#define USERDATA_H

#include "common.h"
#include "object.h"
#include "vm.h"

typedef struct Ut {
  const char *name;
  V (*finalizer)(V *);
} Ut;

typedef struct Ud {
  Ut *kind;
  V *data;
} Ud;

O userdata_make(Vm *, V *, Ut *);
Ud *userdata_unwrap(O, Ut *);

#endif
