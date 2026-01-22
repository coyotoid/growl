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
  for (I i = 0; i < indent; i++)
    printf("  ");
  printf("%04zu ", offset);

  I col = -1;
  I line = chunk_get_line(chunk, offset, &col);
  if (line >= 0) {
    printf("%4ld:%-3ld ", line + 1, col + 1);
  } else {
    printf("        ");
  }

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
    CASE(DOWORD) {
      Z bytes_read;
      I idx = decode_sleb128(&chunk->items[offset], &bytes_read);
      Dt *word = chunk->symbols.items[idx].resolved;
      printf("DOWORD %s\n", word->name);
      return offset + bytes_read;
    }
    SIMPLE(CALL);
    CASE(TAIL_DOWORD) {
      Z bytes_read;
      I idx = decode_sleb128(&chunk->items[offset], &bytes_read);
      Dt *word = chunk->symbols.items[idx].resolved;
      printf("TAIL_DOWORD %s\n", word->name);
      return offset + bytes_read;
    }
    SIMPLE(TAIL_CALL);
    SIMPLE(RETURN);
    SIMPLE(CHOOSE);
    SIMPLE(ADD);
    SIMPLE(SUB);
    SIMPLE(MUL);
    SIMPLE(DIV);
    SIMPLE(MOD);
    SIMPLE(LOGAND);
    SIMPLE(LOGOR);
    SIMPLE(LOGXOR);
    SIMPLE(LOGNOT);
    SIMPLE(EQ);
    SIMPLE(NEQ);
    SIMPLE(LT);
    SIMPLE(GT);
    SIMPLE(LTE);
    SIMPLE(GTE);
    SIMPLE(AND);
    SIMPLE(OR);
    SIMPLE(TYPE);
    SIMPLE(CONCAT);
    SIMPLE(PPRINT);
    SIMPLE(PRINTSTACK);
  default:
    printf("??? (%d)\n", opcode);
    return offset;
  }

#undef SIMPLE
#undef CASE
}
