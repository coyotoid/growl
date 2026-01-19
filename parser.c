#include "parser.h"
#include "vendor/mpc.h"

mpc_parser_t *Pragma, *Comment, *Expr, *Number, *String, *Word, *Definition,
    *Command, *List, *Table, *Quotation, *Program;

V parser_init(V) {
  Pragma = mpc_new("pragma");
  Comment = mpc_new("comment");
  Expr = mpc_new("expr");
  Number = mpc_new("number");
  String = mpc_new("string");
  Word = mpc_new("word");
  Definition = mpc_new("def");
  Command = mpc_new("command");
  List = mpc_new("list");
  Table = mpc_new("table");
  Quotation = mpc_new("quotation");
  Program = mpc_new("program");

  mpc_err_t *err = mpca_lang(
      MPCA_LANG_DEFAULT,
      " pragma     : '#' <word> ('(' <expr>* ')')?                  ; "
      " comment    : /\\\\[^\\n]*/                                  ; "
      " expr       : ( <pragma> | <def> | <command> | <quotation>     "
      "              | <number> | <list> | <table> | <string>         "
      "              | <word> | <comment> )                         ; "
      " number     : ( /0x[0-9A-Fa-f]+/ | /-?[0-9]+/ )              ; "
      " string     : /\"(\\\\.|[^\"])*\"/                           ; "
      " word       : /[a-zA-Z0-9_!.,@#$%^&*_+\\-=><|\\/]+/          ; "
      " def        : ':' <word> <expr>* ';'                         ; "
      " command    : <word> ':' <expr>+ ';'                         ; "
      " list       : '(' <expr>* ')'                                ; "
      " table      : '{' <expr>* '}'                                ; "
      " quotation  : '[' <expr>* ']'                                ; "
      " program    : /^/ <expr>* /$/                                ; ",
      Pragma, Comment, Expr, Number, String, Word, Definition, Command, List,
      Table, Quotation, Program, NULL);

  // crash if i do a woopsie
  if (err != NULL) {
    mpc_err_print(err);
    mpc_err_delete(err);
    abort();
  }
}

V parser_deinit(V) {
  mpc_cleanup(12, Pragma, Comment, Expr, Number, String, Word, Definition,
              Command, List, Table, Quotation, Program);
}
