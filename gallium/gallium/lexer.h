#ifndef _PARSER_LEXER_H
#define _PARSER_LEXER_H

#include <gallium/list.h>
#include <gallium/stringbuf.h>

#define LEXER_EOF               1
#define LEXER_UKN_CHAR          2
#define LEXER_UNTERMINATED_STR  3

typedef enum {
    TOK_KEYWORD,
    TOK_IDENT,
    TOK_AND,
    TOK_OR,
    TOK_XOR,
    TOK_ADD,
    TOK_SUB,
    TOK_MUL,
    TOK_DIV,
    TOK_MOD,
    TOK_ASSIGN,
    TOK_EQUALS,
    TOK_NOT_EQUALS,
    TOK_GREATER_THAN,
    TOK_LESS_THAN,
    TOK_GREATER_THAN_OR_EQU,
    TOK_LESS_THAN_OR_EQU,
    TOK_LOGICAL_AND,
    TOK_LOGICAL_OR,
    TOK_LOGICAL_NOT,
    TOK_NOT,
    TOK_LEFT_PAREN,
    TOK_RIGHT_PAREN,
    TOK_OPEN_BRACE,
    TOK_CLOSE_BRACE,
    TOK_OPEN_BRACKET,
    TOK_CLOSE_BRACKET,
    TOK_SEMICOLON,
    TOK_COLON,
    TOK_DOT,
    TOK_COMMA,
    TOK_UNARYOP,
    TOK_STRING_LIT,
    TOK_INT_LIT,
    TOK_FLOAT_LIT,
    TOK_PHAT_ARROW,
    TOK_CLOSED_RANGE,
    TOK_HALF_RANGE,
    TOK_SHL,
    TOK_SHR,
    TOK_BACKTICK,
    TOK_INPLACE_ADD,
    TOK_INPLACE_SUB,
    TOK_INPLACE_MUL,
    TOK_INPLACE_DIV,
    TOK_INPLACE_MOD,
    TOK_INPLACE_AND,
    TOK_INPLACE_XOR,
    TOK_INPLACE_OR,
    TOK_INPLACE_SHL,
    TOK_INPLACE_SHR,
    TOK_THICC_COLON
} token_class_t;

struct token {
    token_class_t           type;
    struct stringbuf    *   sb;
    int                     col;
    int                     row;
};

struct lexer_state {
    int             lex_errno;
    char    *       text;
    int             position;
    int             col;
    int             row;
    int             token_col;
    int             token_row;
    size_t          text_len;
    struct list *   tokens;
};

void    _GaLexer_Init(struct lexer_state *);
void    _GaLexer_Fini(struct lexer_state *);
void    _GaLexer_ScanStr(struct lexer_state *, const char *);

#endif
