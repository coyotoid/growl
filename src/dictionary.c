#include <string.h>

#include "arena.h"
#include "common.h"
#include "dictionary.h"

U64 hash64(const char *str) {
  I len = strlen(str);
  U64 h = 0x100;
  for (I i = 0; i < len; i++) {
    h ^= str[i] & 255;
    h *= 1111111111111111111;
  }
  return h;
}

Dt *upsert(Dt **env, const char *key, Ar *a) {
  U64 hash = hash64(key);
  for (U64 h = hash; *env; h <<= 2) {
    if (hash == (*env)->hash)
      return *env;
    env = &(*env)->child[h >> 62];
  }
  if (!a)
    return 0;
  *env = arena_alloc(a, 1, Dt);
  (*env)->name = key;
  (*env)->hash = hash;
  return *env;
}

Dt *lookup_hash(Dt **env, U64 hash) {
  for (U64 h = hash; *env; h <<= 2) {
    if ((*env)->hash == hash)
      return *env;
    env = &(*env)->child[h >> 62];
  }
  return NULL;
}
