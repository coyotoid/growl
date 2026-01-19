#include "chunk.h"
#include "vendor/yar.h"

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

V chunk_free(Bc *chunk) {
  yar_free(&chunk->constants);
  yar_free(chunk);
}
