#include <stdio.h>

#include "chunk.h"
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

static Z dis_instr(Bc *chunk, Z offset, Dt **dictionary, I indent);

static V dis(Bc *chunk, Dt **dictionary, I indent) {
  Z offset = 0;
  while (offset < chunk->count)
    offset = dis_instr(chunk, offset, dictionary, indent);
}

V disassemble(Bc *chunk, const char *name, Dt **dictionary) {
  printf("=== %s ===\n", name);
  dis(chunk, dictionary, 0);
}

static Z dis_instr(Bc *chunk, Z offset, Dt **dictionary, I indent) {
  for (I i = 0; i < indent * 2; i++)
    putchar(' ');
  fflush(stdout);
  printf("%04zu ", offset);
  U8 opcode = chunk->items[offset++];

#define CASE(name) case OP_##name:
#define SIMPLE(name)                                                           \
  case OP_##name:                                                              \
    printf(#name "\n");                                                        \
    return offset;

  switch (opcode) {
    SIMPLE(NOP);
    SIMPLE(NIL);
    CASE(CONST) {
      Z bytes_read;
      I idx = decode_sleb128(&chunk->items[offset], &bytes_read);
      printf("CONST %ld", idx);
      if (idx >= 0 && idx < (I)chunk->constants.count) {
        O obj = chunk->constants.items[idx];
        printf(" (");
        print(obj);
        printf(")");

        if (!IMM(obj) && obj != NIL && type(obj) == TYPE_QUOT) {
          putchar('\n');
          Hd *hdr = UNBOX(obj);
          Bc **chunk_ptr = (Bc **)(hdr + 1);
          Bc *quot_chunk = *chunk_ptr;
          dis(quot_chunk, dictionary, indent + 1);
          return offset + bytes_read;
        }
      }
      printf("\n");
      return offset + bytes_read;
    }
    SIMPLE(DROP);
    SIMPLE(DUP);
    SIMPLE(SWAP);
    SIMPLE(NIP);
    SIMPLE(OVER);
    SIMPLE(BURY);
    SIMPLE(DIG);
    SIMPLE(TOR);
    SIMPLE(FROMR);
    CASE(JUMP) {
      Z bytes_read;
      I ofs = decode_sleb128(&chunk->items[offset], &bytes_read);
      printf("JUMP %ld -> %zu\n", ofs, offset + bytes_read + ofs);
      return offset + bytes_read;
    }
    CASE(JUMP_IF_NIL) {
      Z bytes_read;
      I ofs = decode_sleb128(&chunk->items[offset], &bytes_read);
      printf("JUMP_IF_NIL %ld -> %zu\n", ofs, offset + bytes_read + ofs);
      return offset + bytes_read;
    }
    CASE(CALL) {
      Z bytes_read;
      I ofs = decode_sleb128(&chunk->items[offset], &bytes_read);
      printf("CALL %ld\n", ofs);
      return offset + bytes_read;
    }
    CASE(DOWORD) {
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
    SIMPLE(APPLY);
    CASE(TAIL_CALL) {
      Z bytes_read;
      I ofs = decode_sleb128(&chunk->items[offset], &bytes_read);
      printf("TAIL_CALL %ld\n", ofs);
      return offset + bytes_read;
    }
    CASE(TAIL_DOWORD) {
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
    SIMPLE(TAIL_APPLY);
    SIMPLE(RETURN);
    SIMPLE(CHOOSE);
    SIMPLE(ADD);
    SIMPLE(SUB);
    SIMPLE(MUL);
    SIMPLE(DIV);
    SIMPLE(MOD);
    SIMPLE(EQ);
    SIMPLE(NEQ);
    SIMPLE(LT);
    SIMPLE(GT);
    SIMPLE(LTE);
    SIMPLE(GTE);
  default:
    printf("??? (%d)\n", opcode);
    return offset;
  }

#undef SIMPLE
#undef CASE
}
