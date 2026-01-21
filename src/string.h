#include "common.h"
#include "object.h"
#include "vm.h"

/** String */
typedef struct Str {
  Z len;
  char data[];
} Str;

O string_make(Vm *, const char *, I);
Str *string_unwrap(O);
O string_concat(Vm *, Str *, Str *);
