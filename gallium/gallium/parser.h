#ifndef _PARSER_PARSER_H
#define _PARSER_PARSER_H

#include <gallium/ast.h>
#include <gallium/lexer.h>
#include <gallium/list.h>

#define PARSER_EXPECTED_TOK         1   /* an expected token is missing */
#define PARSER_EXPECTED_TOK_KIND    2   /* an expended TYPE of token is missing */
#define PARSER_UNEXPECTED_TOK       3   /* an unexpected token was encountered */
#define PARSER_EXPECTED_EXPR        4   /* an expression was expected */
#define PARSER_LEXER_ERR            5   /* the error was during scanning, check lexer state */
#define PARSER_UNEXPECTED_EOF       6   /* unexpected end of file */
#define PARSER_INTEGER_TOO_BIG      7   /* Integer falls outside of 64-bit range */

struct parser_state {
    list_iter_t         iter;
    struct lexer_state  lex_state;
    struct token    *   last_tok;
    int                 parser_errno;
    const char *        err_info;
};


bool                parser_accept_tok_class(struct parser_state *, token_class_t);
bool                parser_accept_tok_val(struct parser_state *, token_class_t, const char *);

struct token    *   parser_peek_tok(struct parser_state *);
struct token    *   parser_read_tok(struct parser_state *);

void                parser_explain(struct parser_state *);
void                parser_init(struct parser_state *);
void                parser_init_lazy(struct parser_state *, struct list *);
struct ast_node *   parser_parse(struct parser_state *, const char *);
void                parser_fini(struct parser_state *);

struct ast_node *   parser_parse_all(struct parser_state *);
struct ast_node *   parser_parse_decl(struct parser_state *);
struct ast_node *   parser_parse_expr(struct parser_state *);
struct ast_node *   parser_parse_stmt(struct parser_state *);

#endif
