#ifndef LEXER_H
#define LEXER_H

#include "common.h"
#include "stream.h"

enum {
  TOK_INVALID = -1,
  TOK_EOF = 0,
  TOK_WORD = 'a',
  TOK_STRING = '"',
  TOK_SEMICOLON = ';',
  TOK_LPAREN = '(',
  TOK_RPAREN = ')',
  TOK_LBRACKET = '[',
  TOK_RBRACKET = ']',
  TOK_LBRACE = '{',
  TOK_RBRACE = '}',
  TOK_COMMENT = '\\',
};

typedef struct Lx {
  I kind;
  I cursor;
  I curr_line, curr_col;
  I start_line, start_col;
  Stream *stream;
  char *items;
  Z count, capacity;
} Lx;

Lx *lexer_make(Stream *);
V lexer_free(Lx *lx);
I lexer_next(Lx *);

#endif
