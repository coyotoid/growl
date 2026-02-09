#include <ctype.h>
#include <growl.h>
#include <stdlib.h>

static int lexer_getc(GrowlLexer *lx) {
  int c = getc(lx->file);
  if (c == '\n') {
    lx->current_row++;
    lx->current_col = 0;
  } else if (c != EOF) {
    lx->current_col++;
  }
  return c;
}

static void lexer_ungetc(GrowlLexer *lx, int c) {
  ungetc(c, lx->file);
  if (c == '\n') {
    lx->current_row--;
  } else if (c != EOF) {
    lx->current_col--;
  }
}

static int getc_ws(GrowlLexer *lx) {
  if (feof(lx->file))
    return EOF;
  for (;;) {
    int ch = lexer_getc(lx);
    if (isspace(ch))
      continue;
    return ch;
  }
}

static int is_delimiter(int i) {
  return i == '(' || i == ')' || i == '[' || i == ']' || i == '{' || i == '}' ||
         i == ';' || i == '\\' || i == '"';
}

static void append(GrowlLexer *lexer, int ch) {
  if (lexer->cursor >= GROWL_LEXER_BUFSIZE) {
    fprintf(stderr, "lexer: buffer overflow\n");
    abort();
  }
  lexer->buffer[lexer->cursor++] = (char)(ch & 0xff);
}

static int scan_word(GrowlLexer *lx) {
  int next = lexer_getc(lx);
  for (;;) {
    if (next == -1) {
      if (lx->cursor == 0)
        lx->kind = GTOK_EOF;
      append(lx, 0);
      return lx->kind;
    }
    if (is_delimiter(next) || isspace(next)) {
      lexer_ungetc(lx, next);
      append(lx, 0);
      return lx->kind;
    }
    append(lx, next);
    next = lexer_getc(lx);
  }
}

static int scan_string(GrowlLexer *lexer) {
  int next;
  for (;;) {
    next = lexer_getc(lexer);
    switch (next) {
    case EOF:
      goto eof;
    case '\\':
      // TODO: \x escape sequences
      next = lexer_getc(lexer);
      if (next == -1)
        goto eof;
      switch (next) {
      case 't':
        append(lexer, '\t');
        break;
      case 'n':
        append(lexer, '\n');
        break;
      case 'r':
        append(lexer, '\r');
        break;
      case 'b':
        append(lexer, '\b');
        break;
      case 'v':
        append(lexer, '\v');
        break;
      case 'f':
        append(lexer, '\f');
        break;
      case '0':
        append(lexer, '\0');
        break;
      case 'e':
        append(lexer, '\x1b');
        break;
      case '\\':
      case '"':
        append(lexer, next);
        break;
      default:
        return lexer->kind = GTOK_INVALID;
      }
      break;
    case '"':
      append(lexer, 0);
      return lexer->kind = GTOK_STRING;
    default:
      append(lexer, next);
    }
  }

eof:
  return lexer->kind = GTOK_INVALID;
}

int growl_lexer_next(GrowlLexer *lexer) {
  lexer->cursor = 0;
  if (feof(lexer->file)) {
    return lexer->kind = GTOK_EOF;
  }

  int next = getc_ws(lexer);
  lexer->start_row = lexer->current_row;
  lexer->start_col = lexer->current_col ? lexer->current_col - 1 : 0;

  switch (next) {
  case '\\':
    for (; next != '\n'; next = lexer_getc(lexer))
      ;
    return growl_lexer_next(lexer);
  case '(':
  case ')':
  case '[':
  case ']':
  case '{':
  case '}':
  case ';':
    return lexer->kind = next;
  case '"':
    return scan_string(lexer);
  default:
    lexer_ungetc(lexer, next);
    lexer->kind = GTOK_WORD;
    return scan_word(lexer);
  }
}
