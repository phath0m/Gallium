/*
 * parser.c - Gallium module for parsing Gallium code
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
#include <stdio.h>
#include <stdlib.h>
#include <gallium/builtins.h>
#include <gallium/list.h>
#include <gallium/object.h>
#include <gallium/parser.h>
#include <gallium/vm.h>

GA_BUILTIN_TYPE_DECL(ga_token_type_inst, "Token", NULL);
GA_BUILTIN_TYPE_DECL(ga_tokenstream_type_inst, "Tokenstream", NULL);

struct tokenstream_state {
    struct parser_state     parser_state;
    struct list         *   tokens;
};

static struct ga_obj *
ga_token_new(struct token *tok)
{
    struct ga_obj *obj = ga_obj_new(&ga_token_type_inst, NULL);

    GAOBJ_SETATTR(obj, NULL, "type", GA_INT_FROM_I64(tok->type));
    if (tok->sb)
    GAOBJ_SETATTR(obj, NULL, "value", ga_str_from_stringbuf(tok->sb));

    return obj;
}

static struct ga_obj *
tokenstream_accept_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc < 1) {
        vm_raise_exception(vm, ga_argument_error_new("accept() requires at-least one argument"));
        return NULL;
    }

    struct ga_obj *int_arg = ga_obj_super(args[0], &ga_int_type_inst);

    if (!int_arg) {
        vm_raise_exception(vm, ga_type_error_new("Int"));
        return NULL;
    }

    struct tokenstream_state *statep = self->un.statep;

    token_class_t kind = GA_INT_TO_I64(int_arg);

    if (argc == 1) 
        return GA_BOOL_FROM_BOOL(parser_accept_tok_class(&statep->parser_state, kind));
    else {
        struct ga_obj *str = GAOBJ_INC_REF(GAOBJ_STR(args[1], vm));
        struct ga_obj *res = GA_BOOL_FROM_BOOL(parser_accept_tok_val(&statep->parser_state, kind, ga_str_to_cstring(str)));
        GAOBJ_DEC_REF(str);
        return res;
    }
}

static struct ga_obj *
tokenstream_expect_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc < 1) {
        vm_raise_exception(vm, ga_argument_error_new("expect() requires at-least one argument"));
        return NULL;
    }

    struct ga_obj *int_arg = ga_obj_super(args[0], &ga_int_type_inst);

    if (!int_arg) {
        vm_raise_exception(vm, ga_type_error_new("Int"));
        return NULL;
    }

    struct tokenstream_state *statep = self->un.statep;

    token_class_t kind = GA_INT_TO_I64(int_arg);

    if (argc == 1 && !parser_accept_tok_class(&statep->parser_state, kind)) {
        vm_raise_exception(vm, ga_syntax_error_new("Unexpected token!"));
    } else if (argc == 2) {
        struct ga_obj *str = GAOBJ_INC_REF(GAOBJ_STR(args[1], vm));
        bool res = parser_accept_tok_val(&statep->parser_state, kind, ga_str_to_cstring(str));
        GAOBJ_DEC_REF(str);
        if (!res) vm_raise_exception(vm, ga_syntax_error_new("Unexpected token!"));
    }
    return GA_NULL;
}

static struct ga_obj *
tokenstream_empty_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 0) {
        vm_raise_exception(vm, ga_argument_error_new("empty() requires zero arguments"));
        return NULL;
    }

    struct tokenstream_state *statep = self->un.statep;
    return GA_BOOL_FROM_BOOL(parser_peek_tok(&statep->parser_state) == NULL);
}

static struct ga_obj *
tokenstream_parse_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 0) {
        vm_raise_exception(vm, ga_argument_error_new("parse() requires zero arguments"));
        return NULL;
    }

    struct tokenstream_state *statep = self->un.statep;
    struct ast_node *node = parser_parse_all(&statep->parser_state);

    if (!node) {
        printf("syntax error\n");
        parser_explain(&statep->parser_state);
        /* raise syntax exception */
        return NULL;
    }

    return ga_ast_node_new(node, NULL);
}

static struct ga_obj *
tokenstream_parse_expr_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 0) {
        vm_raise_exception(vm, ga_argument_error_new("parse_expr() requires zero arguments"));
        return NULL;
    }

    struct tokenstream_state *statep = self->un.statep;
    struct ast_node *node = parser_parse_expr(&statep->parser_state);

    if (!node) {
        vm_raise_exception(vm, ga_syntax_error_new("Error while parsing expression!"));
        return NULL;
    }

    return ga_ast_node_new(node, NULL);
}

static struct ga_obj *
tokenstream_parse_stmt_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 0) {
        vm_raise_exception(vm, ga_argument_error_new("parse_stmt() requires zero arguments"));
        return NULL;
    }

    struct tokenstream_state *statep = self->un.statep;
    struct ast_node *node = parser_parse_stmt(&statep->parser_state);

    if (!node) {
        vm_raise_exception(vm, ga_syntax_error_new("Error while parsing statement!"));
        return NULL;
    }

    return ga_ast_node_new(node, NULL);
}

static struct ga_obj *
tokenstream_parse_ident_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 0) {
        vm_raise_exception(vm, ga_argument_error_new("ident() requires zero arguments"));
        return NULL;
    }

    struct tokenstream_state *statep = self->un.statep;
    struct token *tok = parser_read_tok(&statep->parser_state);

    if (!tok || tok->type != TOK_IDENT) {
        vm_raise_exception(vm, ga_syntax_error_new("Expected identifier!"));
        return NULL;
    }
    
    return ga_ast_node_new(symbol_term_new(STRINGBUF_VALUE(tok->sb)), NULL);
}

static struct ga_obj *
tokenstream_read_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 0) {
        vm_raise_exception(vm, ga_argument_error_new("read() requires zero arguments"));
        return NULL;
    }

    struct tokenstream_state *statep = self->un.statep;
    struct token *tok = parser_read_tok(&statep->parser_state);

    return ga_token_new(tok);
}

struct ga_obj *
ga_tokenstream_new(struct list *tokens)
{
    struct ga_obj *obj = ga_obj_new(&ga_tokenstream_type_inst, NULL);
    struct tokenstream_state *statep = calloc(sizeof(struct tokenstream_state), 1);
    
    statep->tokens = tokens;
    obj->un.statep = statep;

    parser_init_lazy(&statep->parser_state, tokens);

    GAOBJ_SETATTR(obj, NULL, "parse", ga_builtin_new(tokenstream_parse_method, obj));
    GAOBJ_SETATTR(obj, NULL, "expr", ga_builtin_new(tokenstream_parse_expr_method, obj));
    GAOBJ_SETATTR(obj, NULL, "stmt", ga_builtin_new(tokenstream_parse_stmt_method, obj));
    GAOBJ_SETATTR(obj, NULL, "accept", ga_builtin_new(tokenstream_accept_method, obj));
    GAOBJ_SETATTR(obj, NULL, "expect", ga_builtin_new(tokenstream_expect_method, obj));
    GAOBJ_SETATTR(obj, NULL, "empty", ga_builtin_new(tokenstream_empty_method, obj));
    GAOBJ_SETATTR(obj, NULL, "read", ga_builtin_new(tokenstream_read_method, obj));
    GAOBJ_SETATTR(obj, NULL, "ident", ga_builtin_new(tokenstream_parse_ident_method, obj));

    return obj;
}

struct ga_obj *
ga_parser_mod_open()
{
    static struct ga_obj *mod = NULL;

    if (mod) {
        return mod;
    }

    mod = ga_mod_new("parser", NULL, NULL);

    GAOBJ_SETATTR(mod, NULL, "TOK_KEYWORD", GA_INT_FROM_I64((int64_t)TOK_KEYWORD));
    GAOBJ_SETATTR(mod, NULL, "TOK_IDENT", GA_INT_FROM_I64((int64_t)TOK_IDENT));
    GAOBJ_SETATTR(mod, NULL, "TOK_AND", GA_INT_FROM_I64((int64_t)TOK_AND));
    GAOBJ_SETATTR(mod, NULL, "TOK_OR", GA_INT_FROM_I64((int64_t)TOK_OR));
    GAOBJ_SETATTR(mod, NULL, "TOK_XOR", GA_INT_FROM_I64((int64_t)TOK_XOR));
    GAOBJ_SETATTR(mod, NULL, "TOK_ADD", GA_INT_FROM_I64((int64_t)TOK_ADD));
    GAOBJ_SETATTR(mod, NULL, "TOK_SUB", GA_INT_FROM_I64((int64_t)TOK_SUB));
    GAOBJ_SETATTR(mod, NULL, "TOK_MUL", GA_INT_FROM_I64((int64_t)TOK_MUL));
    GAOBJ_SETATTR(mod, NULL, "TOK_DIV", GA_INT_FROM_I64((int64_t)TOK_DIV));
    GAOBJ_SETATTR(mod, NULL, "TOK_MOD", GA_INT_FROM_I64((int64_t)TOK_MOD));
    GAOBJ_SETATTR(mod, NULL, "TOK_ASSIGN", GA_INT_FROM_I64((int64_t)TOK_ASSIGN));
    GAOBJ_SETATTR(mod, NULL, "TOK_EQUALS", GA_INT_FROM_I64((int64_t)TOK_EQUALS));
    GAOBJ_SETATTR(mod, NULL, "TOK_NOT_EQUALS", GA_INT_FROM_I64((int64_t)TOK_NOT_EQUALS));
    GAOBJ_SETATTR(mod, NULL, "TOK_GT", GA_INT_FROM_I64((int64_t)TOK_GREATER_THAN));
    GAOBJ_SETATTR(mod, NULL, "TOK_LT", GA_INT_FROM_I64((int64_t)TOK_LESS_THAN));
    GAOBJ_SETATTR(mod, NULL, "TOK_GE", GA_INT_FROM_I64((int64_t)TOK_GREATER_THAN_OR_EQU));
    GAOBJ_SETATTR(mod, NULL, "TOK_LE", GA_INT_FROM_I64((int64_t)TOK_LESS_THAN_OR_EQU));
    GAOBJ_SETATTR(mod, NULL, "TOK_LOGICAL_AND", GA_INT_FROM_I64((int64_t)TOK_LOGICAL_AND));
    GAOBJ_SETATTR(mod, NULL, "TOK_LOGICAL_OR", GA_INT_FROM_I64((int64_t)TOK_LOGICAL_OR));
    GAOBJ_SETATTR(mod, NULL, "TOK_LOGICAL_NOT", GA_INT_FROM_I64((int64_t)TOK_LOGICAL_NOT));
    GAOBJ_SETATTR(mod, NULL, "TOK_NOT", GA_INT_FROM_I64((int64_t)TOK_NOT));
    GAOBJ_SETATTR(mod, NULL, "TOK_DOT", GA_INT_FROM_I64((int64_t)TOK_DOT));
    GAOBJ_SETATTR(mod, NULL, "TOK_COMMA", GA_INT_FROM_I64((int64_t)TOK_COMMA));
    GAOBJ_SETATTR(mod, NULL, "TOK_STRING", GA_INT_FROM_I64((int64_t)TOK_STRING_LIT));
    GAOBJ_SETATTR(mod, NULL, "TOK_INT", GA_INT_FROM_I64((int64_t)TOK_INT_LIT));
    GAOBJ_SETATTR(mod, NULL, "TOK_PHAT_ARROW", GA_INT_FROM_I64((int64_t)TOK_PHAT_ARROW));
    GAOBJ_SETATTR(mod, NULL, "TOK_CLOSED_RANGE", GA_INT_FROM_I64((int64_t)TOK_CLOSED_RANGE));
    GAOBJ_SETATTR(mod, NULL, "TOK_HALF_RANGE", GA_INT_FROM_I64((int64_t)TOK_HALF_RANGE));
    GAOBJ_SETATTR(mod, NULL, "TOK_SHL", GA_INT_FROM_I64((int64_t)TOK_SHL));
    GAOBJ_SETATTR(mod, NULL, "TOK_SHR", GA_INT_FROM_I64((int64_t)TOK_SHR));
    GAOBJ_SETATTR(mod, NULL, "TOK_BACKTICK", GA_INT_FROM_I64((int64_t)TOK_BACKTICK));
    GAOBJ_SETATTR(mod, NULL, "TOK_OPEN_BRACKET", GA_INT_FROM_I64((int64_t)TOK_OPEN_BRACKET));
    GAOBJ_SETATTR(mod, NULL, "TOK_CLOSE_BRACKET", GA_INT_FROM_I64((int64_t)TOK_CLOSE_BRACKET));
    GAOBJ_SETATTR(mod, NULL, "TOK_LEFT_PAREN", GA_INT_FROM_I64((int64_t)TOK_LEFT_PAREN));
    GAOBJ_SETATTR(mod, NULL, "TOK_RIGHT_PAREN", GA_INT_FROM_I64((int64_t)TOK_RIGHT_PAREN));

    return mod;
}