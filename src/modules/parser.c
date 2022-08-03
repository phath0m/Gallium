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
#include <gallium/vm.h>
#include "../parser.h"

GaObject * _GaParser_type = NULL;

GA_BUILTIN_TYPE_DECL(ga_token_type_inst, "Token", NULL);

struct tokenstream_state {
    struct parser_state     parser_state;
    _Ga_list_t         *   tokens;
};

static GaObject *
ga_token_new(struct token *tok)
{
    GaObject *obj = GaObj_New(&ga_token_type_inst, NULL);

    GaObj_SETATTR(obj, NULL, "type", GaInt_FROM_I64(tok->type));
    if (tok->sb)
    GaObj_SETATTR(obj, NULL, "value", GaStr_FromStringBuilder(tok->sb));

    return obj;
}

static GaObject *
tokenstream_accept_method(GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_OPTIONAL(vm, 2, (GaObject*[]){ GA_PARSER_TYPE,
                                GA_INT_TYPE, GA_STR_TYPE }, argc, args))
    {
        return NULL;
    }

    GaObject *self = GaObj_Super(args[0], GA_PARSER_TYPE);
    GaObject *int_arg = GaObj_Super(args[1], &_GaInt_Type);

    struct tokenstream_state *statep = self->un.statep;

    token_class_t kind = GaInt_TO_I64(int_arg);

    if (argc == 2) 
        return GaBool_FROM_BOOL(GaParser_AcceptTokClass(&statep->parser_state, kind));
    else {
        GaObject *str = GaObj_INC_REF(GaObj_STR(args[2], vm));
        GaObject *res = GaBool_FROM_BOOL(GaParser_AcceptTokVal(&statep->parser_state, kind, GaStr_ToCString(str)));
        GaObj_DEC_REF(str);
        return res;
    }
}

static GaObject *
tokenstream_expect_method(GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_OPTIONAL(vm, 2, (GaObject*[]){ GA_PARSER_TYPE,
                                GA_INT_TYPE, GA_STR_TYPE }, argc, args))
    {
        return NULL;
    }

    GaObject *self = GaObj_Super(args[0], GA_PARSER_TYPE);
    GaObject *int_arg = GaObj_Super(args[1], GA_INT_TYPE);
    struct tokenstream_state *statep = self->un.statep;
    token_class_t kind = GaInt_TO_I64(int_arg);

    if (argc == 2 && !GaParser_AcceptTokClass(&statep->parser_state, kind)) {
        GaEval_RaiseException(vm, GaErr_NewSyntaxError("Unexpected token!"));
    } else if (argc == 3) {
        GaObject *str = GaObj_INC_REF(GaObj_STR(args[2], vm));
        bool res = GaParser_AcceptTokVal(&statep->parser_state, kind, GaStr_ToCString(str));
        GaObj_DEC_REF(str);
        if (!res) GaEval_RaiseException(vm, GaErr_NewSyntaxError("Unexpected token!"));
    }
    return Ga_NULL;
}

static GaObject *
tokenstream_empty_method(GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_EXACT(vm, 1, (GaObject*[]){ GA_PARSER_TYPE }, argc,
                             args))
    {
        return NULL;
    }

    GaObject *self = GaObj_Super(args[0], GA_PARSER_TYPE);
    struct tokenstream_state *statep = self->un.statep;

    return GaBool_FROM_BOOL(GaParser_PeekTok(&statep->parser_state) == NULL);
}

static GaObject *
tokenstream_parse_method(GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_EXACT(vm, 1, (GaObject*[]){ GA_PARSER_TYPE }, argc,
                             args))
    {
        return NULL;
    }

    GaObject *self = GaObj_Super(args[0], GA_PARSER_TYPE);
    struct tokenstream_state *statep = self->un.statep;
    struct ast_node *node = GaParser_ParseAll(&statep->parser_state);

    if (!node) {
        printf("syntax error\n");
        //GaParser_Explain(&statep->parser_state);
        /* raise syntax exception */
        return NULL;
    }

    return GaAstNode_New(node, NULL);
}

static GaObject *
tokenstream_parse_expr_method(GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_EXACT(vm, 1, (GaObject*[]){ GA_PARSER_TYPE }, argc,
                             args))
    {
        return NULL;
    }

    GaObject *self = GaObj_Super(args[0], GA_PARSER_TYPE);
    struct tokenstream_state *statep = self->un.statep;
    struct ast_node *node = _GaParser_ParseExpr(&statep->parser_state);

    if (!node) {
        GaEval_RaiseException(vm, GaErr_NewSyntaxError("Error while parsing expression!"));
        return NULL;
    }

    return GaAstNode_New(node, NULL);
}

static GaObject *
tokenstream_parse_stmt_method(GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_EXACT(vm, 1, (GaObject*[]){ GA_PARSER_TYPE }, argc,
                             args))
    {
        return NULL;
    }

    GaObject *self = GaObj_Super(args[0], GA_PARSER_TYPE);
    struct tokenstream_state *statep = self->un.statep;
    struct ast_node *node = _GaParser_ParseStmt(&statep->parser_state);

    if (!node) {
        GaEval_RaiseException(vm, GaErr_NewSyntaxError("Error while parsing statement!"));
        return NULL;
    }

    return GaAstNode_New(node, NULL);
}

static GaObject *
tokenstream_parse_ident_method(GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_EXACT(vm, 1, (GaObject*[]){ GA_PARSER_TYPE }, argc,
                             args))
    {
        return NULL;
    }

    GaObject *self = GaObj_Super(args[0], GA_PARSER_TYPE);
    struct tokenstream_state *statep = self->un.statep;
    struct token *tok = GaParser_ReadTok(&statep->parser_state);

    if (!tok || tok->type != TOK_IDENT) {
        GaEval_RaiseException(vm, GaErr_NewSyntaxError("Expected identifier!"));
        return NULL;
    }
    
    return GaAstNode_New(GaAst_NewSymbol(STRINGBUF_VALUE(tok->sb)), NULL);
}

static GaObject *
tokenstream_read_method(GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_EXACT(vm, 1, (GaObject*[]){ GA_PARSER_TYPE }, argc,
                             args))
    {
        return NULL;
    }

    GaObject *self = GaObj_Super(args[0], GA_PARSER_TYPE);
    struct tokenstream_state *statep = self->un.statep;
    struct token *tok = GaParser_ReadTok(&statep->parser_state);

    return ga_token_new(tok);
}

static void
assign_methods(GaObject *target, GaObject *self)
{
    GaObj_SETATTR(target, NULL, "parse", GaBuiltin_New(tokenstream_parse_method, self));
    GaObj_SETATTR(target, NULL, "expr", GaBuiltin_New(tokenstream_parse_expr_method, self));
    GaObj_SETATTR(target, NULL, "stmt", GaBuiltin_New(tokenstream_parse_stmt_method, self));
    GaObj_SETATTR(target, NULL, "accept", GaBuiltin_New(tokenstream_accept_method, self));
    GaObj_SETATTR(target, NULL, "expect", GaBuiltin_New(tokenstream_expect_method, self));
    GaObj_SETATTR(target, NULL, "empty", GaBuiltin_New(tokenstream_empty_method, self));
    GaObj_SETATTR(target, NULL, "read", GaBuiltin_New(tokenstream_read_method, self));
    GaObj_SETATTR(target, NULL, "ident", GaBuiltin_New(tokenstream_parse_ident_method, self));
}

GaObject *
_GaParser_init()
{
    _GaParser_type = GaObj_NewType("Parser", NULL);
    assign_methods(_GaParser_type, NULL);
    return GaObj_INC_REF(_GaParser_type);
}

void
_GaParser_fini()
{
    GaObj_XDEC_REF(_GaParser_type);
}

GaObject *
ga_tokenstream_new(_Ga_list_t *tokens)
{
    GaObject *obj = GaObj_New(GA_PARSER_TYPE, NULL);
    struct tokenstream_state *statep = calloc(sizeof(struct tokenstream_state), 1);
    
    statep->tokens = tokens;
    obj->un.statep = statep;

    GaParser_InitLazy(&statep->parser_state, tokens);
    assign_methods(obj, obj);

    return obj;
}

GaObject *
GaMod_OpenParser()
{
    static GaObject *mod = NULL;

    if (mod) {
        return mod;
    }

    mod = GaModule_New("parser", NULL, NULL);

    GaObj_SETATTR(mod, NULL, "TOK_KEYWORD", GaInt_FROM_I64((int64_t)TOK_KEYWORD));
    GaObj_SETATTR(mod, NULL, "TOK_IDENT", GaInt_FROM_I64((int64_t)TOK_IDENT));
    GaObj_SETATTR(mod, NULL, "TOK_AND", GaInt_FROM_I64((int64_t)TOK_AND));
    GaObj_SETATTR(mod, NULL, "TOK_OR", GaInt_FROM_I64((int64_t)TOK_OR));
    GaObj_SETATTR(mod, NULL, "TOK_XOR", GaInt_FROM_I64((int64_t)TOK_XOR));
    GaObj_SETATTR(mod, NULL, "TOK_ADD", GaInt_FROM_I64((int64_t)TOK_ADD));
    GaObj_SETATTR(mod, NULL, "TOK_SUB", GaInt_FROM_I64((int64_t)TOK_SUB));
    GaObj_SETATTR(mod, NULL, "TOK_MUL", GaInt_FROM_I64((int64_t)TOK_MUL));
    GaObj_SETATTR(mod, NULL, "TOK_DIV", GaInt_FROM_I64((int64_t)TOK_DIV));
    GaObj_SETATTR(mod, NULL, "TOK_MOD", GaInt_FROM_I64((int64_t)TOK_MOD));
    GaObj_SETATTR(mod, NULL, "TOK_ASSIGN", GaInt_FROM_I64((int64_t)TOK_ASSIGN));
    GaObj_SETATTR(mod, NULL, "TOK_EQUALS", GaInt_FROM_I64((int64_t)TOK_EQUALS));
    GaObj_SETATTR(mod, NULL, "TOK_NOT_EQUALS", GaInt_FROM_I64((int64_t)TOK_NOT_EQUALS));
    GaObj_SETATTR(mod, NULL, "TOK_GT", GaInt_FROM_I64((int64_t)TOK_GREATER_THAN));
    GaObj_SETATTR(mod, NULL, "TOK_LT", GaInt_FROM_I64((int64_t)TOK_LESS_THAN));
    GaObj_SETATTR(mod, NULL, "TOK_GE", GaInt_FROM_I64((int64_t)TOK_GREATER_THAN_OR_EQU));
    GaObj_SETATTR(mod, NULL, "TOK_LE", GaInt_FROM_I64((int64_t)TOK_LESS_THAN_OR_EQU));
    GaObj_SETATTR(mod, NULL, "TOK_LOGICAL_AND", GaInt_FROM_I64((int64_t)TOK_LOGICAL_AND));
    GaObj_SETATTR(mod, NULL, "TOK_LOGICAL_OR", GaInt_FROM_I64((int64_t)TOK_LOGICAL_OR));
    GaObj_SETATTR(mod, NULL, "TOK_LOGICAL_NOT", GaInt_FROM_I64((int64_t)TOK_LOGICAL_NOT));
    GaObj_SETATTR(mod, NULL, "TOK_NOT", GaInt_FROM_I64((int64_t)TOK_NOT));
    GaObj_SETATTR(mod, NULL, "TOK_DOT", GaInt_FROM_I64((int64_t)TOK_DOT));
    GaObj_SETATTR(mod, NULL, "TOK_COMMA", GaInt_FROM_I64((int64_t)TOK_COMMA));
    GaObj_SETATTR(mod, NULL, "TOK_STRING", GaInt_FROM_I64((int64_t)TOK_STRING_LIT));
    GaObj_SETATTR(mod, NULL, "TOK_INT", GaInt_FROM_I64((int64_t)TOK_INT_LIT));
    GaObj_SETATTR(mod, NULL, "TOK_PHAT_ARROW", GaInt_FROM_I64((int64_t)TOK_PHAT_ARROW));
    GaObj_SETATTR(mod, NULL, "TOK_CLOSED_RANGE", GaInt_FROM_I64((int64_t)TOK_CLOSED_RANGE));
    GaObj_SETATTR(mod, NULL, "TOK_HALF_RANGE", GaInt_FROM_I64((int64_t)TOK_HALF_RANGE));
    GaObj_SETATTR(mod, NULL, "TOK_SHL", GaInt_FROM_I64((int64_t)TOK_SHL));
    GaObj_SETATTR(mod, NULL, "TOK_SHR", GaInt_FROM_I64((int64_t)TOK_SHR));
    GaObj_SETATTR(mod, NULL, "TOK_BACKTICK", GaInt_FROM_I64((int64_t)TOK_BACKTICK));
    GaObj_SETATTR(mod, NULL, "TOK_OPEN_BRACKET", GaInt_FROM_I64((int64_t)TOK_OPEN_BRACKET));
    GaObj_SETATTR(mod, NULL, "TOK_CLOSE_BRACKET", GaInt_FROM_I64((int64_t)TOK_CLOSE_BRACKET));
    GaObj_SETATTR(mod, NULL, "TOK_LEFT_PAREN", GaInt_FROM_I64((int64_t)TOK_LEFT_PAREN));
    GaObj_SETATTR(mod, NULL, "TOK_RIGHT_PAREN", GaInt_FROM_I64((int64_t)TOK_RIGHT_PAREN));

    return mod;
}