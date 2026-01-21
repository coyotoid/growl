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
    yar_free(&chunk->lines);
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

V chunk_emit_byte_with_line(Bc *chunk, U8 byte, I line, I col) {
  *yar_append(chunk) = byte;
  if (chunk->lines.count == 0 ||
      chunk->lines.items[chunk->lines.count - 1].row != line ||
      chunk->lines.items[chunk->lines.count - 1].col != col) {
    Bl *entry = yar_append(&chunk->lines);
    entry->offset = chunk->count - 1;
    entry->row = line;
    entry->col = col;
  }
}

I chunk_get_line(Bc *chunk, Z offset, I *out_col) {
  if (chunk->lines.count == 0)
    return -1;
  Z left = 0, right = chunk->lines.count - 1;
  while (left < right) {
    Z mid = left + (right - left + 1) / 2;
    if (chunk->lines.items[mid].offset <= offset)
      left = mid;
    else
      right = mid - 1;
  }
  if (out_col)
    *out_col = chunk->lines.items[left].col;
  return chunk->lines.items[left].row;
}
