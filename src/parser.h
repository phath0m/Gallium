#ifndef _PARSER_H
#define _PARSER_H

#include <gallium/list.h>
#include "ast.h"
#include "lexer.h"

#define PARSER_EXPECTED_TOK         1   /* an expected token is missing */
#define PARSER_EXPECTED_TOK_KIND    2   /* an expended TYPE of token is missing */
#define PARSER_UNEXPECTED_TOK       3   /* an unexpected token was encountered */
#define PARSER_EXPECTED_EXPR        4   /* an expression was expected */
#define PARSER_LEXER_ERR            5   /* the error was during scanning, check lexer state */
#define PARSER_UNEXPECTED_EOF       6   /* unexpected end of file */
#define PARSER_INTEGER_TOO_BIG      7   /* Integer falls outside of 64-bit range */
#define PARSER_VARARGS_MUST_BE_LAST 8

typedef struct Ga_Object GaObject;

struct parser_state {
    _Ga_iter_t          iter;
    struct lexer_state  lex_state;
    struct token    *   last_tok;
    GaObject        *   error;
};


bool                GaParser_AcceptTokClass(struct parser_state *, token_class_t);
bool                GaParser_AcceptTokVal(struct parser_state *, token_class_t, const char *);

struct token    *   GaParser_PeekTok(struct parser_state *);
struct token    *   GaParser_ReadTok(struct parser_state *);

void                parser_init(struct parser_state *);
void                GaParser_InitLazy(struct parser_state *, _Ga_list_t *);
struct ast_node *   GaParser_ParseString(struct parser_state *, const char *);
void                GaParser_Fini(struct parser_state *);

struct ast_node *   GaParser_ParseAll(struct parser_state *);
struct ast_node *   _GaParser_ParseDecl(struct parser_state *);
struct ast_node *   _GaParser_ParseExpr(struct parser_state *);
struct ast_node *   _GaParser_ParseStmt(struct parser_state *);

#endif
