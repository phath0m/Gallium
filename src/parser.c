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
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gallium/list.h>
#include <gallium/stringbuf.h>
#include <gallium/builtins.h>
#include "parser.h"
#include "lexer.h"
#include "parser.h"

#ifndef PATH_MAX
#define PATH_MAX    512
#endif

struct token *
GaParser_PeekTok(struct parser_state *statep)
{
    struct token *token;

    if (!_Ga_iter_peek(&statep->iter, (void**)&token)) {
        return NULL;
    }

    return token;
}

struct token *
GaParser_PeakNTok(struct parser_state *statep, int n)
{
    struct token *token;

    if (!_Ga_iter_peek_ex(&statep->iter, n, (void**)&token)) {
        return NULL;
    }

    return token;
}

struct token *
GaParser_ReadTok(struct parser_state *statep)
{
    struct token *token;

    if (!_Ga_iter_next(&statep->iter, (void**)&token)) {
        return NULL;
    }

    statep->last_tok = token;

    return token;
}

bool
GaParser_MatchNTokClass(struct parser_state *statep, int n, token_class_t type)
{
    struct token *token = GaParser_PeakNTok(statep, n);

    if (token) {
        return (token->type == type);
    }

    return false;
}

bool
GaParser_MatchNTokVal(struct parser_state *statep, int n, token_class_t type,
                      const char *val)
{
    size_t val_len = strlen(val);
    struct token *token = GaParser_PeakNTok(statep, n);

    if (token) {
        return (token->type == type &&
                token->sb && val_len == STRINGBUF_LEN(token->sb) &&
                strcmp(STRINGBUF_VALUE(token->sb), val) == 0);
    }

    return false;
}

bool
GaParser_MatchTokClass(struct parser_state *statep, token_class_t type)
{
    struct token *token = GaParser_PeekTok(statep);

    if (token) {
        return (token->type == type);
    }

    return false;
}

bool
GaParser_MatchTokVal(struct parser_state *statep, token_class_t type,
                     const char *val)
{
    size_t val_len = strlen(val);
    struct token *token = GaParser_PeekTok(statep);

    if (token) {
        return (token->type == type &&
                token->sb && val_len == STRINGBUF_LEN(token->sb) &&
                strcmp(STRINGBUF_VALUE(token->sb), val) == 0);
    }

    return false;
}

bool
GaParser_AcceptTokClass(struct parser_state *statep, token_class_t type)
{
    if (GaParser_MatchTokClass(statep, type)) {
        GaParser_ReadTok(statep);
        return true;
    }

    return false;
}

bool
GaParser_AcceptTokVal(struct parser_state *statep, token_class_t type,
                      const char *val)
{
    if (GaParser_MatchTokVal(statep, type, val)) {
        GaParser_ReadTok(statep);
        return true;
    }

    return false;
}

static void
parser_error(struct parser_state *statep, const char *fmt, ...)
{
    char msg[512];
    va_list vlist;
    va_start(vlist, fmt);
    vsnprintf(msg, sizeof(msg) - 1, fmt, vlist);
    va_end(vlist);
    struct token *tok = statep->last_tok;
    if (!statep->error) {
        assert(tok);
        statep->error = GaErr_NewSyntaxError("line: %d, %d: %s", tok->row,
                                             tok->col, msg);
        GaObj_INC_REF(statep->error);
    } 
}

static void
parser_eof_error(struct parser_state *statep)
{
    parser_error(statep, "unexpected end of file");
}

static void
parser_unexpected_token_error(struct parser_state *statep, const char *tok)
{
    parser_error(statep, "Got unexpected token %s", tok);
}

static void
parser_expected_token_error(struct parser_state *statep, const char *tok)
{
    parser_error(statep, "Expected token %s", tok);
}

/* callback destroying a list of struct ast_node *s */
void
_GaAst_ListDestroyCb(void *elem, void *statep)
{
    struct ast_node *node = elem;

    GaAst_Destroy(node);
}

struct ast_node *   _GaParser_ParseExpr(struct parser_state *);

static struct ast_node *
parse_term(struct parser_state *statep)
{
    if (GaParser_AcceptTokVal(statep, TOK_KEYWORD, "true")) {
        return GaAst_NewBool(true);
    }

    if (GaParser_AcceptTokVal(statep, TOK_KEYWORD, "false")) {
        return GaAst_NewBool(false);
    }

    struct token *tok = GaParser_ReadTok(statep);

    if (!tok) {
        parser_eof_error(statep);
        return NULL;
    }

    if (tok->type == TOK_STRING_LIT) {
        return GaAst_NewString(tok->sb);
    }

    if (tok->type == TOK_INT_LIT) {
        int64_t val = strtoll(STRINGBUF_VALUE(tok->sb), NULL, 10);
        
        if (errno != 0) {
            /* conversion failed... */
            parser_error(statep, "integer value exceeds maximum size");
            return NULL;
        }
        return GaAst_NewInteger(val);
    }

    if (tok->type == TOK_FLOAT_LIT) {
        double val = atof(STRINGBUF_VALUE(tok->sb));
        return GaAst_NewFloat(val);
    }

    if (tok->type == TOK_IDENT) {
        return GaAst_NewSymbol(STRINGBUF_VALUE(tok->sb));
    }

    if (tok->sb) {
        parser_unexpected_token_error(statep, STRINGBUF_VALUE(tok->sb));
    } else {
        parser_error(statep, "Got unexpected token of type %x", tok->type);
    }

    return NULL;
}

static struct ast_node *
parse_quote(struct parser_state *statep)
{
    if (!GaParser_AcceptTokClass(statep, TOK_BACKTICK)) {
        return parse_term(statep);
    }

    _Ga_list_t *children = _Ga_list_new();

    while (!GaParser_MatchTokClass(statep, TOK_BACKTICK) &&
            GaParser_PeekTok(statep) != NULL)
    {
        struct ast_node *node = _GaParser_ParseDecl(statep);

        if (!node) {
            goto error;
        }

        _Ga_list_push(children, node);
    }

    if (!GaParser_ReadTok(statep)) {
        parser_expected_token_error(statep, "`");
        goto error;
    }

    return GaAst_NewQuote(children);
error:
    _Ga_list_destroy(children, _GaAst_ListDestroyCb, NULL);
    return NULL;
}

static struct ast_node * _GaParser_ParseFunc(struct parser_state *, bool);

static struct ast_node *
parse_anonymous_func(struct parser_state *statep)
{
    if (GaParser_MatchTokVal(statep, TOK_KEYWORD, "func")) {
        return _GaParser_ParseFunc(statep, true);
    }

    if (GaParser_MatchTokVal(statep, TOK_KEYWORD, "macro")) {
        return _GaParser_ParseFunc(statep, true);
    }

    return parse_quote(statep);
}

static struct ast_node *
parse_dict_expr(struct parser_state *statep)
{
    if (!GaParser_AcceptTokClass(statep, TOK_OPEN_BRACE)) {
        return parse_anonymous_func(statep);
    }

    _Ga_list_t *kvp_pairs = _Ga_list_new();

    while (!GaParser_MatchTokClass(statep, TOK_CLOSE_BRACE)) {
        struct ast_node *key = _GaParser_ParseExpr(statep);

        if (!key) goto error;

        if (!GaParser_AcceptTokClass(statep, TOK_COLON)) {
            parser_expected_token_error(statep, ":");
            GaAst_Destroy(key);
            goto error;
        }

        struct ast_node *val = _GaParser_ParseExpr(statep);

        if (!val) {
            GaAst_Destroy(key);
            goto error;
        }

        _Ga_list_push(kvp_pairs, GaAst_NewKeyValuePair(key, val)); 

        if (!GaParser_AcceptTokClass(statep, TOK_COMMA)) {
            break;
        }
    }

    if (!GaParser_AcceptTokClass(statep, TOK_CLOSE_BRACE)) {
        parser_expected_token_error(statep, "}");
        goto error;
    }

    return GaAst_NewDict(kvp_pairs);

error:
    _Ga_list_destroy(kvp_pairs, _GaAst_ListDestroyCb, NULL);
    return NULL;
}

static struct ast_node *
parse_list_expr(struct parser_state *statep)
{
    if (!GaParser_AcceptTokClass(statep, TOK_OPEN_BRACKET)) {
        return parse_dict_expr(statep);
    }

    _Ga_list_t *items = _Ga_list_new();

    while (!GaParser_MatchTokClass(statep, TOK_CLOSE_BRACKET))  {
        struct ast_node *item = _GaParser_ParseExpr(statep);
        
        if (!item) goto error;

        _Ga_list_unshift(items, item);

        if (!GaParser_AcceptTokClass(statep, TOK_COMMA)) {
            break;
        }
    }

    if (!GaParser_AcceptTokClass(statep, TOK_CLOSE_BRACKET)) {
        parser_expected_token_error(statep, "]");
        goto error;
    }

    return GaAst_NewList(items);

error:
    _Ga_list_destroy(items, _GaAst_ListDestroyCb, NULL);
    return NULL;
}

static struct ast_node *
parse_tuple(struct parser_state *statep, struct ast_node *first_item)
{
    _Ga_list_t *items = _Ga_list_new();

    _Ga_list_push(items, first_item);

    for (;;) {
        struct ast_node *expr = _GaParser_ParseExpr(statep);

        if (!expr) goto error;

        _Ga_list_push(items, expr);

        if (!GaParser_AcceptTokClass(statep, TOK_COMMA)) {
            break;
        }
    }

    if (!GaParser_AcceptTokClass(statep, TOK_RIGHT_PAREN)) {
        parser_expected_token_error(statep, ")");
        goto error;
    }

    return GaAst_NewTuple(items);

error:
    _Ga_list_destroy(items, _GaAst_ListDestroyCb, NULL);
    return NULL;
}

static struct ast_node *
parse_grouping(struct parser_state *statep)
{
    struct ast_node *expr = NULL;

    if (GaParser_AcceptTokClass(statep, TOK_LEFT_PAREN)) {
        expr = _GaParser_ParseExpr(statep);

        if (!expr) {
            parser_error(statep, "An expression was expected");
            goto error;
        }

        if (GaParser_AcceptTokClass(statep, TOK_COMMA)) {
            return parse_tuple(statep, expr);
        }

        if (!GaParser_AcceptTokClass(statep, TOK_RIGHT_PAREN)) {
            parser_expected_token_error(statep, ")");
            goto error;
        }

        return expr;
    }

    return parse_list_expr(statep);

error:
    if (expr) GaAst_Destroy(expr);
    return NULL;
}

static struct ast_node *
parse_index_expr(struct ast_node *left, struct parser_state *statep)
{
    struct ast_node *key = NULL;

    if (!left) return NULL;

    if (GaParser_AcceptTokClass(statep, TOK_OPEN_BRACKET)) {
        key = _GaParser_ParseExpr(statep);

        if (!key) {
            goto error;
        }

        if (!GaParser_AcceptTokClass(statep, TOK_CLOSE_BRACKET)) {
            parser_expected_token_error(statep, "]");
            goto error;
        }

        return GaAst_NewIndexer(left, key);
    }

    return left;

error:
    if (left) GaAst_Destroy(left);
    if (key) GaAst_Destroy(key);

    return NULL;
}

static struct ast_node *
parse_member_access(struct ast_node *left, struct parser_state *statep)
{
    if (!left) return NULL;

    GaParser_AcceptTokClass(statep, TOK_DOT);

    if (!GaParser_MatchTokClass(statep, TOK_IDENT)) {
        GaAst_Destroy(left);
        parser_error(statep, "An identifier was expected");
        return NULL;
    }

    struct token *tok = GaParser_ReadTok(statep);
    
    return GaAst_NewMemberAccess(left, STRINGBUF_VALUE(tok->sb));
}

static _Ga_list_t *
parse_arglist(struct parser_state *statep, int *call_flags)
{
    _Ga_list_t *args = _Ga_list_new();

    GaParser_ReadTok(statep);

    while (!GaParser_MatchTokClass(statep, TOK_RIGHT_PAREN)) {
        if (GaParser_AcceptTokClass(statep, TOK_MUL)) {
            *call_flags |= AST_CALL_PACKED;
        }

        struct ast_node *expr = _GaParser_ParseExpr(statep);

        if (!expr) {
            parser_error(statep, "An expression was expected");
            goto error;
        }

        _Ga_list_push(args, expr);

        if (!GaParser_MatchTokClass(statep, TOK_COMMA)) {
            break;
        }

        GaParser_ReadTok(statep);
    }

    if (!GaParser_AcceptTokClass(statep, TOK_RIGHT_PAREN)) {
        parser_expected_token_error(statep, ")");
        goto error;
    }

    return args;
error:
    if (args) _Ga_list_destroy(args, _GaAst_ListDestroyCb, NULL);
    return NULL;
}

static struct ast_node *
parse_call_macro_expr(struct ast_node *left, struct parser_state *statep)
{
    _Ga_list_t *token_list = _Ga_list_new();

    if (GaParser_MatchTokClass(statep, TOK_LEFT_PAREN)) {
        /* Keep parsing until you run out of parenthesis... */
        int parens = 0; /* How many parenthesis */
        do {
            struct token *tok = GaParser_ReadTok(statep);
            if (tok->type == TOK_RIGHT_PAREN) parens--;
            else if (tok->type == TOK_LEFT_PAREN) parens++;
            struct token *tok_copy = calloc(sizeof(struct token), 1);
            memcpy(tok_copy, tok, sizeof(struct token));
            _Ga_list_push(token_list, tok_copy);
        } while (parens > 0);
    }

    if (GaParser_MatchTokClass(statep, TOK_OPEN_BRACE)) {
        /* keep parsing until you hit a closing brace on the same indent line as the macro call */
        int indent_level = -1;
        struct token *tokens_start = GaParser_PeekTok(statep);
        
        do {
            struct token *tok = GaParser_ReadTok(statep);
            if (indent_level == -1 && tok->row != tokens_start->row) {
                indent_level = tok->col;
            }
            struct token *tok_copy = calloc(sizeof(struct token), 1);
            memcpy(tok_copy, tok, sizeof(struct token));
            _Ga_list_push(token_list, tok_copy);

            if (tok->type == TOK_CLOSE_BRACE && tok->col <= indent_level) {
                break;
            }
        } while (GaParser_PeekTok(statep));
    }
    return GaAst_NewMacro(left, token_list);
}

static struct ast_node *
parse_call_expr(struct ast_node *left, struct parser_state *statep)
{
    if (!left) return NULL;

    struct ast_node *ret = NULL;

    if (GaParser_AcceptTokClass(statep, TOK_BACKTICK)) {
        ret = parse_call_macro_expr(left, statep);
    } else if (GaParser_MatchTokClass(statep, TOK_LEFT_PAREN)) {
        int call_flags = 0;
        _Ga_list_t *params = parse_arglist(statep, &call_flags);
        if (!params) return NULL;
        ret = parse_call_expr(GaAst_NewCall(left, params, call_flags), statep);
    } else if (GaParser_MatchTokClass(statep, TOK_DOT)) {
        ret = parse_call_expr(parse_member_access(left, statep), statep);
    } else if (GaParser_MatchTokClass(statep, TOK_OPEN_BRACKET)) {
        ret = parse_call_expr(parse_index_expr(left, statep), statep);
    }

    if (ret || statep->error) return ret;
    else return left;
}

static struct ast_node *
parse_unary(struct parser_state *statep)
{
    unaryop_t op;

    if (GaParser_MatchTokClass(statep, TOK_LOGICAL_NOT)) {
        op = UNARYOP_LOGICAL_NOT;    
    } else if (GaParser_MatchTokClass(statep, TOK_NOT)) {
        op = UNARYOP_NOT;
    } else if (GaParser_MatchTokClass(statep, TOK_SUB)) {
        op = UNARYOP_NEGATE;
    } else {
        return parse_call_expr(parse_grouping(statep), statep);
    }

    GaParser_ReadTok(statep);

    struct ast_node *expr = parse_call_expr(parse_grouping(statep), statep);

    if (!expr) return NULL;

    return GaAst_NewUnaryOp(op, expr);
}

static struct ast_node *
parse_div_mul(struct parser_state *statep)
{
    struct ast_node *left = parse_unary(statep);

    if (!left) return NULL;

    while (GaParser_MatchTokClass(statep, TOK_DIV) ||
            GaParser_MatchTokClass(statep, TOK_MUL) ||
            GaParser_MatchTokClass(statep, TOK_MOD))
    {
        struct token *tok = GaParser_ReadTok(statep);
        struct ast_node *right = parse_unary(statep);
        binop_t op;

        if (!right) {
            GaAst_Destroy(left);
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

        left = GaAst_NewBinOp(op, left, right);
    }

    return left; 
}

static struct ast_node *
parse_add_sub(struct parser_state *statep)
{
    struct ast_node *left = parse_div_mul(statep);

    if (!left) return NULL;

    while (GaParser_MatchTokClass(statep, TOK_ADD) ||
           GaParser_MatchTokClass(statep, TOK_SUB))
    {
        struct token *tok = GaParser_ReadTok(statep);
        struct ast_node *right = parse_div_mul(statep);
        binop_t op;

        if (!right) {
            GaAst_Destroy(left);
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

        left = GaAst_NewBinOp(op, left, right);
    }

    return left;
}

static struct ast_node *
parse_bitshift(struct parser_state *statep)
{
    struct ast_node *left = parse_add_sub(statep);

    if (!left) return NULL;

    while (GaParser_MatchTokClass(statep, TOK_SHL) ||
           GaParser_MatchTokClass(statep, TOK_SHR))
    {
        struct token *tok = GaParser_ReadTok(statep);
        struct ast_node *right = parse_add_sub(statep);
        binop_t op;

        if (!right) {
            GaAst_Destroy(left);
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

        left = GaAst_NewBinOp(op, left, right);
    }

    return left;
}

static struct ast_node *
parse_relational(struct parser_state *statep)
{
    struct ast_node *left = parse_bitshift(statep);

    if (!left) return NULL;

    while (GaParser_MatchTokClass(statep, TOK_GREATER_THAN) ||
           GaParser_MatchTokClass(statep, TOK_LESS_THAN) ||
           GaParser_MatchTokClass(statep, TOK_GREATER_THAN_OR_EQU) ||
           GaParser_MatchTokClass(statep, TOK_LESS_THAN_OR_EQU))
    {
        struct token *tok = GaParser_ReadTok(statep);
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

        left = GaAst_NewBinOp(op, left, right);
    }

    return left;
}

static struct ast_node *
parse_equality(struct parser_state *statep)
{
    struct ast_node *left = parse_relational(statep);

    if (!left) return NULL;

    while (GaParser_MatchTokClass(statep, TOK_EQUALS) ||
           GaParser_MatchTokClass(statep, TOK_NOT_EQUALS))
    {
        struct token *tok = GaParser_ReadTok(statep);
        struct ast_node *right = parse_relational(statep);
        binop_t op;

        if (!right) {
            GaAst_Destroy(left);
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

        left = GaAst_NewBinOp(op, left, right);
    }

    return left;
}

static struct ast_node *
parse_and(struct parser_state *statep)
{
    struct ast_node *left = parse_equality(statep);

    if (!left) return NULL;

    while (GaParser_AcceptTokClass(statep, TOK_AND)) {
        struct ast_node *right = parse_equality(statep);

        if (!right) {
            GaAst_Destroy(left);
            return NULL; 
        }

        left = GaAst_NewBinOp(BINOP_AND, left, right);
    }

    return left;
}

static struct ast_node *
parse_xor(struct parser_state *statep)
{
    struct ast_node *left = parse_and(statep);

    if (!left) return NULL;

    while (GaParser_AcceptTokClass(statep, TOK_XOR)) {
        struct ast_node *right = parse_and(statep);

        if (!right) {
            GaAst_Destroy(left);
            return NULL;
        }

        left = GaAst_NewBinOp(BINOP_XOR, left, right);
    }

    return left;
}

static struct ast_node *
parse_or(struct parser_state *statep)
{
    struct ast_node *left = parse_xor(statep);

    if (!left) return NULL;

    while (GaParser_AcceptTokClass(statep, TOK_OR)) {
        struct ast_node *right = parse_xor(statep);

        if (!right) {
            GaAst_Destroy(left);
            return NULL;
        }

        left = GaAst_NewBinOp(BINOP_OR, left, right);
    }

    return left;
}

static struct ast_node *
parse_logical_and(struct parser_state *statep)
{
    struct ast_node *left = parse_or(statep);

    if (!left) return NULL;

    while (GaParser_AcceptTokClass(statep, TOK_LOGICAL_AND)) {
        struct ast_node *right = parse_or(statep);

        if (!right) {
            GaAst_Destroy(left);
            return NULL;
        }

        left = GaAst_NewBinOp(BINOP_LOGICAL_AND, left, right);
    }

    return left;
}

static struct ast_node *
parse_logical_or(struct parser_state *statep)
{
    struct ast_node *left = parse_logical_and(statep);

    if (!left) return NULL;

    while (GaParser_AcceptTokClass(statep, TOK_LOGICAL_OR)) {
        struct ast_node *right = parse_logical_and(statep);

        if (!right) {
            GaAst_Destroy(left);
            return NULL;
        }

        left = GaAst_NewBinOp(BINOP_LOGICAL_OR, left, right);
    }

    return left;
}

static struct ast_node *
parse_range(struct parser_state *statep)
{
    struct ast_node *left = parse_logical_or(statep);

    if (!left) return NULL;

    while (GaParser_MatchTokClass(statep, TOK_HALF_RANGE) ||
           GaParser_MatchTokClass(statep, TOK_CLOSED_RANGE))
    {
        struct token *tok = GaParser_ReadTok(statep);
        struct ast_node *right = parse_logical_or(statep);

        if (!right) {
            GaAst_Destroy(left);
        }

        switch (tok->type) {
            case TOK_HALF_RANGE:
                left = GaAst_NewBinOp(BINOP_HALF_RANGE, left, right);
                break;
            case TOK_CLOSED_RANGE:
                left = GaAst_NewBinOp(BINOP_CLOSED_RANGE, left, right);
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
    struct token *tok = GaParser_PeekTok(statep);

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

static struct ast_node *
parse_assign(struct parser_state *statep)
{
    struct ast_node *left = parse_range(statep);

    if (!left) return NULL;

    while (GaParser_MatchTokClass(statep, TOK_ASSIGN) ||
           match_inplace_assign(statep))
    {
        if (GaParser_AcceptTokClass(statep, TOK_ASSIGN)) {
            struct ast_node *right = parse_assign(statep);
            
            if (!right) {
                GaAst_Destroy(left);
                return NULL;
            }

            left = GaAst_NewExpr(left, right);
        } else {
            struct token *tok = GaParser_ReadTok(statep);
            
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
                GaAst_Destroy(left);
                return NULL;
            }

            left = GaAst_NewExpr(left, GaAst_NewBinOp(type, left, right));
        }
    }

    return left;
}

static struct ast_node *
parse_pattern_term(struct parser_state *statep)
{
    struct ast_node *expr = parse_call_expr(parse_grouping(statep), statep);

    if (!expr) return NULL;

    return expr;
}

static struct ast_node * parse_pattern(struct parser_state *);

static struct ast_node *
parse_pattern_collection(struct parser_state *statep)
{
    _Ga_list_t *items = _Ga_list_new();

    while (!GaParser_MatchTokClass(statep, TOK_CLOSE_BRACKET))  {
        struct ast_node *item = parse_pattern(statep);
        
        if (!item) goto error;

        _Ga_list_push(items, item);

        if (!GaParser_AcceptTokClass(statep, TOK_COMMA)) {
            break;
        }
    }

    if (!GaParser_AcceptTokClass(statep, TOK_CLOSE_BRACKET)) {
        parser_expected_token_error(statep, "]");
        goto error;
    }

    return GaAst_NewListPattern(items);
error:
    _Ga_list_destroy(items, _GaAst_ListDestroyCb, NULL);
    return NULL;
}

static struct ast_node *
parse_pattern_or(struct parser_state *statep)
{
    _Ga_list_t *items = _Ga_list_new();

    do {
        struct ast_node *item = parse_pattern(statep);

        if (!item) goto error;

        _Ga_list_push(items, item);        
    } while (GaParser_AcceptTokClass(statep, TOK_OR));

    if (!GaParser_AcceptTokClass(statep, TOK_RIGHT_PAREN)) {
        parser_expected_token_error(statep, ")");
        goto error;
    }

    return GaAst_NewOrPattern(items);
error:
    _Ga_list_destroy(items, _GaAst_ListDestroyCb, NULL);
    return NULL;
}


static struct ast_node *
parse_pattern(struct parser_state *statep)
{
    if (GaParser_AcceptTokClass(statep, TOK_LEFT_PAREN))
        return parse_pattern_or(statep);
    else if (GaParser_AcceptTokClass(statep, TOK_OPEN_BRACKET))
        return parse_pattern_collection(statep);
    else
        return parse_pattern_term(statep);
}

struct ast_node * _GaParser_ParseCodeBlock(struct parser_state *);

struct ast_node *
_GaParser_ParseMatch(struct parser_state *statep)
{
    if (!GaParser_AcceptTokVal(statep, TOK_KEYWORD, "match")) {
        return parse_assign(statep);
    }

    struct ast_node *expr = _GaParser_ParseExpr(statep);

    if (!expr) return NULL;

    _Ga_list_t *cases = _Ga_list_new();
    struct ast_node *pattern = NULL;
    struct ast_node *cond = NULL;
    struct ast_node *val = NULL;
    struct ast_node *default_case = NULL;

    if (!GaParser_AcceptTokClass(statep, TOK_OPEN_BRACE)) {
        parser_expected_token_error(statep, "{");
        goto error_1;
    }

    while (GaParser_AcceptTokVal(statep, TOK_KEYWORD, "case")) {
        pattern = parse_pattern(statep);

        if (!pattern) goto error_1;
        if (GaParser_AcceptTokVal(statep, TOK_KEYWORD, "when")) {
            cond = _GaParser_ParseExpr(statep);
            if (!cond) goto error_2;
        }

        if (GaParser_AcceptTokClass(statep, TOK_PHAT_ARROW))
            val = _GaParser_ParseExpr(statep);
        else
            val = _GaParser_ParseCodeBlock(statep);

        if (!val) goto error_3;
        _Ga_list_push(cases, GaAst_NewCase(pattern, cond, val));
    }

    if (GaParser_AcceptTokVal(statep, TOK_KEYWORD, "default")) {
        if (GaParser_AcceptTokClass(statep, TOK_PHAT_ARROW))
            default_case = _GaParser_ParseExpr(statep);
        else
            default_case = _GaParser_ParseCodeBlock(statep);

        if (!default_case) goto error_1;
    }

    if (!GaParser_AcceptTokClass(statep, TOK_CLOSE_BRACE)) {
        parser_expected_token_error(statep, "}");
        goto error_1;
    }
    return GaAst_NewMatch(expr, cases, default_case);
error_3:
    if (cond) GaAst_Destroy(cond);
error_2:
    if (pattern) GaAst_Destroy(pattern);
error_1:
    if (expr) GaAst_Destroy(expr);
    
    return NULL;
}

struct ast_node *
_GaParser_ParseWhen(struct parser_state *statep)
{
    struct ast_node *left = _GaParser_ParseMatch(statep);

    if (!GaParser_AcceptTokVal(statep, TOK_KEYWORD, "when")) {
        return left;
    }

    struct ast_node *cond = _GaParser_ParseExpr(statep);

    if (!cond) goto error;
    if (!GaParser_AcceptTokVal(statep, TOK_KEYWORD, "else")) goto error;

    struct ast_node *else_val = _GaParser_ParseExpr(statep);

    if (!else_val) goto error;

    return GaAst_NewWhen(left, cond, else_val);
error:
    if (left) GaAst_Destroy(left);
    if (cond) GaAst_Destroy(cond);
    return NULL;
}

struct ast_node *
_GaParser_ParseExpr(struct parser_state *statep)
{
    return _GaParser_ParseWhen(statep);
}

struct ast_node *   _GaParser_ParseDecl(struct parser_state *);
struct ast_node *   _GaParser_ParseStmt(struct parser_state *);

struct ast_node *
_GaParser_ParseCodeBlock(struct parser_state *statep)
{
    GaParser_ReadTok(statep);

    _Ga_list_t *children = _Ga_list_new();

    while (!GaParser_MatchTokClass(statep, TOK_CLOSE_BRACE) &&
            GaParser_PeekTok(statep) != NULL)
    {
        struct ast_node *node = _GaParser_ParseDecl(statep);
        if (!node) goto error;
        _Ga_list_push(children, node);
    }

    if (!GaParser_ReadTok(statep)) {
        parser_expected_token_error(statep, "}");
        goto error;
    }

    if (_Ga_LIST_COUNT(children) == 1) {
        struct ast_node *val = _Ga_list_head(children);
        _Ga_list_destroy(children, NULL, NULL);
        return val;
    } else {
        return GaAst_NewCodeBlock(children);
    }
error:
    _Ga_list_destroy(children, _GaAst_ListDestroyCb, NULL);
    return NULL;
}

struct ast_node *
_GaParser_ParseFor(struct parser_state *statep)
{
    GaParser_ReadTok(statep);

    if (!GaParser_MatchTokClass(statep, TOK_IDENT)) {
        parser_error(statep, "An identifier was expected");
        return NULL;
    }

    struct token *tok = GaParser_ReadTok(statep);

    if (!GaParser_AcceptTokVal(statep, TOK_KEYWORD, "in")) {
        parser_error(statep, "Keyword 'in' expected");
        return NULL;
    }

    struct ast_node *expr = _GaParser_ParseExpr(statep);

    if (!expr) {
        return NULL;
    }

    struct ast_node *body = _GaParser_ParseStmt(statep);

    if (!body) {
        GaAst_Destroy(expr);
        return NULL;
    }

    return GaAst_NewFor(STRINGBUF_VALUE(tok->sb), expr, body);
}

struct ast_node *
_GaParser_ParseIf(struct parser_state *statep)
{
    GaParser_ReadTok(statep);

    struct ast_node *cond = _GaParser_ParseExpr(statep);

    if (!cond) {
        return NULL;
    }

    struct ast_node *body = _GaParser_ParseStmt(statep);
    struct ast_node *else_body = NULL;

    if (!body) {
        goto error;
    }

    if (GaParser_MatchTokVal(statep, TOK_KEYWORD, "else")) {
        GaParser_ReadTok(statep);
        else_body = _GaParser_ParseStmt(statep);

        if (!else_body) {
            goto error;
        }
    }

    return GaAst_NewIf(cond, body, else_body);
    
error:
    if (cond) GaAst_Destroy(cond);
    if (body) GaAst_Destroy(body);
    if (else_body) GaAst_Destroy(else_body);
    return NULL;
}

struct ast_node *
GaParser_ParseReturn(struct parser_state *statep)
{
    GaParser_ReadTok(statep);

    struct ast_node *val = _GaParser_ParseExpr(statep);

    if (!val) {
        return NULL;
    }

    return GaAst_NewReturn(val);
}

struct ast_node *
_GaParser_ParseTry(struct parser_state *statep)
{
    GaParser_ReadTok(statep);

    struct ast_node *try_body = _GaParser_ParseStmt(statep);

    if (!try_body) return NULL;

    if (!GaParser_AcceptTokVal(statep, TOK_KEYWORD, "except")) {
        parser_error(statep, "Keyword 'expect' was expected");
        return NULL;
    }

    const char *varname = NULL;

    if (GaParser_MatchTokClass(statep, TOK_IDENT)) {
        struct token *tok = GaParser_ReadTok(statep);
        varname = STRINGBUF_VALUE(tok->sb);
    }

    struct ast_node *except_body = _GaParser_ParseStmt(statep);

    if (!except_body) goto error;

    return GaAst_NewTry(try_body, except_body, varname);

error:
    if (except_body) GaAst_Destroy(except_body);

    return NULL;
}

struct ast_node *
GaParser_ParseRaise(struct parser_state *statep)
{
    GaParser_ReadTok(statep);

    struct ast_node *val = _GaParser_ParseExpr(statep);

    if (!val) return NULL;

    return GaAst_NewRaise(val);
}

struct ast_node *
GaParser_ParseLet(struct parser_state *statep)
{
    GaParser_ReadTok(statep);

    if (!GaParser_MatchTokClass(statep, TOK_IDENT)) {
        parser_error(statep, "An identifier was expected");
        return NULL;
    }

    struct token *ident = GaParser_ReadTok(statep);

    if (!GaParser_AcceptTokClass(statep, TOK_ASSIGN)) {
        parser_expected_token_error(statep, "=");
        return NULL;
    }

    struct ast_node *val = _GaParser_ParseExpr(statep);

    if (!val) return NULL;

    return GaAst_NewLet(STRINGBUF_VALUE(ident->sb), val);
}

static bool
parse_module_path(struct parser_state *statep, char *import_path)
{
    int component = 0;

    if (GaParser_AcceptTokClass(statep, TOK_DOT)) {
        strncat(import_path, "./", PATH_MAX);
    }

    do {
        struct token *tok = GaParser_ReadTok(statep);

        if (!tok) {
            parser_eof_error(statep);
            return false;
        }

        if (tok->type != TOK_IDENT) {
            parser_error(statep, "An identifier was expected");
            return false;
        }
    
        if (component > 0) {
            strncat(import_path, "/", PATH_MAX);
        }

        strncat(import_path, STRINGBUF_VALUE(tok->sb), PATH_MAX);
        component++;
    } while (GaParser_AcceptTokClass(statep, TOK_DOT));

    return true;
}

struct ast_node *
_GaParser_ParseUse(struct parser_state *statep)
{
    bool wildcard = false;
    _Ga_list_t *imports = NULL;
    char import_path[PATH_MAX+1];

    import_path[0] = 0;

    if (!GaParser_AcceptTokVal(statep, TOK_KEYWORD, "use")) {
        parser_error(statep, "Keyword 'use' was expected");
        return NULL;
    }

    bool is_selective_import = GaParser_MatchTokClass(statep, TOK_IDENT) && (
                                   GaParser_MatchNTokClass(statep, 1, TOK_COMMA) ||
                                   GaParser_MatchNTokVal(statep, 1, TOK_KEYWORD, "from"));
    if (is_selective_import) {
        if (GaParser_AcceptTokClass(statep, TOK_MUL)) {
            wildcard = true;
        } else {
            imports = _Ga_list_new();

            do {
                struct token *tok = GaParser_ReadTok(statep);

                if (!tok) {
                    parser_eof_error(statep);
                    goto error; 
                }
                _Ga_list_push(imports, GaAst_NewSymbol(STRINGBUF_VALUE(tok->sb)));
            } while (GaParser_AcceptTokClass(statep, TOK_COMMA));
        }

        if (!GaParser_AcceptTokVal(statep, TOK_KEYWORD, "from")) {
            parser_error(statep, "Keyword 'from' was expected");
            goto error;
        }

    }

    parse_module_path(statep, import_path);

    return GaAst_NewUse(import_path, imports, wildcard);
    
error:
    _Ga_list_destroy(imports, _GaAst_ListDestroyCb, NULL);
    return NULL;
}

struct ast_node *
_GaParser_ParseWhile(struct parser_state *statep)
{
    GaParser_ReadTok(statep);

    struct ast_node *cond = _GaParser_ParseExpr(statep);

    if (!cond) {
        return NULL;
    }

    struct ast_node *body = _GaParser_ParseStmt(statep);
    
    if (!body) {
        GaAst_Destroy(cond);
        return NULL;
    }
    
    return GaAst_NewWhile(cond, body);
}

struct ast_node *
_GaParser_ParseWith(struct parser_state *statep)
{
    GaParser_ReadTok(statep);

    struct ast_node *expr = _GaParser_ParseExpr(statep);

    if (!expr) {
        return NULL;
    }

    struct ast_node *body = _GaParser_ParseStmt(statep);
    
    if (!body) {
        GaAst_Destroy(expr);
        return NULL;
    }
    
    return GaAst_NewWith(expr, body);
}

struct ast_node *
_GaParser_ParseStmt(struct parser_state *statep)
{
    if (GaParser_MatchTokClass(statep, TOK_SEMICOLON)) {
        while (GaParser_AcceptTokClass(statep, TOK_SEMICOLON));
        return &ast_empty_stmt_inst;
    }

    if (GaParser_MatchTokVal(statep, TOK_KEYWORD, "if")) {
        return _GaParser_ParseIf(statep);
    }

    if (GaParser_MatchTokVal(statep, TOK_KEYWORD, "return")) {
        return GaParser_ParseReturn(statep);
    }

    if (GaParser_MatchTokVal(statep, TOK_KEYWORD, "try")) {
        return _GaParser_ParseTry(statep);
    }

    if (GaParser_MatchTokVal(statep, TOK_KEYWORD, "use")) {
        return _GaParser_ParseUse(statep);
    }

    if (GaParser_MatchTokVal(statep, TOK_KEYWORD, "while")) {
        return _GaParser_ParseWhile(statep);
    }

    if (GaParser_MatchTokVal(statep, TOK_KEYWORD, "with")) {
        return _GaParser_ParseWith(statep);
    }

    if (GaParser_MatchTokVal(statep, TOK_KEYWORD, "for")) {
        return _GaParser_ParseFor(statep);
    }

    if (GaParser_AcceptTokVal(statep, TOK_KEYWORD, "break")) {
        return GaAst_NewBreak();
    }

    if (GaParser_AcceptTokVal(statep, TOK_KEYWORD, "continue")) {
        return GaAst_NewContinue();
    }

    if (GaParser_MatchTokVal(statep, TOK_KEYWORD, "raise")) {
        return GaParser_ParseRaise(statep);
    }

    if (GaParser_MatchTokVal(statep, TOK_KEYWORD, "let")) {
        return GaParser_ParseLet(statep);
    }

    if (GaParser_MatchTokClass(statep, TOK_OPEN_BRACE)) {
        return _GaParser_ParseCodeBlock(statep);        
    }

    return _GaParser_ParseExpr(statep);
}

struct ast_node *_GaParser_ParseDecl(struct parser_state *statep);

static struct ast_node *
parse_mixin_inclusion(struct parser_state *statep)
{
    if (!GaParser_AcceptTokVal(statep, TOK_KEYWORD, "use")) {
        parser_error(statep, "Keyword 'use' was expected");
        return NULL;
    }

    return _GaParser_ParseExpr(statep);
}

struct ast_node *
_GaParser_ParseClass(struct parser_state *statep)
{
    GaParser_ReadTok(statep);

    if (!GaParser_MatchTokClass(statep, TOK_IDENT)) {
        parser_error(statep, "An identifier was expected");
        return NULL;
    }

    struct token *class_name = GaParser_ReadTok(statep);
    struct ast_node *base = NULL;

    if (GaParser_AcceptTokVal(statep, TOK_KEYWORD, "extends")) {
        base = _GaParser_ParseExpr(statep);

        if (!base) return NULL;
    }

    if (!GaParser_AcceptTokClass(statep, TOK_OPEN_BRACE)) {
        parser_expected_token_error(statep, "{");
        return NULL;
    }

    _Ga_list_t *mixins = _Ga_list_new();
    _Ga_list_t *methods = _Ga_list_new();

    while (!GaParser_AcceptTokClass(statep, TOK_CLOSE_BRACE)) {
        struct ast_node *member;

        if (GaParser_MatchTokVal(statep, TOK_KEYWORD, "use")) {
            member = parse_mixin_inclusion(statep);

            if (!member) goto error;

            _Ga_list_push(mixins, member);
        } else {
            member = _GaParser_ParseDecl(statep);

            if (!member) goto error;

            _Ga_list_push(methods, member);
        }
    }
   
    return GaAst_NewClass(STRINGBUF_VALUE(class_name->sb), base, mixins, methods);

error:
    if (base) GaAst_Destroy(base);
    _Ga_list_destroy(methods, _GaAst_ListDestroyCb, NULL);
    return NULL;
}

struct ast_node *
_GaParser_ParseEnum(struct parser_state *statep)
{
    GaParser_ReadTok(statep);

    if (!GaParser_MatchTokClass(statep, TOK_IDENT)) {
        parser_error(statep, "An identifier was expected");
        return NULL;
    }

    struct token *enum_name = GaParser_ReadTok(statep);

    if (!GaParser_AcceptTokClass(statep, TOK_OPEN_BRACE)) {
        parser_expected_token_error(statep, "{");
        return NULL;
    }

    _Ga_list_t *values = _Ga_list_new();

    do {
        struct token *tok = GaParser_ReadTok(statep);

        if (tok->type != TOK_IDENT) {
            parser_error(statep, "An identifier was expected");
            goto error;
        }

        _Ga_list_push(values, GaAst_NewSymbol(STRINGBUF_VALUE(tok->sb)));
    } while (GaParser_AcceptTokClass(statep, TOK_COMMA));

    if (!GaParser_AcceptTokClass(statep, TOK_CLOSE_BRACE)) {
        parser_expected_token_error(statep, "}");
        goto error;
    }
   
    return GaAst_NewEnum(STRINGBUF_VALUE(enum_name->sb), values);
error:
    _Ga_list_destroy(values, _GaAst_ListDestroyCb, NULL);
    return NULL;
}


struct ast_node *
_GaParser_ParseMixin(struct parser_state *statep)
{
    GaParser_ReadTok(statep);

    if (!GaParser_MatchTokClass(statep, TOK_IDENT)) {
        parser_error(statep, "An identifier was expected");
        return NULL;
    }

    struct token *mixin_name = GaParser_ReadTok(statep);

    if (!GaParser_AcceptTokClass(statep, TOK_OPEN_BRACE)) {
        parser_expected_token_error(statep, "{");
        return NULL;
    }

    _Ga_list_t *members = _Ga_list_new();

    while (!GaParser_AcceptTokClass(statep, TOK_CLOSE_BRACE)) {
        struct ast_node *member = _GaParser_ParseDecl(statep);

        if (!member) goto error;

        _Ga_list_push(members, member);
    }

    return GaAst_NewMixin(STRINGBUF_VALUE(mixin_name->sb), members);
error:
    _Ga_list_destroy(members, _GaAst_ListDestroyCb, NULL);
    return NULL;
}

struct ast_node *
_GaParser_ParseFunc(struct parser_state *statep, bool is_expr)
{
    GaParser_ReadTok(statep);

    const char *func_name = NULL;

    if (!is_expr) {
        if (!GaParser_MatchTokClass(statep, TOK_IDENT)) {
            parser_error(statep, "An identifier was expected");
            return NULL;
        }

        struct token *tok = GaParser_ReadTok(statep);
        func_name = STRINGBUF_VALUE(tok->sb);
    }

    if (!GaParser_MatchTokClass(statep, TOK_LEFT_PAREN)) {
        parser_expected_token_error(statep, "(");
        return NULL;
    }
    
    GaParser_ReadTok(statep);

    _Ga_list_t *params = _Ga_list_new();
    _Ga_list_t *statements = _Ga_list_new();

    bool is_variadic = false;

    while (!GaParser_MatchTokClass(statep, TOK_RIGHT_PAREN)) {
        int flags = 0;
        if (is_variadic) {
            parser_error(statep, "A variable argument parameter must be last");
            goto error;
        }

        if (GaParser_AcceptTokClass(statep, TOK_MUL)) {
            is_variadic = true;
            flags = AST_FUNC_VARIADIC;
        }

        if (!GaParser_MatchTokClass(statep, TOK_IDENT)) {
            goto error;
        }

        struct token *func_param = GaParser_ReadTok(statep);
        const char *name = STRINGBUF_VALUE(func_param->sb);
        _Ga_list_push(params, GaAst_NewFuncParam(name, flags));

        if (!GaParser_MatchTokClass(statep, TOK_COMMA)) {
            break;
        }
        GaParser_ReadTok(statep);
    }

    if (!GaParser_MatchTokClass(statep, TOK_RIGHT_PAREN)) {
        goto error;
    }

    GaParser_ReadTok(statep);

    if (GaParser_AcceptTokClass(statep, TOK_OPEN_BRACE)) {
        while (!GaParser_MatchTokClass(statep, TOK_CLOSE_BRACE) &&
                GaParser_PeekTok(statep) != NULL)
        {
            struct ast_node *node = _GaParser_ParseDecl(statep);

            if (!node) goto error;

            _Ga_list_push(statements, node);
        }

        if (!GaParser_ReadTok(statep)) {
            parser_expected_token_error(statep, "}");
            goto error;
        }
    } else if (GaParser_AcceptTokClass(statep, TOK_PHAT_ARROW)) {
        struct ast_node *expr_body = _GaParser_ParseExpr(statep);

        if (!expr_body) goto error;

        _Ga_list_push(statements, GaAst_NewReturn(expr_body));
    }

    if (is_expr) {
        return GaAst_NewAnonymousFunc(params, GaAst_NewCodeBlock(statements));
    }

    return GaAst_NewFunc(func_name, params, GaAst_NewCodeBlock(statements));

error:
    _Ga_list_destroy(params, _GaAst_ListDestroyCb, NULL);
    _Ga_list_destroy(statements, _GaAst_ListDestroyCb, NULL);
    return NULL;
}

struct ast_node *
_GaParser_ParseDecl(struct parser_state  *statep)
{
    if (GaParser_MatchTokVal(statep, TOK_KEYWORD, "macro")) {
        return _GaParser_ParseFunc(statep, false);
    }

    if (GaParser_MatchTokVal(statep, TOK_KEYWORD, "func")) {
        return _GaParser_ParseFunc(statep, false);
    }

    if (GaParser_MatchTokVal(statep, TOK_KEYWORD, "class")) {
        return _GaParser_ParseClass(statep);
    }

    if (GaParser_MatchTokVal(statep, TOK_KEYWORD, "enum")) {
        return _GaParser_ParseEnum(statep);
    }

    if (GaParser_MatchTokVal(statep, TOK_KEYWORD, "mixin")) {
        return _GaParser_ParseMixin(statep);
    }

    return _GaParser_ParseStmt(statep);
}
/*
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
            fprintf(stderr, "unknown error %d during lexical analysis\n",
                    statep->lex_state.lex_errno);
            break;
    }
    
    fputs("\n", stderr);
}*/

void
GaParser_InitLazy(struct parser_state *statep, _Ga_list_t *tokens)
{
    memset(statep, 0, sizeof(struct parser_state));
    _Ga_list_get_iter(tokens, &statep->iter);
}

struct ast_node *
GaParser_ParseString(struct parser_state *statep, const char *src)
{
    memset(statep, 0, sizeof(struct parser_state));

    _GaLexer_Init(&statep->lex_state);
    _GaLexer_ScanStr(&statep->lex_state, src);

    if (statep->lex_state.error) {
        statep->error = statep->lex_state.error;
        return NULL;
    }

    _Ga_list_get_iter(statep->lex_state.tokens, &statep->iter);
    
    return GaParser_ParseAll(statep);
}

struct ast_node *
GaParser_ParseAll(struct parser_state *statep)
{
    _Ga_list_t *nodes = _Ga_list_new();

    while (GaParser_PeekTok(statep)) {
        struct ast_node *node = _GaParser_ParseDecl(statep);

        if (!node) {
            _Ga_list_destroy(nodes, _GaAst_ListDestroyCb, NULL);
            return NULL;
        }

        _Ga_list_push(nodes, node);
    }

    return GaAst_NewCodeBlock(nodes);
}

void
GaParser_Fini(struct parser_state *statep)
{
    _GaLexer_Fini(&statep->lex_state);
}