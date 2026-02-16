#include "sleb128.h"

intptr_t growl_sleb128_decode(uint8_t **ptr) {
  intptr_t result = 0;
  intptr_t shift = 0;
  uint8_t byte;

  do {
    byte = **ptr;
    (*ptr)++;
    result |= (intptr_t)(byte & 0x7F) << shift;
    shift += 7;
  } while (byte & 0x80);

  if (shift < 64 && byte & 0x40) {
    result |= -(1LL << shift);
  }

  return result;
}

size_t growl_sleb128_peek(const uint8_t *ptr, intptr_t *out) {
  intptr_t result = 0, shift = 0;
  size_t bytes = 0;
  uint8_t byte;

  do {
    byte = ptr[bytes];
    bytes++;
    result |= (intptr_t)(byte & 0x7f) << shift;
    shift += 7;
  } while (byte & 0x80);

  if (shift < 64 && byte & 0x40) {
    result |= -(1LL << shift);
  }

  if (out)
    *out = result;
  return bytes;
}
