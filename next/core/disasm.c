#include "opcodes.h"
#include "sleb128.h"
#include <growl.h>

static void disassemble(GrowlVM *vm, GrowlQuotation *quot, int indent);

static size_t disassemble_instr(GrowlVM *vm, GrowlQuotation *quot,
                                size_t offset, int indent) {
  for (int i = 0; i < indent; i++) {
    printf("  ");
  }
  printf("%04zu ", offset);

  uint8_t opcode = quot->data[offset++];

  // clang-format off
#define OPCODE(name) case GOP_## name:
#define OPCODE1(name) case GOP_## name: printf(#name "\n"); return offset;
  // clang-format on

  switch (opcode) {
    OPCODE1(NOP);
    OPCODE1(PUSH_NIL);
    OPCODE(PUSH_CONSTANT) {
      intptr_t idx;
      size_t bytes_read = growl_sleb128_peek(&quot->data[offset], &idx);
      printf("PUSH_CONSTANT %ld", idx);
      if (quot->constants != GROWL_NIL &&
          growl_type(quot->constants) == GROWL_TYPE_TUPLE) {
        GrowlTuple *constants = growl_unwrap_tuple(quot->constants);
        if (idx >= 0 && (size_t)idx < constants->count) {
          Growl constant = constants->data[idx];
          printf(" (");
          growl_print(constant);
          printf(")");

          if (!GROWL_IMM(constant) && constant != GROWL_NIL &&
              growl_type(constant) == GROWL_TYPE_QUOTATION) {
            putchar('\n');
            GrowlQuotation *inner = growl_unwrap_quotation(constant);
            disassemble(vm, inner, indent + 1);
            return offset + bytes_read;
          }
        }
      }
      putchar('\n');
      return offset + bytes_read;
    }
    OPCODE1(DROP);
    OPCODE1(DUP);
    OPCODE1(SWAP);
    OPCODE1(2DROP);
    OPCODE1(2DUP);
    OPCODE1(2SWAP);
    OPCODE1(NIP);
    OPCODE1(OVER);
    OPCODE1(BURY);
    OPCODE1(DIG);
    OPCODE1(TO_RETAIN);
    OPCODE1(FROM_RETAIN);
    OPCODE1(CALL);
    OPCODE1(CALL_NEXT);
    OPCODE1(TAIL_CALL);
    OPCODE(WORD) {
      intptr_t idx;
      size_t bytes_read = growl_sleb128_peek(&quot->data[offset], &idx);
      printf("WORD %s\n", vm->defs.data[idx].name);
      return offset + bytes_read;
    }
    OPCODE(TAIL_WORD) {
      intptr_t idx;
      size_t bytes_read = growl_sleb128_peek(&quot->data[offset], &idx);
      printf("TAIL_WORD %s\n", vm->defs.data[idx].name);
      return offset + bytes_read;
    }
    OPCODE1(RETURN);
    OPCODE1(COMPOSE);
    OPCODE1(CURRY);
    OPCODE1(PPRINT);
    OPCODE1(ADD);
    OPCODE1(MUL);
    OPCODE1(SUB);
    OPCODE1(DIV);
    OPCODE1(MOD);
    OPCODE1(BAND);
    OPCODE1(BOR);
    OPCODE1(BXOR);
    OPCODE1(BNOT);
    OPCODE1(EQ);
    OPCODE1(NEQ);
    OPCODE1(LT);
    OPCODE1(LTE);
    OPCODE1(GT);
    OPCODE1(GTE);
  default:
    printf("%d\n", opcode);
    return offset;
  }
}

static void disassemble(GrowlVM *vm, GrowlQuotation *quot, int indent) {
  size_t offset = 0;
  while (offset < quot->count)
    offset = disassemble_instr(vm, quot, offset, indent);
}

void growl_disassemble(GrowlVM *vm, GrowlQuotation *quot) {
  disassemble(vm, quot, 0);
}
