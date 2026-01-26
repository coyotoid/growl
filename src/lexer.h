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
  Stream *stream;
  char *items;
  Z count, capacity;
} Lx;

Lx *lexer_make(Stream *);
I lexer_next(Lx *);

#endif
