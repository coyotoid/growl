#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <utf.h>

#include "lexer.h"
#include "vendor/yar.h"

static inline int is_delimiter(int i) {
  return i == '(' || i == ')' || i == '[' || i == ']' || i == '{' || i == '}' ||
         i == ';' || i == '\\' || i == '"';
}

static inline void appendrune(Lx *lx, Rune rn) {
  char data[5];
  I len = runetochar(data, &rn);
  yar_append_many(lx, data, len);
}

static inline void appendbyte(Lx *lx, char byte) { *yar_append(lx) = byte; }

static int getc_ws(Lx *lx) {
  if (ST_EOF(lx->stream))
    return -1;
  for (;;) {
    int ch = ST_GETC(lx->stream);
    if (isspace(ch))
      continue;
    return ch;
  }
}

static int scanword(Lx *lx) {
  int next = ST_GETC(lx->stream);

  for (;;) {
    if (next == -1) {
      if (lx->cursor == 0)
        lx->kind = TOK_EOF;
      appendbyte(lx, 0);
      return lx->kind;
    } else if (is_delimiter(next) || isspace(next)) {
      ST_UNGETC(next, lx->stream);
      appendbyte(lx, 0);
      return lx->kind;
    } else {
      appendbyte(lx, next);
      next = ST_GETC(lx->stream);
      continue;
    }
  }
}

static void scanescape(Lx *lx) {
  char escbuf[7], *escptr = escbuf;
  int next;
  Rune tmp;

  for (;;) {
    next = ST_GETC(lx->stream);

    if (next == -1) {
      errx(1, "unterminated hex sequence '%s'", escbuf);
    } else if (next == ';') {
      *escptr = 0;
      break;
    } else if (!isxdigit(next)) {
      errx(1, "invalid hex digit '%c'", next);
    }

    if (escptr - escbuf >= 6) {
      errx(1, "hex sequence too long (6 chars max.)");
    } else {
      *(escptr++) = next;
    }
  }

  tmp = strtol(escbuf, &escptr, 16);
  if (*escptr == '\0')
    appendrune(lx, tmp);
  else
    errx(1, "invalid hex sequence '%s'", escbuf);
}

static int scanstring(Lx *lx) {
  int next;

  for (;;) {
    next = ST_GETC(lx->stream);
    switch (next) {
    case -1:
      goto eof;
    case '\\':
      next = ST_GETC(lx->stream);
      if (next == -1)
        goto eof;
      switch (next) {
      case 't':
        appendbyte(lx, '\t');
        break;
      case 'n':
        appendbyte(lx, '\n');
        break;
      case 'r':
        appendbyte(lx, '\r');
        break;
      case 'b':
        appendbyte(lx, '\b');
        break;
      case 'v':
        appendbyte(lx, '\v');
        break;
      case 'f':
        appendbyte(lx, '\f');
        break;
      case '0':
        appendbyte(lx, '\0');
        break;
      case 'e':
        appendbyte(lx, '\x1b');
        break;
      case '\\':
      case '"':
        appendbyte(lx, next);
        break;
      case 'x':
        scanescape(lx);
        break;
      default:
        fprintf(stderr, "unknown escape sequence '\\%c'\n", next);
        abort();
      }
      break;
    case '"':
      appendbyte(lx, 0);
      return (lx->kind = TOK_STRING);
    default:
      appendbyte(lx, next);
    }
  }

eof:
  errx(1, "unterminated string literal");
  return 0;
}

I lexer_next(Lx *lx) {
  int next;
  lx->cursor = 0;

  if (ST_EOF(lx->stream)) {
    lx->kind = TOK_EOF;
    return 0;
  }

  next = getc_ws(lx);

  switch (next) {
  case '\\':
    for (; next != '\n'; next = ST_GETC(lx->stream))
      ;
    return lexer_next(lx);
  case '(':
  case ')':
  case '[':
  case ']':
  case '{':
  case '}':
  case ';':
    return (lx->kind = next);
  case '"':
    return scanstring(lx);
  default:
    ST_UNGETC(next, lx->stream);
    lx->kind = TOK_WORD;
    return scanword(lx);
  };
}
