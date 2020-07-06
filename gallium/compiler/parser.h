#ifndef _PARSER_PARSER_H
#define _PARSER_PARSER_H

#include <compiler/ast.h>
#include <compiler/lexer.h>
#include <gallium/list.h>

#define PARSER_EXPECTED_TOK     1   /* an expected token is missing */
#define PARSER_UNEXPECTED_TOK   2   /* an unexpected token was encountered */
#define PARSER_EXPECTED_EXPR    3   /* an expression was expected */
#define PARSER_LEXER_ERR        4   /* the error was during scanning, check lexer state */
#define PARSER_UNEXPECTED_EOF   5   /* unexpected end of file */

struct parser_state {
    list_iter_t         iter;
    struct lexer_state  lex_state;
    struct token    *   last_tok;
    int                 parser_errno;
    const char *        err_info;
};

void                parser_explain(struct parser_state *);
void                parser_init(struct parser_state *);
struct ast_node *   parser_parse(struct parser_state *, const char *);
void                parser_fini(struct parser_state *);
#endif
