/*
 * parser.c - Responsible for creating abstract syntax tree from token list
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gallium/ast.h>
#include <gallium/lexer.h>
#include <gallium/list.h>
#include <gallium/parser.h>
#include <gallium/stringbuf.h>

#ifndef PATH_MAX
#define PATH_MAX    512
#endif

struct token *
parser_peek_tok(struct parser_state *statep)
{
    struct token *token;

    if (!iter_peek_elem(&statep->iter, (void**)&token)) {
        return NULL;
    }

    return token;
}

struct token *
parser_read_tok(struct parser_state *statep)
{
    struct token *token;

    if (!iter_next_elem(&statep->iter, (void**)&token)) {
        return NULL;
    }

    statep->last_tok = token;

    return token;
}

static bool
parser_match_tok_class(struct parser_state *statep, token_class_t type)
{
    struct token *token = parser_peek_tok(statep);

    if (token) {
        return (token->type == type);
    }

    return false;
}

static bool
parser_match_tok_val(struct parser_state *statep, token_class_t type, const char *val)
{
    size_t val_len = strlen(val);
    struct token *token = parser_peek_tok(statep);

    if (token) {
        return (token->type == type && token->sb && val_len == STRINGBUF_LEN(token->sb) &&
                strcmp(STRINGBUF_VALUE(token->sb), val) == 0);
    }

    return false;
}

bool
parser_accept_tok_class(struct parser_state *statep, token_class_t type)
{
    if (parser_match_tok_class(statep, type)) {
        parser_read_tok(statep);
        return true;
    }

    return false;
}

static bool
parser_accept_tok_val(struct parser_state *statep, token_class_t type, const char *val)
{
    if (parser_match_tok_val(statep, type, val)) {
        parser_read_tok(statep);
        return true;
    }

    return false;
}

void
parser_seterrno(struct parser_state *statep, int err, const char *info)
{
    statep->parser_errno = err;
    statep->err_info = info;
}

/* callback destroying a list of struct ast_node *s */
void
ast_list_destroy_cb(void *elem, void *statep)
{
    struct ast_node *node = elem;

    ast_destroy(node);
}

struct ast_node *   parser_parse_expr(struct parser_state *);

struct ast_node *
parse_term(struct parser_state *statep)
{
    if (parser_accept_tok_val(statep, TOK_KEYWORD, "true")) {
        return bool_term_new(true);
    }

    if (parser_accept_tok_val(statep, TOK_KEYWORD, "false")) {
        return bool_term_new(false);
    }

    struct token *tok = parser_read_tok(statep);

    if (!tok) {
        parser_seterrno(statep, PARSER_UNEXPECTED_EOF, NULL);
        return NULL;
    }

    if (tok->type == TOK_STRING_LIT) {
        return string_term_new(tok->sb);
    }

    if (tok->type == TOK_INT_LIT) {
        int64_t val = strtoll(STRINGBUF_VALUE(tok->sb), NULL, 10);
        
        if (errno != 0) {
            /* conversion failed... */
            parser_seterrno(statep, PARSER_INTEGER_TOO_BIG, NULL);
            return NULL;
        }
        return integer_term_new(val);
    }

    if (tok->type == TOK_IDENT) {
        return symbol_term_new(STRINGBUF_VALUE(tok->sb));
    }

    if (tok->sb) {
        parser_seterrno(statep, PARSER_UNEXPECTED_TOK, STRINGBUF_VALUE(tok->sb));
    } else {
        parser_seterrno(statep, PARSER_UNEXPECTED_TOK, "<not implemented>");
        printf("%x\n", tok->type);
    }

    return NULL;
}

struct ast_node *
parse_quote(struct parser_state *statep)
{
    if (!parser_accept_tok_class(statep, TOK_BACKTICK)) {
        return parse_term(statep);
    }

    struct list *children = list_new();

    while (!parser_match_tok_class(statep, TOK_BACKTICK) && parser_peek_tok(statep) != NULL) {
        struct ast_node *node = parser_parse_decl(statep);

        if (!node) {
            goto error;
        }

        list_append(children, node);
    }

    if (!parser_read_tok(statep)) {
        parser_seterrno(statep, PARSER_EXPECTED_TOK, "`");
        goto error;
    }

    return quote_expr_new(children);
error:
    list_destroy(children, ast_list_destroy_cb, NULL);
    return NULL;
}

static struct ast_node * parse_func(struct parser_state *, bool);

struct ast_node *
parse_anonymous_func(struct parser_state *statep)
{
    if (parser_match_tok_val(statep, TOK_KEYWORD, "func")) {
        return parse_func(statep, true);
    }

    if (parser_match_tok_val(statep, TOK_KEYWORD, "macro")) {
        return parse_func(statep, true);
    }

    return parse_quote(statep);
}

struct ast_node *
parse_dict_expr(struct parser_state *statep)
{
    if (!parser_accept_tok_class(statep, TOK_OPEN_BRACE)) {
        return parse_anonymous_func(statep);
    }

    struct list *kvp_pairs = list_new();

    while (!parser_match_tok_class(statep, TOK_CLOSE_BRACE)) {
        struct ast_node *key = parser_parse_expr(statep);

        if (!key) goto error;

        if (!parser_accept_tok_class(statep, TOK_COLON)) {
            parser_seterrno(statep, PARSER_EXPECTED_TOK, ":");
            ast_destroy(key);
            goto error;
        }

        struct ast_node *val = parser_parse_expr(statep);

        if (!val) {
            ast_destroy(key);
            goto error;
        }

        list_append(kvp_pairs, key_val_expr_new(key, val)); 

        if (!parser_accept_tok_class(statep, TOK_COMMA)) {
            break;
        }
    }

    if (!parser_accept_tok_class(statep, TOK_CLOSE_BRACE)) {
        parser_seterrno(statep, PARSER_EXPECTED_TOK, "}");
        goto error;
    }

    return dict_expr_new(kvp_pairs);

error:
    list_destroy(kvp_pairs, ast_list_destroy_cb, NULL);
    return NULL;
}

struct ast_node *
parse_list_expr(struct parser_state *statep)
{
    if (!parser_accept_tok_class(statep, TOK_OPEN_BRACKET)) {
        return parse_dict_expr(statep);
    }

    struct list *items = list_new();

    while (!parser_match_tok_class(statep, TOK_CLOSE_BRACKET))  {
        struct ast_node *item = parser_parse_expr(statep);
        
        if (!item) goto error;

        list_append_front(items, item);

        if (!parser_accept_tok_class(statep, TOK_COMMA)) {
            break;
        }
    }

    if (!parser_accept_tok_class(statep, TOK_CLOSE_BRACKET)) {
        parser_seterrno(statep, PARSER_EXPECTED_TOK, "]");
        goto error;
    }

    return list_expr_new(items);

error:
    list_destroy(items, ast_list_destroy_cb, NULL);
    return NULL;
}

struct ast_node *
parse_tuple(struct parser_state *statep, struct ast_node *first_item)
{
    struct list *items = list_new();

    list_append(items, first_item);

    for (;;) {
        struct ast_node *expr = parser_parse_expr(statep);

        if (!expr) goto error;

        list_append(items, expr);

        if (!parser_accept_tok_class(statep, TOK_COMMA)) {
            break;
        }
    }

    if (!parser_accept_tok_class(statep, TOK_RIGHT_PAREN)) {
        parser_seterrno(statep, PARSER_EXPECTED_TOK, ")");
        goto error;
    }

    return tuple_expr_new(items);

error:
    list_destroy(items, ast_list_destroy_cb, NULL);
    return NULL;
}

struct ast_node *
parse_grouping(struct parser_state *statep)
{
    struct ast_node *expr = NULL;

    if (parser_accept_tok_class(statep, TOK_LEFT_PAREN)) {
        expr = parser_parse_expr(statep);

        if (!expr) {
            parser_seterrno(statep, PARSER_EXPECTED_EXPR, NULL);
            goto error;
        }

        if (parser_accept_tok_class(statep, TOK_COMMA)) {
            return parse_tuple(statep, expr);
        }

        if (!parser_accept_tok_class(statep, TOK_RIGHT_PAREN)) {
            parser_seterrno(statep, PARSER_EXPECTED_TOK, ")");
            goto error;
        }

        return expr;
    }

    return parse_list_expr(statep);

error:
    if (expr) ast_destroy(expr);
    return NULL;
}

struct ast_node *
parse_index_expr(struct ast_node *left, struct parser_state *statep)
{
    struct ast_node *key = NULL;

    if (!left) return NULL;

    if (parser_accept_tok_class(statep, TOK_OPEN_BRACKET)) {
        key = parser_parse_expr(statep);

        if (!key) {
            goto error;
        }

        if (!parser_accept_tok_class(statep, TOK_CLOSE_BRACKET)) {
            parser_seterrno(statep, PARSER_EXPECTED_TOK, "]");
            goto error;
        }

        return index_access_expr_new(left, key);
    }

    return left;

error:
    if (left) ast_destroy(left);
    if (key) ast_destroy(key);

    return NULL;
}

struct ast_node *
parse_member_access(struct ast_node *left, struct parser_state *statep)
{
    if (!left) return NULL;

    parser_accept_tok_class(statep, TOK_DOT);

    if (!parser_match_tok_class(statep, TOK_IDENT)) {
        ast_destroy(left);
        return NULL;
    }

    struct token *tok = parser_read_tok(statep);
    
    return member_access_expr_new(left, STRINGBUF_VALUE(tok->sb));
}

static struct list *
parse_arglist(struct parser_state *statep)
{
    struct list *args = list_new();

    parser_read_tok(statep);

    while (!parser_match_tok_class(statep, TOK_RIGHT_PAREN)) {
        struct ast_node *expr = parser_parse_expr(statep);

        if (!expr) {
            parser_seterrno(statep, PARSER_EXPECTED_EXPR, NULL);
            goto error;
        }

        list_append(args, expr);

        if (!parser_match_tok_class(statep, TOK_COMMA)) {
            break;
        }

        parser_read_tok(statep);
    }

    if (!parser_accept_tok_class(statep, TOK_RIGHT_PAREN)) {
        parser_seterrno(statep, PARSER_EXPECTED_TOK, ")");
        goto error;
    }

    return args;

error:
    if (args) list_destroy(args, ast_list_destroy_cb, NULL);
    return NULL;
}

struct ast_node *
parse_call_macro_expr(struct ast_node *left, struct parser_state *statep)
{
    struct list *token_list = list_new();

    if (parser_match_tok_class(statep, TOK_LEFT_PAREN)) {
        /* Keep parsing until you run out of parenthesis... */
        int parens = 0; /* How many parenthesis */
        do {
            struct token *tok = parser_read_tok(statep);
            if (tok->type == TOK_RIGHT_PAREN) parens--;
            else if (tok->type == TOK_LEFT_PAREN) parens++;
            struct token *tok_copy = calloc(sizeof(struct token), 1);
            memcpy(tok_copy, tok, sizeof(struct token));
            list_append(token_list, tok_copy);
        } while (parens > 0);
    }

    if (parser_match_tok_class(statep, TOK_OPEN_BRACE)) {
        /* keep parsing until you hit a closing brace on the same indent line as the macro call */
        int indent_level = -1;
        struct token *tokens_start = parser_peek_tok(statep);
        
        do {
            struct token *tok = parser_read_tok(statep);
            if (indent_level == -1 && tok->row != tokens_start->row) {
                indent_level = tok->col;
            }
            struct token *tok_copy = calloc(sizeof(struct token), 1);
            memcpy(tok_copy, tok, sizeof(struct token));
            list_append(token_list, tok_copy);

            if (tok->type == TOK_CLOSE_BRACE && tok->col <= indent_level) {
                break;
            }
        } while (parser_peek_tok(statep));
    }
    return call_macro_expr_new(left, token_list);
}

struct ast_node *
parse_call_expr(struct ast_node *left, struct parser_state *statep)
{
    if (!left) return NULL;

    struct ast_node *ret = NULL;

    if (parser_accept_tok_class(statep, TOK_BACKTICK)) {
        ret = parse_call_macro_expr(left, statep);
    } else if (parser_match_tok_class(statep, TOK_LEFT_PAREN)) {
        struct list *params = parse_arglist(statep);
        if (!params) return NULL;
        ret = parse_call_expr(call_expr_new(left, params), statep);
    } else if (parser_match_tok_class(statep, TOK_DOT)) {
        ret = parse_call_expr(parse_member_access(left, statep), statep);
    } else if (parser_match_tok_class(statep, TOK_OPEN_BRACKET)) {
        ret = parse_call_expr(parse_index_expr(left, statep), statep);
    }

    if (ret) {
        return ret;
    }

    return left;
}

struct ast_node *
parse_unary(struct parser_state *statep)
{
    unaryop_t op;

    if (parser_match_tok_class(statep, TOK_LOGICAL_NOT)) {
        op = UNARYOP_LOGICAL_NOT;    
    } else if (parser_match_tok_class(statep, TOK_NOT)) {
        op = UNARYOP_NOT;
    } else if (parser_match_tok_class(statep, TOK_SUB)) {
        op = UNARYOP_NEGATE;
    } else {
        return parse_call_expr(parse_grouping(statep), statep);
    }

    parser_read_tok(statep);

    struct ast_node *expr = parse_call_expr(parse_grouping(statep), statep);

    if (!expr) return NULL;

    return unary_expr_new(op, expr);
}

struct ast_node *
parse_div_mul(struct parser_state *statep)
{
    struct ast_node *left = parse_unary(statep);

    if (!left) return NULL;

    while (parser_match_tok_class(statep, TOK_DIV) || parser_match_tok_class(statep, TOK_MUL) ||
            parser_match_tok_class(statep, TOK_MOD))
    {
        struct token *tok = parser_read_tok(statep);
        struct ast_node *right = parse_unary(statep);
        binop_t op;

        if (!right) {
            ast_destroy(left);
            return NULL;
        }

        switch (tok->type) {
            case TOK_DIV:
                op = BINOP_DIV;
                break;
            case TOK_MUL:
                op = BINOP_MUL;
                break;
            case TOK_MOD:
                op = BINOP_MOD;
                break;
            default:
                return NULL;

        }

        left = bin_expr_new(op, left, right);
    }

    return left; 
}

struct ast_node *
parse_add_sub(struct parser_state *statep)
{
    struct ast_node *left = parse_div_mul(statep);

    if (!left) return NULL;

    while (parser_match_tok_class(statep, TOK_ADD) || parser_match_tok_class(statep, TOK_SUB)) {
        struct token *tok = parser_read_tok(statep);
        struct ast_node *right = parse_div_mul(statep);
        binop_t op;

        if (!right) {
            ast_destroy(left);
            return NULL;
        }

        switch (tok->type) {
            case TOK_ADD:
                op = BINOP_ADD;
                break;
            case TOK_SUB:
                op = BINOP_SUB;
                break;
            default:
                return NULL;

        }

        left = bin_expr_new(op, left, right);
    }

    return left;
}

struct ast_node *
parse_bitshift(struct parser_state *statep)
{
    struct ast_node *left = parse_add_sub(statep);

    if (!left) return NULL;

    while (parser_match_tok_class(statep, TOK_SHL) || parser_match_tok_class(statep, TOK_SHR)) {
        struct token *tok = parser_read_tok(statep);
        struct ast_node *right = parse_add_sub(statep);
        binop_t op;

        if (!right) {
            ast_destroy(left);
            return NULL;
        }

        switch (tok->type) {
            case TOK_SHL:
                op = BINOP_SHL;
                break;
            case TOK_SHR:
                op = BINOP_SHR;
                break;
            default:
                return NULL;

        }

        left = bin_expr_new(op, left, right);
    }

    return left;
}

struct ast_node *
parse_relational(struct parser_state *statep)
{
    struct ast_node *left = parse_bitshift(statep);

    if (!left) return NULL;

    while ( parser_match_tok_class(statep, TOK_GREATER_THAN) || parser_match_tok_class(statep, TOK_LESS_THAN) ||
            parser_match_tok_class(statep, TOK_GREATER_THAN_OR_EQU) || parser_match_tok_class(statep, TOK_LESS_THAN_OR_EQU))
    {
        struct token *tok = parser_read_tok(statep);
        struct ast_node *right = parse_bitshift(statep);
        binop_t op;

        if (!right) {
            return NULL;
        }

        switch (tok->type) {
            case TOK_GREATER_THAN:
                op = BINOP_GREATER_THAN;
                break;
            case TOK_LESS_THAN:
                op = BINOP_LESS_THAN;
                break;
            case TOK_GREATER_THAN_OR_EQU:
                op = BINOP_GREATER_THAN_OR_EQU;
                break;
            case TOK_LESS_THAN_OR_EQU:
                op = BINOP_LESS_THAN_OR_EQU;
                break;
            default:
                return NULL;
        }

        left = bin_expr_new(op, left, right);
    }

    return left;
}

struct ast_node *
parse_equality(struct parser_state *statep)
{
    struct ast_node *left = parse_relational(statep);

    if (!left) return NULL;

    while (parser_match_tok_class(statep, TOK_EQUALS) || parser_match_tok_class(statep, TOK_NOT_EQUALS)) {
        struct token *tok = parser_read_tok(statep);
        struct ast_node *right = parse_relational(statep);
        binop_t op;

        if (!right) {
            ast_destroy(left);
            return NULL;
        }

        switch (tok->type) {
            case TOK_EQUALS:
                op = BINOP_EQUALS;
                break;
            case TOK_NOT_EQUALS:
                op = BINOP_NOT_EQUALS;
                break;
            default:
                return NULL;

        }

        left = bin_expr_new(op, left, right);
    }

    return left;
}

struct ast_node *
parse_and(struct parser_state *statep)
{
    struct ast_node *left = parse_equality(statep);

    if (!left) return NULL;

    while (parser_accept_tok_class(statep, TOK_AND)) {
        struct ast_node *right = parse_equality(statep);

        if (!right) {
            ast_destroy(left);
            return NULL; 
        }

        left = bin_expr_new(BINOP_AND, left, right);
    }

    return left;
}

struct ast_node *
parse_xor(struct parser_state *statep)
{
    struct ast_node *left = parse_and(statep);

    if (!left) return NULL;

    while (parser_accept_tok_class(statep, TOK_XOR)) {
        struct ast_node *right = parse_and(statep);

        if (!right) {
            ast_destroy(left);
            return NULL;
        }

        left = bin_expr_new(BINOP_XOR, left, right);
    }

    return left;
}

struct ast_node *
parse_or(struct parser_state *statep)
{
    struct ast_node *left = parse_xor(statep);

    if (!left) return NULL;

    while (parser_accept_tok_class(statep, TOK_OR)) {
        struct ast_node *right = parse_xor(statep);

        if (!right) {
            ast_destroy(left);
            return NULL;
        }

        left = bin_expr_new(BINOP_OR, left, right);
    }

    return left;
}

struct ast_node *
parse_logical_and(struct parser_state *statep)
{
    struct ast_node *left = parse_or(statep);

    if (!left) return NULL;

    while (parser_accept_tok_class(statep, TOK_LOGICAL_AND)) {
        struct ast_node *right = parse_or(statep);

        if (!right) {
            ast_destroy(left);
            return NULL;
        }

        left = bin_expr_new(BINOP_LOGICAL_AND, left, right);
    }

    return left;
}

struct ast_node *
parse_logical_or(struct parser_state *statep)
{
    struct ast_node *left = parse_logical_and(statep);

    if (!left) return NULL;

    while (parser_accept_tok_class(statep, TOK_LOGICAL_OR)) {
        struct ast_node *right = parse_logical_and(statep);

        if (!right) {
            ast_destroy(left);
            return NULL;
        }

        left = bin_expr_new(BINOP_LOGICAL_OR, left, right);
    }

    return left;
}

struct ast_node *
parse_range(struct parser_state *statep)
{
    struct ast_node *left = parse_logical_or(statep);

    if (!left) return NULL;

    while (parser_match_tok_class(statep, TOK_HALF_RANGE)
            || parser_match_tok_class(statep, TOK_CLOSED_RANGE))
    {
        struct token *tok = parser_read_tok(statep);
        struct ast_node *right = parse_logical_or(statep);

        if (!right) {
            ast_destroy(left);
        }

        switch (tok->type) {
            case TOK_HALF_RANGE:
                left = bin_expr_new(BINOP_HALF_RANGE, left, right);
                break;
            case TOK_CLOSED_RANGE:
                left = bin_expr_new(BINOP_CLOSED_RANGE, left, right);
                break;
            default:
                break;
        }
    }

    return left;
}

static bool
match_inplace_assign(struct parser_state *statep)
{
    struct token *tok = parser_peek_tok(statep);

    if (!tok) return false;

    switch (tok->type) {
        case TOK_INPLACE_ADD:
        case TOK_INPLACE_SUB:
        case TOK_INPLACE_MUL:
        case TOK_INPLACE_DIV:
        case TOK_INPLACE_MOD:
        case TOK_INPLACE_AND:
        case TOK_INPLACE_XOR:
        case TOK_INPLACE_OR:
        case TOK_INPLACE_SHL:
        case TOK_INPLACE_SHR:
            return true;
        default:
            return false;
    }
}

struct ast_node *
parse_assign(struct parser_state *statep)
{
    struct ast_node *left = parse_range(statep);

    if (!left) return NULL;

    while (parser_match_tok_class(statep, TOK_ASSIGN) || match_inplace_assign(statep)) {

        if (parser_accept_tok_class(statep, TOK_ASSIGN)) {
            struct ast_node *right = parse_assign(statep);
            
            if (!right) {
                ast_destroy(left);
                return NULL;
            }

            left = assign_expr_new(left, right);
        } else {
            struct token *tok = parser_read_tok(statep);
            
            binop_t type;

            switch (tok->type) {
                case TOK_INPLACE_ADD:
                    type = BINOP_ADD;
                    break;
                case TOK_INPLACE_SUB:
                    type = BINOP_SUB;
                    break;
                case TOK_INPLACE_MUL:
                    type = BINOP_MUL;
                    break;
                case TOK_INPLACE_DIV:
                    type = BINOP_DIV;
                    break;
                case TOK_INPLACE_MOD:
                    type = BINOP_MOD;
                    break;
                case TOK_INPLACE_AND:
                    type = BINOP_AND;
                    break;
                case TOK_INPLACE_XOR:
                    type = BINOP_XOR;
                    break;
                case TOK_INPLACE_OR:
                    type = BINOP_OR;
                    break;
                case TOK_INPLACE_SHL:
                    type = BINOP_SHL;
                    break;
                case TOK_INPLACE_SHR:
                    type = BINOP_SHR;
                    break;
                default:
                    /* panic */
                    return NULL;
            }

            struct ast_node *right = parse_assign(statep);

            if (!right) {
                ast_destroy(left);
                return NULL;
            }

            left = assign_expr_new(left, bin_expr_new(type, left, right));
        }
    }

    return left;
}

struct ast_node *
parse_pattern_term(struct parser_state *statep)
{
    struct ast_node *expr = parse_call_expr(parse_grouping(statep), statep);

    if (!expr) {
        return NULL;
    }

    return expr;
}

struct ast_node *
parse_pattern(struct parser_state *statep)
{
    return parse_pattern_term(statep);
}

struct ast_node *
parse_match_expr(struct parser_state *statep)
{
    if (!parser_accept_tok_val(statep, TOK_KEYWORD, "match")) {
        return parse_assign(statep);
    }

    struct ast_node *expr = parser_parse_expr(statep);

    if (!expr) return NULL;

    struct list *cases = list_new();
    struct ast_node *pattern = NULL;
    struct ast_node *cond = NULL;
    struct ast_node *val = NULL;
    struct ast_node *default_case = NULL;

    if (!parser_accept_tok_class(statep, TOK_OPEN_BRACE)) {
        parser_seterrno(statep, PARSER_EXPECTED_TOK, "{");
        goto error_1;
    }


    while (parser_accept_tok_val(statep, TOK_KEYWORD, "case")) {
        pattern = parse_pattern(statep);

        if (!pattern) goto error_1;
        
        if (parser_accept_tok_val(statep, TOK_KEYWORD, "when")) {
            cond = parser_parse_expr(statep);

            if (!cond) goto error_2;
        }

        if (!parser_accept_tok_class(statep, TOK_PHAT_ARROW)) {
            parser_seterrno(statep, PARSER_EXPECTED_TOK, "=>");
            goto error_3;
        }

        val = parser_parse_expr(statep);

        if (!val) goto error_3;

        list_append(cases, match_case_new(pattern, cond, val));
    }

    if (parser_accept_tok_val(statep, TOK_KEYWORD, "default")) {

        if (!parser_accept_tok_class(statep, TOK_PHAT_ARROW)) {
            parser_seterrno(statep, PARSER_EXPECTED_TOK, "=>");
            goto error_1;
        }
        
        default_case = parser_parse_expr(statep);

        if (!default_case) goto error_1;
    }

    if (!parser_accept_tok_class(statep, TOK_CLOSE_BRACE)) {
        parser_seterrno(statep, PARSER_EXPECTED_TOK, "}");
        goto error_1;
    }

    return match_expr_new(expr, cases, default_case);

error_3:
    if (cond) ast_destroy(cond);
error_2:
    if (pattern) ast_destroy(pattern);
error_1:
    if (expr) ast_destroy(expr);
    
    return NULL;
}

struct ast_node *
parse_when_expr(struct parser_state *statep)
{
    struct ast_node *left = parse_match_expr(statep);

    if (!parser_accept_tok_val(statep, TOK_KEYWORD, "when")) {
        return left;
    }

    struct ast_node *cond = parser_parse_expr(statep);

    if (!cond) goto error;

    if (!parser_accept_tok_val(statep, TOK_KEYWORD, "else")) goto error;

    struct ast_node *else_val = parser_parse_expr(statep);

    if (!else_val) goto error;

    return when_expr_new(left, cond, else_val);

error:
    if (left) ast_destroy(left);
    if (cond) ast_destroy(cond);
    
    return NULL;
}

struct ast_node *
parser_parse_expr(struct parser_state *statep)
{
    return parse_when_expr(statep);
}

struct ast_node *   parser_parse_decl(struct parser_state *);
struct ast_node *   parser_parse_stmt(struct parser_state *);

struct ast_node *
parse_code_block(struct parser_state *statep)
{
    parser_read_tok(statep);

    struct list *children = list_new();

    while (!parser_match_tok_class(statep, TOK_CLOSE_BRACE) && parser_peek_tok(statep) != NULL) {
        struct ast_node *node = parser_parse_decl(statep);

        if (!node) {
            goto error;
        }
        
        list_append(children, node);
    }

    if (!parser_read_tok(statep)) {
        parser_seterrno(statep, PARSER_EXPECTED_TOK, "}");
        goto error;
    }

    if (LIST_COUNT(children) == 1) {
        struct ast_node *val = list_first(children);
        list_destroy(children, NULL, NULL);
        return val;
    } else {
        return code_block_new(children);
    }
error:
    list_destroy(children, ast_list_destroy_cb, NULL);
    return NULL;
}

struct ast_node *
parse_for(struct parser_state *statep)
{
    parser_read_tok(statep);

    if (!parser_match_tok_class(statep, TOK_IDENT)) {
        parser_seterrno(statep, PARSER_EXPECTED_TOK, "<ident>");
        return NULL;
    }

    struct token *tok = parser_read_tok(statep);

    if (!parser_accept_tok_val(statep, TOK_KEYWORD, "in")) {
        parser_seterrno(statep, PARSER_EXPECTED_TOK, "in");
        return NULL;
    }

    struct ast_node *expr = parser_parse_expr(statep);

    if (!expr) {
        return NULL;
    }

    struct ast_node *body = parser_parse_stmt(statep);

    if (!body) {
        ast_destroy(expr);
        return NULL;
    }

    return for_stmt_new(STRINGBUF_VALUE(tok->sb), expr, body);
}

struct ast_node *
parse_if(struct parser_state *statep)
{
    parser_read_tok(statep);

    struct ast_node *cond = parser_parse_expr(statep);

    if (!cond) {
        return NULL;
    }

    struct ast_node *body = parser_parse_stmt(statep);
    struct ast_node *else_body = NULL;

    if (!body) {
        goto error;
    }

    if (parser_match_tok_val(statep, TOK_KEYWORD, "else")) {
        parser_read_tok(statep);
        else_body = parser_parse_stmt(statep);

        if (!else_body) {
            goto error;
        }
    }

    return if_stmt_new(cond, body, else_body);
    
error:
    if (cond) ast_destroy(cond);
    if (body) ast_destroy(body);
    if (else_body) ast_destroy(else_body);
    return NULL;
}

struct ast_node *
parse_return(struct parser_state *statep)
{
    parser_read_tok(statep);

    struct ast_node *val = parser_parse_expr(statep);

    if (!val) {
        return NULL;
    }

    return return_stmt_new(val);
}

struct ast_node *
parse_try(struct parser_state *statep)
{
    parser_read_tok(statep);

    struct ast_node *try_body = parser_parse_stmt(statep);

    if (!try_body) {
        goto error;
    }

    if (!parser_accept_tok_val(statep, TOK_KEYWORD, "except")) {
        parser_seterrno(statep, PARSER_EXPECTED_TOK, "except");
        goto error;
    }

    const char *varname = NULL;

    if (parser_match_tok_class(statep, TOK_IDENT)) {
        struct token *tok = parser_read_tok(statep);
        varname = STRINGBUF_VALUE(tok->sb);
    }

    struct ast_node *except_body = parser_parse_stmt(statep);

    if (!except_body) {
        goto error;
    }

    return try_stmt_new(try_body, except_body, varname);

error:
    if (try_body) ast_destroy(try_body);
    if (except_body) ast_destroy(except_body);

    return NULL;
}

struct ast_node *
parse_use(struct parser_state *statep)
{
    char import_path[PATH_MAX+1];
    int component = 0;

    import_path[0] = 0;

    parser_read_tok(statep);

    do {
        struct token *tok = parser_read_tok(statep);

        if (!tok) {
            parser_seterrno(statep, PARSER_UNEXPECTED_EOF, NULL);
            return NULL;
        }

        if (tok->type != TOK_IDENT) {
            /* expected ident */
            parser_seterrno(statep, PARSER_EXPECTED_TOK, "<ident>");
            return NULL;
        }
    
        if (component > 0) {
            strncat(import_path, "/", PATH_MAX);
        }

        strncat(import_path, STRINGBUF_VALUE(tok->sb), PATH_MAX);
        component++;
    } while (parser_accept_tok_class(statep, TOK_DOT));


    bool wildcard = false;
    struct list *imports = NULL;

    if (parser_accept_tok_class(statep, TOK_THICC_COLON)) {

        /* ::{ident[, <ident>...]} */
        if (parser_accept_tok_class(statep, TOK_OPEN_BRACE)) {
            imports = list_new();

            do {
                struct token *tok = parser_read_tok(statep);

                if (!tok) {
                    parser_seterrno(statep, PARSER_UNEXPECTED_EOF, NULL);
                    goto error; 
                }
                
                list_append(imports, symbol_term_new(STRINGBUF_VALUE(tok->sb)));

            } while (parser_accept_tok_class(statep, TOK_COMMA));
            
            if (!parser_accept_tok_class(statep, TOK_CLOSE_BRACE)) {
                parser_seterrno(statep, PARSER_EXPECTED_TOK, "}");
                goto error;
            }
        } else if (parser_accept_tok_class(statep, TOK_MUL)) {
            wildcard = true;
        } else if (parser_match_tok_class(statep, TOK_IDENT)) {
            imports = list_new();

            struct token *tok = parser_read_tok(statep);

            list_append(imports, symbol_term_new(STRINGBUF_VALUE(tok->sb)));
        }
    }

    return use_stmt_new(import_path, imports, wildcard);
    
error:
    list_destroy(imports, ast_list_destroy_cb, NULL);
    return NULL;
}

struct ast_node *
parse_while(struct parser_state *statep)
{
    parser_read_tok(statep);

    struct ast_node *cond = parser_parse_expr(statep);

    if (!cond) {
        return NULL;
    }

    struct ast_node *body = parser_parse_stmt(statep);
    
    if (!body) {
        ast_destroy(cond);
        return NULL;
    }
    
    return while_stmt_new(cond, body);
}

struct ast_node *
parser_parse_stmt(struct parser_state *statep)
{
    if (parser_match_tok_class(statep, TOK_SEMICOLON)) {
        while (parser_accept_tok_class(statep, TOK_SEMICOLON));
        return &ast_empty_stmt_inst;
    }

    if (parser_match_tok_val(statep, TOK_KEYWORD, "if")) {
        return parse_if(statep);
    }

    if (parser_match_tok_val(statep, TOK_KEYWORD, "return")) {
        return parse_return(statep);
    }

    if (parser_match_tok_val(statep, TOK_KEYWORD, "try")) {
        return parse_try(statep);
    }

    if (parser_match_tok_val(statep, TOK_KEYWORD, "use")) {
        return parse_use(statep);
    }

    if (parser_match_tok_val(statep, TOK_KEYWORD, "while")) {
        return parse_while(statep);
    }

    if (parser_match_tok_val(statep, TOK_KEYWORD, "for")) {
        return parse_for(statep);
    }

    if (parser_accept_tok_val(statep, TOK_KEYWORD, "break")) {
        return break_stmt_new();
    }

    if (parser_accept_tok_val(statep, TOK_KEYWORD, "continue")) {
        return continue_stmt_new();
    }

    if (parser_match_tok_class(statep, TOK_OPEN_BRACE)) {
        return parse_code_block(statep);        
    }

    return parser_parse_expr(statep);
}

struct ast_node *parser_parse_decl(struct parser_state *statep);

struct ast_node *
parse_class(struct parser_state *statep)
{
    parser_read_tok(statep);

    if (!parser_match_tok_class(statep, TOK_IDENT)) {
        /* set the right error */
        return NULL;
    }

    struct token *class_name = parser_read_tok(statep);
    struct ast_node *base = NULL;

    if (parser_accept_tok_val(statep, TOK_KEYWORD, "extends")) {
        base = parser_parse_expr(statep);

        if (!base) {
            return NULL;
        }
    }

    if (!parser_accept_tok_class(statep, TOK_OPEN_BRACE)) {
        parser_seterrno(statep, PARSER_EXPECTED_TOK, "{");
        return NULL;
    }

    struct list *methods = list_new();

    while (!parser_accept_tok_class(statep, TOK_CLOSE_BRACE)) {
        struct ast_node *method = parser_parse_decl(statep);

        if (!method) {
            goto error;
        }

        list_append(methods, method);
    }
   
    return class_decl_new(STRINGBUF_VALUE(class_name->sb), base, methods);

error:
    if (base) ast_destroy(base);
    list_destroy(methods, ast_list_destroy_cb, NULL);
    return NULL;
}

struct ast_node *
parse_func(struct parser_state *statep, bool is_expr)
{
    parser_read_tok(statep);

    const char *func_name = NULL;

    if (!is_expr) {
        if (!parser_match_tok_class(statep, TOK_IDENT)) {
            /* TODO: set error*/
            return NULL;
        }

        struct token *tok = parser_read_tok(statep);
        
        func_name = STRINGBUF_VALUE(tok->sb);
    }

    if (!parser_match_tok_class(statep, TOK_LEFT_PAREN)) {
        statep->parser_errno = PARSER_EXPECTED_TOK;
        statep->err_info = "(";
        return NULL;
    }
    
    parser_read_tok(statep);

    struct list *params = list_new();
    struct list *statements = list_new();

    while (!parser_match_tok_class(statep, TOK_RIGHT_PAREN)) {
        if (!parser_match_tok_class(statep, TOK_IDENT)) {
            goto error;
        }

        struct token *func_param = parser_read_tok(statep);

        list_append(params, func_param_new(STRINGBUF_VALUE(func_param->sb)));

        if (!parser_match_tok_class(statep, TOK_COMMA)) {
            break;
        }
        parser_read_tok(statep);
    }

    if (!parser_match_tok_class(statep, TOK_RIGHT_PAREN)) {
        goto error;
    }

    parser_read_tok(statep);

    if (parser_accept_tok_class(statep, TOK_OPEN_BRACE)) {
        while (!parser_match_tok_class(statep, TOK_CLOSE_BRACE) && parser_peek_tok(statep) != NULL) {
            struct ast_node *node = parser_parse_decl(statep);

            if (!node) goto error;

            list_append(statements, node);
        }

        if (!parser_read_tok(statep)) {
            parser_seterrno(statep, PARSER_EXPECTED_TOK, "}");
            goto error;
        }
    } else if (parser_accept_tok_class(statep, TOK_PHAT_ARROW)) {
        struct ast_node *expr_body = parser_parse_expr(statep);

        if (!expr_body) goto error;

        list_append(statements, return_stmt_new(expr_body));
    }

    if (is_expr) {
        return func_expr_new(params, code_block_new(statements));
    }

    return func_decl_new(func_name, params, code_block_new(statements));

error:
    list_destroy(params, ast_list_destroy_cb, NULL);
    list_destroy(statements, ast_list_destroy_cb, NULL);
    return NULL;
}

struct ast_node *
parser_parse_decl(struct parser_state  *statep)
{
    if (parser_match_tok_val(statep, TOK_KEYWORD, "macro")) {
        return parse_func(statep, false);
    }

    if (parser_match_tok_val(statep, TOK_KEYWORD, "func")) {
        return parse_func(statep, false);
    }

    if (parser_match_tok_val(statep, TOK_KEYWORD, "class")) {
        return parse_class(statep);
    }

    return parser_parse_stmt(statep);
}

static void
explain_lexer_error(struct parser_state *statep)
{
    fprintf(stderr, "line: %d,%d: ", statep->lex_state.row, statep->lex_state.col);

    switch (statep->lex_state.lex_errno) {
        case LEXER_UNTERMINATED_STR:
            fputs("unterminated string literal", stderr);
            break;
        case LEXER_UKN_CHAR:
            fputs("encountered unexpected character", stderr);
            break;
        default:
            fputs("unknown error countered during lexical analysis", stderr);
            break;
    }
    
    fputs("\n", stderr);
}

void
parser_explain(struct parser_state *statep)
{
    if (statep->parser_errno == PARSER_LEXER_ERR) {
        explain_lexer_error(statep);
        return;
    }

    struct token *tok = statep->last_tok;

    if (tok) {
        fprintf(stderr, "line: %d,%d: ", tok->row, tok->col);
    }

    switch (statep->parser_errno) {
        case PARSER_EXPECTED_TOK:
            fprintf(stderr, "expected '%s'", statep->err_info);
            break;
        case PARSER_UNEXPECTED_TOK:
            fprintf(stderr, "unexpected '%s'", statep->err_info);
            break;
        case PARSER_EXPECTED_EXPR:
            fputs("an expression was expected", stderr);
            break;
        case PARSER_UNEXPECTED_EOF:
            fputs("unexpected end of file", stderr);
            break;
        default:
            fputs("unknown error during semantic analysis", stderr);
            break;
    }
    
    fputs("\n", stderr);
}

void
parser_init_lazy(struct parser_state *statep, struct list *tokens)
{
    memset(statep, 0, sizeof(struct parser_state));

    list_get_iter(tokens, &statep->iter);
}

struct ast_node *
parser_parse(struct parser_state *statep, const char *src)
{
    memset(statep, 0, sizeof(struct parser_state));

    lexer_init(&statep->lex_state);
    lexer_scan(&statep->lex_state, src);

    if (statep->lex_state.lex_errno != LEXER_EOF) {
        statep->parser_errno = PARSER_LEXER_ERR;
        return NULL;
    }

    list_get_iter(statep->lex_state.tokens, &statep->iter);
    
    return parser_parse_all(statep);
}

struct ast_node *
parser_parse_all(struct parser_state *statep)
{
    struct list *nodes = list_new();

    while (parser_peek_tok(statep)) {
        struct ast_node *node = parser_parse_decl(statep);

        if (!node) {
            list_destroy(nodes, ast_list_destroy_cb, NULL);
            return NULL;
        }

        list_append(nodes, node);
    }

    return code_block_new(nodes);
}

void
parser_fini(struct parser_state *statep)
{
    lexer_fini(&statep->lex_state);
}
