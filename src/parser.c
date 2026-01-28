#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Ast *ast_new(I type, I line, I col) {
  Ast *node = calloc(1, sizeof(Ast));
  node->type = type;
  node->line = line;
  node->col = col;
  return node;
}

void ast_free(Ast *ast) {
  if (!ast)
    return;
  if (ast->name)
    free(ast->name);
  for (size_t i = 0; i < ast->children.count; i++) {
    ast_free(ast->children.items[i]);
  }
  yar_free(&ast->children);
  free(ast);
}

static Ast *parse_expr_at(Lx *lx);

static void parse_block(Lx *lx, Ast *parent, int close_token) {
  while (1) {
    if (lx->kind == TOK_EOF) {
      if (close_token != TOK_EOF)
        fprintf(stderr, "syntax error: unexpected EOF, expected '%c'\n",
                close_token);
      break;
    }
    if (lx->kind == close_token) {
      lexer_next(lx);
      break;
    }
    Ast *expr = parse_expr_at(lx);
    *yar_append(&parent->children) = expr;
  }
}

static Ast *parse_expr_at(Lx *lx) {
  int kind = lx->kind;
  I line = lx->start_line;
  I col = lx->start_col;

  if (kind == TOK_WORD) {
    char *text = lx->items;

    if (strcmp(text, "def") == 0) {
      Ast *node = ast_new(AST_DEF, line, col);
      lexer_next(lx);

      if (lx->kind != TOK_WORD) {
        fprintf(stderr, "syntax error: expected word after 'def' at %ld:%ld\n",
                (long)line + 1, (long)col + 1);
        return node;
      }
      node->name = strdup(lx->items);
      lexer_next(lx);

      if (lx->kind != '{') {
        fprintf(stderr,
                "syntax error: expected '{' after def name at %ld:%ld\n",
                (long)lx->start_line + 1, (long)lx->start_col + 1);
        return node;
      }
      lexer_next(lx);
      parse_block(lx, node, '}');
      return node;
    }

    size_t len = strlen(text);
    if (len > 0 && text[len - 1] == ':') {
      Ast *node = ast_new(AST_CMD, line, col);
      node->name = strndup(text, len - 1);
      lexer_next(lx);
      parse_block(lx, node, ';');
      return node;
    }

    if (text[0] == '#') {
      Ast *node = ast_new(AST_PRAGMA, line, col);
      node->name = strdup(text);
      lexer_next(lx);
      if (lx->kind == '(') {
        lexer_next(lx);
        parse_block(lx, node, ')');
      }
      return node;
    }

    char *end;
    long val = strtol(text, &end, 0);
    if (*end == '\0') {
      Ast *node = ast_new(AST_INT, line, col);
      node->int_val = val;
      lexer_next(lx);
      return node;
    }

    Ast *node = ast_new(AST_WORD, line, col);
    node->name = strdup(text);
    lexer_next(lx);
    return node;
  }

  if (kind == TOK_STRING) {
    Ast *node = ast_new(AST_STR, line, col);
    node->name = strdup(lx->items);
    lexer_next(lx);
    return node;
  }

  if (kind == '[') {
    Ast *node = ast_new(AST_QUOTE, line, col);
    lexer_next(lx);
    parse_block(lx, node, ']');
    return node;
  }

  if (kind == '{') {
    Ast *node = ast_new(AST_TABLE, line, col);
    lexer_next(lx);
    parse_block(lx, node, '}');
    return node;
  }

  if (kind == '(') {
    Ast *node = ast_new(AST_LIST, line, col);
    lexer_next(lx);
    parse_block(lx, node, ')');
    return node;
  }

  if (kind == TOK_INVALID) {
    fprintf(stderr, "syntax error: invalid token at %ld:%ld\n", (long)line + 1,
            (long)col + 1);
  } else {
    fprintf(stderr, "syntax error: unexpected token '%c' (%d) at %ld:%ld\n",
            kind, kind, (long)line + 1, (long)col + 1);
  }
  lexer_next(lx);

  return NULL;
}

Ast *parser_parse(Lx *lx) {
  Ast *root = ast_new(AST_PROGRAM, 0, 0);
  lexer_next(lx);
  parse_block(lx, root, TOK_EOF);
  return root;
}
