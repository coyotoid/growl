#include <growl.h>
#include <string.h>

GrowlDictionary *growl_dictionary_upsert(GrowlDictionary **dict,
                                         const char *name, GrowlArena *perm) {
  size_t len = strlen(name);
  for (uint64_t h = growl_hash_bytes((uint8_t *)name, len); *dict; h <<= 2) {
    if (strcmp(name, (*dict)->name) == 0) {
      return *dict;
    }
    dict = &(*dict)->child[h >> 62];
  }
  if (!perm) {
    return NULL;
  }
  *dict = growl_arena_new(perm, GrowlDictionary, 1);
  (*dict)->name = name;
  return *dict;
}


