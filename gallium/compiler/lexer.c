#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <compiler/lexer.h>
#include <gallium/list.h>
#include <gallium/stringbuf.h>

static char *gallium_keywords[] = {
    "if", "while", "for", "func", "use", "lambda", "class",
    "raise", "continue", "break", "return", "else", "try", 
    "except", "extends", "in", "true", "false", "macro",
    NULL
};

static int
peek_char(struct lexer_state *statep, int lookahead)
{
    if (statep->position + lookahead >= statep->text_len) {
        return -1;
    }

    return statep->text[statep->position + lookahead];
}

static int
read_char(struct lexer_state *statep)
{
    if (statep->position >= statep->text_len) {
        return -1;
    }

    int ret = statep->text[statep->position++];

    if (ret == '\n') {
        statep->col = 0;
        statep->row++;
    } else {
        statep->col++;
    }

    return ret;
}

static int
read_chars(struct lexer_state *statep, int amount)
{
    int nread = 0;

    while (nread < amount) {
        int ch = read_char(statep);

        if (ch == -1) break;

        nread++;
    }

    return nread;
}

static bool
match_str(struct lexer_state *statep, const char *str)
{
    size_t str_len = strlen(str);

    for (int i = 0; i < str_len; i++) {
        if (peek_char(statep, i) != str[i]) {
            return false;
        }
    }

    return true;
}

static void
mark_token_start(struct lexer_state *statep)
{
    statep->token_col = statep->col;
    statep->token_row = statep->row;
}

static struct token *
token_new(struct lexer_state *statep, token_class_t type, struct stringbuf *sb)
{
    struct token *tok = calloc(sizeof(struct token), 1);
    tok->col = statep->token_col;
    tok->row = statep->token_row;
    tok->type = type;
    tok->sb = sb;
    return tok;
}

static void
scan_comment(struct lexer_state *statep)
{
    while (peek_char(statep, 0)) {
        int ch = read_char(statep);

        if (ch == '\n') break;
    }
}

static struct token *
scan_identifier(struct lexer_state *statep)
{
    int ch = 0;
    struct stringbuf *sb = stringbuf_new();

    do {
        stringbuf_append(sb, ((char[]){read_char(statep), 0}));
        ch = peek_char(statep, 0);
    } while (isalnum(ch) || isdigit(ch) || ch == '_');

    char **keywords = gallium_keywords;

    while (*keywords) {
        if(strcmp(*keywords, STRINGBUF_VALUE(sb)) == 0) {
            return token_new(statep, TOK_KEYWORD, sb);
        }
        
        keywords++;
    }

    return token_new(statep, TOK_IDENT, sb);
}

static struct token *
scan_number(struct lexer_state *statep)
{
    struct stringbuf *sb = stringbuf_new();

    mark_token_start(statep);

    while (isdigit(peek_char(statep, 0))) {
        stringbuf_append(sb, ((char[]){read_char(statep), 0}));
    }
    
    return token_new(statep, TOK_INT_LIT, sb);
}

static struct token *
scan_string(struct lexer_state *statep)
{
    int delim = read_char(statep);
    struct stringbuf *sb = stringbuf_new();

    while (peek_char(statep, 0) != delim) {
        int ch = read_char(statep);

        if (ch == -1) {
            statep->lex_errno = LEXER_UNTERMINATED_STR;
            return NULL;
        }
        
        stringbuf_append(sb, ((char[]){ch, 0}));
    }

    read_char(statep);

    return token_new(statep, TOK_STRING_LIT, sb);
}

static struct token *
next_token(struct lexer_state *statep)
{
    while (isspace(peek_char(statep, 0))) {
        read_char(statep);
    }

    int ch = peek_char(statep, 0);

    if (ch == -1) {
        statep->lex_errno = LEXER_EOF;
        return NULL;
    }

    mark_token_start(statep);

    if (ch == '#') {
        scan_comment(statep);
        return next_token(statep);
    }

    if (isalpha(ch)) {
        return scan_identifier(statep);
    }

    if (isdigit(ch)) {
        return scan_number(statep);
    }

    if (match_str(statep, "==")) {
        read_chars(statep, 2);
        return token_new(statep, TOK_EQUALS, NULL);
    }

    if (match_str(statep, "!=")) {
        read_chars(statep, 2);
        return token_new(statep, TOK_NOT_EQUALS, NULL);
    }

    if (match_str(statep, ">=")) {
        read_chars(statep, 2);
        return token_new(statep, TOK_GREATER_THAN_OR_EQU, NULL);
    }

    if (match_str(statep, "<=")) {
        read_chars(statep, 2);
        return token_new(statep, TOK_LESS_THAN_OR_EQU, NULL);
    }

    if (match_str(statep, "||")) {
        read_chars(statep, 2);
        return token_new(statep, TOK_LOGICAL_OR, NULL);
    }

    if (match_str(statep, "&&")) {
        read_chars(statep, 2);
        return token_new(statep, TOK_LOGICAL_AND, NULL);
    }

    if (match_str(statep, "=>")) {
        read_chars(statep, 2);
        return token_new(statep, TOK_PHAT_ARROW, NULL);
    }

    if (match_str(statep, "...")) {
        read_chars(statep, 3);
        return token_new(statep, TOK_CLOSED_RANGE, NULL);
    }

    if (match_str(statep, "..")) {
        read_chars(statep, 2);
        return token_new(statep, TOK_HALF_RANGE, NULL);
    }

    if (match_str(statep, "<<")) {
        read_chars(statep, 2);
        return token_new(statep, TOK_SHL, NULL);
    }

    if (match_str(statep, ">>")) {
        read_chars(statep, 2);
        return token_new(statep, TOK_SHR, NULL);
    }

    if (match_str(statep, "+=")) {
        read_chars(statep, 2);
        return token_new(statep, TOK_INPLACE_ADD, NULL);
    }

    if (match_str(statep, "-=")) {
        read_chars(statep, 2);
        return token_new(statep, TOK_INPLACE_SUB, NULL);
    }

    if (match_str(statep, "/=")) {
        read_chars(statep, 2);
        return token_new(statep, TOK_INPLACE_DIV, NULL);
    }

    if (match_str(statep, "*=")) {
        read_chars(statep, 2);
        return token_new(statep, TOK_INPLACE_MUL, NULL);
    }

    if (match_str(statep, "%=")) {
        read_chars(statep, 2);
        return token_new(statep, TOK_INPLACE_MOD, NULL);
    }

    if (match_str(statep, "^=")) {
        read_chars(statep, 2);
        return token_new(statep, TOK_INPLACE_XOR, NULL);
    }

    if (match_str(statep, "&=")) {
        read_chars(statep, 2);
        return token_new(statep, TOK_INPLACE_AND, NULL);
    }

    if (match_str(statep, "|=")) {
        read_chars(statep, 2);
        return token_new(statep, TOK_INPLACE_OR, NULL);
    }

    if (match_str(statep, "<<=")) {
        read_chars(statep, 2);
        return token_new(statep, TOK_INPLACE_SHL, NULL);
    }

    if (match_str(statep, ">>=")) {
        read_chars(statep, 2);
        return token_new(statep, TOK_INPLACE_SHR, NULL);
    }

    switch (ch) {
        case '`':
            read_char(statep);
            return token_new(statep, TOK_BACKTICK, NULL);
        case '\"':
            return scan_string(statep);
        case '\'':
            break;
        case '(':
            read_char(statep);
            return token_new(statep, TOK_LEFT_PAREN, NULL);
        case ')':
            read_char(statep);
            return token_new(statep, TOK_RIGHT_PAREN, NULL);
        case '{':
            read_char(statep);
            return token_new(statep, TOK_OPEN_BRACE, NULL);
        case '}':
            read_char(statep);
            return token_new(statep, TOK_CLOSE_BRACE, NULL);
        case '[':
            read_char(statep);
            return token_new(statep, TOK_OPEN_BRACKET, NULL);
        case ']':
            read_char(statep);
            return token_new(statep, TOK_CLOSE_BRACKET, NULL);
        case ';':
            read_char(statep);
            return token_new(statep, TOK_SEMICOLON, NULL);
        case ':':
            read_char(statep);
            return token_new(statep, TOK_COLON, NULL);
        case '.':
            read_char(statep);
            return token_new(statep, TOK_DOT, NULL);
        case ',':
            read_char(statep);
            return token_new(statep, TOK_COMMA, NULL);
        case '=':
            read_char(statep);
            return token_new(statep, TOK_ASSIGN, NULL);
        case '+':
            read_char(statep);
            return token_new(statep, TOK_ADD, NULL);
        case '-':
            read_char(statep);
            return token_new(statep, TOK_SUB, NULL);
        case '*':
            read_char(statep);
            return token_new(statep, TOK_MUL, NULL);
        case '/':
            read_char(statep);
            return token_new(statep, TOK_DIV, NULL);
        case '%':
            read_char(statep);
            return token_new(statep, TOK_MOD, NULL);
        case '>':
            read_char(statep);
            return token_new(statep, TOK_GREATER_THAN, NULL);
        case '<':
            read_char(statep);
            return token_new(statep, TOK_LESS_THAN, NULL);
        case '|':
            read_char(statep);
            return token_new(statep, TOK_OR, NULL);
        case '&':
            read_char(statep);
            return token_new(statep, TOK_AND, NULL);
        case '~':
            read_char(statep);
            return token_new(statep, TOK_NOT, NULL);
        case '!':
            read_char(statep);
            return token_new(statep, TOK_LOGICAL_NOT, NULL);
    }

    statep->lex_errno = LEXER_UKN_CHAR;
    return NULL;
}

static void
lexer_list_destroy_cb(void *p, void *s)
{
    struct token *tok = p;

    if (tok->sb) {
        stringbuf_destroy(tok->sb);
    }

    free(tok);
}

void
lexer_init(struct lexer_state *statep)
{
    statep->lex_errno = 0;
    statep->position = 0;
    statep->text = NULL;
    statep->text_len = 0;
    statep->tokens = list_new();
    statep->row = 0;
    statep->col = 0;
}

void
lexer_fini(struct lexer_state *statep)
{
    list_destroy(statep->tokens, lexer_list_destroy_cb, NULL);
}

void
lexer_scan(struct lexer_state *statep, const char *text)
{
    size_t text_len = strlen(text);
    statep->position = 0;
    statep->text = calloc(1, text_len);
    statep->text_len = text_len;
    
    strncpy(statep->text, text, text_len);
    
    while (statep->position < statep->text_len) {
        struct token *tok = next_token(statep);

        if (!tok && statep->lex_errno != LEXER_EOF) {
            break;
        }

        if (tok) {
            list_append(statep->tokens, tok);
        }
    }

}
