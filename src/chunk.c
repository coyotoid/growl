#include <stdlib.h>

#include "chunk.h"
#include "vendor/yar.h"

#if CHUNK_DEBUG
#include <stdio.h>
#endif

Bc *chunk_new(const char *name) {
  Bc *chunk = calloc(1, sizeof(Bc));
  chunk->name = name;
  chunk->ref = 1;
#if CHUNK_DEBUG
  fprintf(stderr, "DEBUG: created chunk %s at %p\n", chunk->name, (V *)chunk);
#endif
  return chunk;
}

V chunk_acquire(Bc *chunk) {
#if CHUNK_DEBUG
  fprintf(stderr, "DEBUG: acquiring chunk %s at %p\n", chunk->name, (V *)chunk);
#endif
  chunk->ref++;
}
V chunk_release(Bc *chunk) {
#if CHUNK_DEBUG
  fprintf(stderr, "DEBUG: releasing chunk %s at %p\n", chunk->name, (V *)chunk);
#endif

  if (--chunk->ref == 0) {
#if CHUNK_DEBUG
    fprintf(stderr, "DEBUG: freeing chunk %s at %p\n", chunk->name, (V *)chunk);
#endif
    yar_free(&chunk->constants);
    yar_free(chunk);
    free(chunk);
  }
}

V chunk_emit_byte(Bc *chunk, U8 byte) { *yar_append(chunk) = byte; }

V chunk_emit_sleb128(Bc *chunk, I num) {
  I more = 1;
  while (more) {
    U8 byte = num & 0x7f;
    num >>= 7;
    if ((num == 0 && !(byte & 0x40)) || (num == -1 && (byte & 0x40))) {
      more = 0;
    } else {
      byte |= 0x80;
    }
    chunk_emit_byte(chunk, byte);
  }
}

I chunk_add_constant(Bc *chunk, O value) {
  I mark = chunk->constants.count;
  *yar_append(&chunk->constants) = value;
  return mark;
}
