#include <growl.h>
#include <string.h>

#include "dynarray.h"

static void call_native(GrowlVM *vm, void *data) {
  void (*fn)(GrowlVM *) = data;
  fn(vm);
}

// clang-format off
static GrowlAlienType native_type = {
  .name = "native",
  .finalizer = NULL,
  .call = call_native,
};
// clang-format on

void growl_register_native(GrowlVM *vm, const char *name,
                           void (*fn)(GrowlVM *)) {
  Growl alien = growl_make_alien_tenured(vm, &native_type, (void *)fn);
  GrowlDictionary *entry =
      growl_dictionary_upsert(&vm->dictionary, name, &vm->arena);
  GrowlDefinition *def = push(&vm->defs, &vm->arena);
  def->name = growl_arena_strdup(&vm->arena, name);
  def->callable = alien;
  entry->callable = alien;
  entry->index = vm->defs.count - 1;
}
