#include <ctype.h>
#include <err.h>
#include <stdlib.h>
#include <utf.h>

#include "lexer.h"
#include "vendor/yar.h"

Lx *lexer_make(Stream *s) {
  Lx *lx = calloc(1, sizeof(Lx));
  lx->stream = s;
  return lx;
}

V lexer_free(Lx *lx) {
  yar_free(lx);
  free(lx);
}

static int lx_getc(Lx *lx) {
  int c = ST_GETC(lx->stream);
  if (c == '\n') {
    lx->curr_line++;
    lx->curr_col = 0;
  } else if (c != -1) {
    lx->curr_col++;
  }
  return c;
}

static void lx_ungetc(Lx *lx, int c) {
  ST_UNGETC(c, lx->stream);
  if (c == '\n') {
    lx->curr_line--;
  } else if (c != -1) {
    lx->curr_col--;
  }
}

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
    int ch = lx_getc(lx);
    if (isspace(ch))
      continue;
    return ch;
  }
}

static int scanword(Lx *lx) {
  int next = lx_getc(lx);

  for (;;) {
    if (next == -1) {
      if (lx->count == 0)
        lx->kind = TOK_EOF;
      appendbyte(lx, 0);
      return lx->kind;
    } else if (is_delimiter(next) || isspace(next)) {
      lx_ungetc(lx, next);
      appendbyte(lx, 0);
      return lx->kind;
    } else {
      appendbyte(lx, next);
      next = lx_getc(lx);
      continue;
    }
  }
}

static void scanescape(Lx *lx) {
  char escbuf[7], *escptr = escbuf;
  int next;
  Rune tmp;

  for (;;) {
    next = lx_getc(lx);

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
  if (*escptr == '\0') {
    if (tmp < 256) {
      appendbyte(lx, (U8)(tmp & 255));
    } else {
      appendrune(lx, tmp);
    }

  } else {
    errx(1, "invalid hex sequence '%s'", escbuf);
  }
}

static int scanstring(Lx *lx) {
  int next;

  for (;;) {
    next = lx_getc(lx);
    switch (next) {
    case -1:
      goto eof;
    case '\\':
      next = lx_getc(lx);
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
        return (lx->kind = TOK_INVALID);
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
  return (lx->kind = TOK_INVALID);
}

I lexer_next(Lx *lx) {
  int next;
  lx->cursor = 0;
  lx->count = 0;

  if (ST_EOF(lx->stream)) {
    lx->kind = TOK_EOF;
    return 0;
  }

  next = getc_ws(lx);

  lx->start_line = lx->curr_line;
  lx->start_col = (lx->curr_col > 0) ? lx->curr_col - 1 : 0;

  switch (next) {
  case '\\':
    for (; next != '\n'; next = lx_getc(lx))
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
    lx_ungetc(lx, next);
    lx->kind = TOK_WORD;
    return scanword(lx);
  };
}
