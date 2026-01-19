#include <stdio.h>

#include "debug.h"
#include "print.h"
#include "vm.h"

static I decode_sleb128(U8 *ptr, Z *bytes_read) {
  I result = 0;
  I shift = 0;
  U8 byte;
  Z count = 0;
  do {
    byte = ptr[count++];
    result |= (I)(byte & 0x7F) << shift;
    shift += 7;
  } while (byte & 0x80);
  if ((shift < 64) && (byte & 0x40))
    result |= -(1LL << shift);
  *bytes_read = count;
  return result;
}

V disassemble(Bc *chunk, const char *name) {
  printf("=== %s ===\n", name);
  Z offset = 0;
  while (offset < chunk->count) {
    offset = disassemble_instruction(chunk, offset);
  }
}

Z disassemble_instruction(Bc *chunk, Z offset) {
  printf("%04zu ", offset);
  U8 opcode = chunk->items[offset++];
  switch (opcode) {
  case OP_NOP:
    printf("NOP\n");
    return offset;
  case OP_CONST: {
    Z bytes_read;
    I idx = decode_sleb128(&chunk->items[offset], &bytes_read);
    printf("CONST %ld", idx);
    if (idx >= 0 && idx < (I)chunk->constants.count) {
      printf(" (");
      print(chunk->constants.items[idx]);
      printf(")");
    }
    printf("\n");
    return offset + bytes_read;
  }
  case OP_JUMP: {
    Z bytes_read;
    I ofs = decode_sleb128(&chunk->items[offset], &bytes_read);
    printf("JUMP %ld -> %zu\n", ofs, offset + bytes_read + ofs);
    return offset + bytes_read;
  }
  case OP_JUMP_IF_NIL: {
    Z bytes_read;
    I ofs = decode_sleb128(&chunk->items[offset], &bytes_read);
    printf("JUMP_IF_NIL %ld -> %zu\n", ofs, offset + bytes_read + ofs);
    return offset + bytes_read;
  }
  case OP_CALL: {
    Z bytes_read;
    I ofs = decode_sleb128(&chunk->items[offset], &bytes_read);
    printf("CALL %ld\n", ofs);
    return offset + bytes_read;
  }
  case OP_APPLY:
    printf("APPLY\n");
    return offset;
  case OP_RETURN:
    printf("RETURN\n");
    return offset;
  case OP_ADD:
    printf("ADD\n");
    return offset;
  default:
    printf("? (%d)\n", opcode);
    return offset;
  }
}
