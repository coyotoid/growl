#ifndef GROWL_SLEB128_H
#define GROWL_SLEB128_H

#include <stdint.h>
#include <stddef.h>

intptr_t growl_sleb128_decode(uint8_t **ptr);
size_t growl_sleb128_peek(const uint8_t *ptr, intptr_t *out);

#endif // GROWL_SLEB128_H
