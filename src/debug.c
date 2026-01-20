#include <stdio.h>

#include "debug.h"
#include "dictionary.h"
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

V disassemble(Bc *chunk, const char *name, Dt **dictionary) {
  printf("=== %s ===\n", name);
  Z offset = 0;
  while (offset < chunk->count) {
    offset = disassemble_instruction(chunk, offset, dictionary);
  }
}

Z disassemble_instruction(Bc *chunk, Z offset, Dt **dictionary) {
  printf("%04zu ", offset);
  U8 opcode = chunk->items[offset++];
  switch (opcode) {
  case OP_NOP:
    printf("NOP\n");
    return offset;
  case OP_NIL:
    printf("NIL\n");
    return offset;
  case OP_CONST: {
    Z bytes_read;
    I idx = decode_sleb128(&chunk->items[offset], &bytes_read);
    printf("CONST %ld", idx);
    if (idx >= 0 && idx < (I)chunk->constants.count) {
      O obj = chunk->constants.items[idx];
      printf(" (");
      print(obj);
      printf(")");

      // If it's a quotation, disassemble it inline
      if (!IMM(obj) && obj != NIL && type(obj) == TYPE_QUOT) {
        Hd *hdr = UNBOX(obj);
        Bc **chunk_ptr = (Bc **)(hdr + 1);
        Bc *quot_chunk = *chunk_ptr;
        printf("\n");

        // Disassemble quotation with indentation
        for (Z i = 0; i < quot_chunk->count; ) {
          printf("  ");
          i = disassemble_instruction(quot_chunk, i, dictionary);
        }
        return offset + bytes_read;
      }
    }
    printf("\n");
    return offset + bytes_read;
  }
  case OP_DROP: {
    printf("DROP\n");
    return offset;
  }
  case OP_DUP: {
    printf("DUP\n");
    return offset;
  }
  case OP_SWAP: {
    printf("SWAP\n");
    return offset;
  }
  case OP_TOR:
    printf("TOR\n");
    return offset;
  case OP_FROMR:
    printf("FROMR\n");
    return offset;
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
  case OP_DOWORD: {
    Z bytes_read;
    I hash = decode_sleb128(&chunk->items[offset], &bytes_read);
    printf("DOWORD");

    if (dictionary && *dictionary) {
      Dt *entry = lookup_hash(dictionary, hash);
      if (entry != NULL) {
        printf(" %s", entry->name);
      } else {
        printf(" ???");
      }
    } else {
      printf(" 0x%lx", hash);
    }
    printf("\n");
    return offset + bytes_read;
  }
  case OP_APPLY:
    printf("APPLY\n");
    return offset;
  case OP_TAIL_CALL: {
    Z bytes_read;
    I ofs = decode_sleb128(&chunk->items[offset], &bytes_read);
    printf("TAIL_CALL %ld\n", ofs);
    return offset + bytes_read;
  }
  case OP_TAIL_DOWORD: {
    Z bytes_read;
    I hash = decode_sleb128(&chunk->items[offset], &bytes_read);
    printf("TAIL_DOWORD");

    if (dictionary && *dictionary) {
      Dt *entry = lookup_hash(dictionary, hash);
      if (entry != NULL) {
        printf(" %s", entry->name);
      } else {
        printf(" ???");
      }
    } else {
      printf(" 0x%lx", hash);
    }
    printf("\n");
    return offset + bytes_read;
  }
  case OP_TAIL_APPLY:
    printf("TAIL_APPLY\n");
    return offset;
  case OP_RETURN:
    printf("RETURN\n");
    return offset;
  case OP_CHOOSE:
    printf("CHOOSE\n");
    return offset;
  case OP_ADD:
    printf("ADD\n");
    return offset;
  case OP_SUB:
    printf("SUB\n");
    return offset;
  case OP_MUL:
    printf("MUL\n");
    return offset;
  case OP_DIV:
    printf("DIV\n");
    return offset;
  case OP_MOD:
    printf("MOD\n");
    return offset;
  case OP_EQ:
    printf("EQ\n");
    return offset;
  case OP_NEQ:
    printf("NEQ\n");
    return offset;
  case OP_LT:
    printf("LT\n");
    return offset;
  case OP_GT:
    printf("GT\n");
    return offset;
  case OP_LTE:
    printf("LTE\n");
    return offset;
  case OP_GTE:
    printf("GTE\n");
    return offset;
  default:
    printf("? (%d)\n", opcode);
    return offset;
  }
}
