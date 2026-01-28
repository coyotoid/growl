#ifndef PARSER_H
#define PARSER_H

#include "common.h"
#include "lexer.h"
#include "vendor/yar.h"

enum {
  AST_PROGRAM,
  AST_INT,
  AST_STR,
  AST_WORD,
  AST_LIST,
  AST_TABLE,
  AST_QUOTE,
  AST_DEF,
  AST_CMD,
  AST_PRAGMA,
};

typedef struct Ast {
  I type;
  char *name;
  I int_val;
  struct {
    struct Ast **items;
    Z count, capacity;
  } children;
  I line, col;
} Ast;

Ast *parser_parse(Lx *lx);
void ast_free(Ast *ast);

#endif
