/*
 * ast.c - Gallium module for interacting/creating new ASTs
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
#include <gallium/object.h>
#include <gallium/vm.h>
#include "../ast.h"
#include "../parser.h"
#include "../compiler.h"

static GaObject *binop_type_invoke(GaObject *, GaContext *, int, GaObject **);
static GaObject *call_type_invoke(GaObject *, GaContext *, int, GaObject **);
static GaObject *code_block_type_invoke(GaObject *, GaContext *, int, GaObject **);
static GaObject *func_expr_type_invoke(GaObject *, GaContext *, int, GaObject **);
static GaObject *func_param_type_invoke(GaObject *, GaContext *, int, GaObject **);
static GaObject *ident_type_invoke(GaObject *, GaContext *, int, GaObject **);
static GaObject *intlit_type_invoke(GaObject *, GaContext *, int, GaObject **);
static GaObject *return_stmt_type_invoke(GaObject *, GaContext *, int, GaObject **);
static GaObject *stringlit_type_invoke(GaObject *, GaContext *, int, GaObject **);
static GaObject *unaryop_type_invoke(GaObject *, GaContext *, int, GaObject **);
static GaObject *while_stmt_type_invoke(GaObject *, GaContext *, int, GaObject **);

GA_BUILTIN_TYPE_DECL(ga_binop_type_inst, "BinOp", binop_type_invoke);
GA_BUILTIN_TYPE_DECL(ga_call_type_inst, "Call", call_type_invoke);
GA_BUILTIN_TYPE_DECL(ga_code_block_type_inst, "CodeBlock", code_block_type_invoke);
GA_BUILTIN_TYPE_DECL(ga_func_expr_type_inst, "FuncExpr", func_expr_type_invoke);
GA_BUILTIN_TYPE_DECL(ga_func_param_type_inst, "FuncParam", func_param_type_invoke);
GA_BUILTIN_TYPE_DECL(ga_ident_type_inst, "Ident", ident_type_invoke);
GA_BUILTIN_TYPE_DECL(ga_intlit_type_inst, "IntLit", intlit_type_invoke);
GA_BUILTIN_TYPE_DECL(ga_return_stmt_type_inst, "ReturnStmt", return_stmt_type_invoke);
GA_BUILTIN_TYPE_DECL(ga_stringlit_type_inst, "StringLit", stringlit_type_invoke);
GA_BUILTIN_TYPE_DECL(ga_unaryop_type_inst, "UnaryOp", unaryop_type_invoke);
GA_BUILTIN_TYPE_DECL(ga_while_stmt_type_inst, "WhileStmt", while_stmt_type_invoke);
GA_BUILTIN_TYPE_DECL(_GaAstNode_Type, "AstNode", NULL);

static void ast_node_destroy(GaObject *);

static struct ga_obj_ops ast_node_ops = {
    .destroy    =   ast_node_destroy
};

struct ast_node_state {
    struct ast_node *   node;
    _Ga_list_t     *   children;
};

static inline struct ast_node *
ast_node_val(GaContext *vm, GaObject *self)
{
    GaObject *self_ast =  Ga_ENSURE_TYPE(vm, self, GA_AST_TYPE);

    if (!self_ast) {
        return NULL;
    }

    struct ast_node_state *statep = self_ast->un.statep;
    return statep->node;
}

static GaObject *
ga_ast_node_compile_method(GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_EXACT(vm, 1, (GaObject*[]){ GA_AST_TYPE }, argc,
                             args))
    {
        return NULL;
    }

    GaObject *self = GaObj_Super(args[0], GA_AST_TYPE);
    struct compiler_state compiler;
    memset(&compiler, 0, sizeof(compiler));

    GaObject *ret = GaAst_Compile(&compiler, ast_node_val(vm, self));

    return ret;
}

static GaObject *
ga_ast_node_compile_inline_method(GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_EXACT(vm, 1, (GaObject*[]){ GA_AST_TYPE }, argc,
                             args))
    {
        return NULL;
    }

    GaObject *self = GaObj_Super(args[0], GA_AST_TYPE);
    struct compiler_state compiler;
    memset(&compiler, 0, sizeof(compiler));

    GaObject *ret = GaAst_CompileInline(&compiler, vm->top->code, ast_node_val(vm, self));

    return ret;
}

static void
ga_ast_node_list_destroy_cb(void *v, void *s)
{
    GaObj_DEC_REF(v);
}

static void
ast_node_destroy(GaObject *self)
{
    struct ast_node_state *statep = self->un.statep;

    if (statep->children) {
        _Ga_list_destroy(statep->children, ga_ast_node_list_destroy_cb, NULL);
    }
    
    //GaAst_Destroy(statep->node);

    // TODO: BAD. VERY BAD MEMORY LEAK HERE. FIX LATER
    free(statep->node);
}

static void
assign_methods(GaObject *target, GaObject *self)
{
    GaObj_SETATTR(target, NULL, "compile",
                  GaBuiltin_New(ga_ast_node_compile_method, self));
    GaObj_SETATTR(target, NULL, "compile_inline",
                  GaBuiltin_New(ga_ast_node_compile_inline_method, self));
}

GaObject *
GaAstNode_New(struct ast_node *node, _Ga_list_t *children)
{
    GaObject *obj = GaObj_New(&_GaAstNode_Type, &ast_node_ops);
    struct ast_node_state *statep = calloc(sizeof(struct ast_node_state), 1);

    statep->node = node;
    statep->children = children;
    
    obj->un.statep = statep;

    static bool type_initialized = false;

    if (!type_initialized) {
        assign_methods(GA_AST_TYPE, NULL);
        type_initialized = true;
    }

    assign_methods(obj, obj);

    return obj;
}

static GaObject *
ast_node_new_1(struct ast_node *node, GaObject *child)
{
    _Ga_list_t *listp = _Ga_list_new();

    _Ga_list_push(listp, GaObj_INC_REF(child));

    return GaAstNode_New(node, listp);
}

static GaObject *
ast_node_new_2(struct ast_node *node, GaObject *child1, GaObject *child2)
{
    _Ga_list_t *listp = _Ga_list_new();

    _Ga_list_push(listp, GaObj_INC_REF(child1));
    _Ga_list_push(listp, GaObj_INC_REF(child2));

    return GaAstNode_New(node, listp);
}

static GaObject *
binop_type_invoke(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_EXACT(vm, 3, (GaObject *[]) { GA_INT_TYPE, GA_AST_TYPE,
                             GA_AST_TYPE }, argc, args))
    {
        return NULL;
    }

    GaObject *binop_type = GaObj_Super(args[0], GA_INT_TYPE);
    struct ast_node *left = ast_node_val(vm, args[1]);
    struct ast_node *right = ast_node_val(vm, args[2]);
    struct ast_node *node = GaAst_NewBinOp((binop_t)GaInt_TO_I64(binop_type), left, right);

    GaObject *ret = GaObj_New(&ga_binop_type_inst, NULL);

    ret->super = GaObj_INC_REF(ast_node_new_2(node, args[1], args[2]));

    return ret;
}

static GaObject *
call_type_invoke(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARG_COUNT_EXACT(vm, 2, argc)) {
        return NULL;
    }

    struct ast_node *target = ast_node_val(vm, args[0]);

    if (!target) {
        return NULL;
    }

    GaObject *iter = GaObj_ITER(args[1], vm);

    if (!iter) {
        return NULL;
    }

    GaObj_INC_REF(iter);

    GaObject *ret = NULL;
    _Ga_list_t *call_args = _Ga_list_new();
    _Ga_list_t *call_children = _Ga_list_new();

    _Ga_list_push(call_children, GaObj_INC_REF(args[0]));

    while (GaObj_ITER_NEXT(iter, vm)) {
        GaObject *obj = GaObj_ITER_CUR(iter, vm);

        if (!obj) {
            goto cleanup;
        }

        GaObj_INC_REF(obj);

        struct ast_node *child_node = ast_node_val(vm, obj);

        if (!child_node) {
            GaObj_DEC_REF(obj);
            goto cleanup;
        }
        
        _Ga_list_push(call_args, child_node);
        _Ga_list_push(call_children, obj);
    }

    struct ast_node *node = GaAst_NewCall(target, call_args, 0);

    ret = GaObj_New(&ga_call_type_inst, NULL);
    ret->super = GaObj_INC_REF(GaAstNode_New(node, call_children));

cleanup:
    if (iter) GaObj_DEC_REF(iter);
    if (!ret) {
        /* also destroy call_children... */
        _Ga_list_destroy(call_args, _GaAst_ListDestroyCb, NULL);
    }

    return ret;
}

static GaObject *
code_block_type_invoke(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARG_COUNT_EXACT(vm, 1, argc)) {
        return NULL;
    }

    GaObject *iter = GaObj_ITER(args[0], vm);

    if (!iter) {
        return NULL;
    }

    GaObj_INC_REF(iter);

    GaObject *ret = NULL;
    _Ga_list_t *stmt_list = _Ga_list_new();
    _Ga_list_t *children = _Ga_list_new();

    while (GaObj_ITER_NEXT(iter, vm)) {
        GaObject *obj = GaObj_ITER_CUR(iter, vm);

        if (!obj) {
            goto cleanup;
        }

        GaObj_INC_REF(obj);

        struct ast_node *child_node = ast_node_val(vm, obj);

        if (!child_node) {
            GaObj_DEC_REF(obj);
            goto cleanup;
        }

        _Ga_list_push(stmt_list, child_node);
        _Ga_list_push(children, obj);
    }

    struct ast_node *node = GaAst_NewCodeBlock(stmt_list);

    ret = GaObj_New(&ga_code_block_type_inst, NULL);
    ret->super = GaObj_INC_REF(GaAstNode_New(node, children));

cleanup:
    if (iter) GaObj_DEC_REF(iter);
    if (!ret) {
        _Ga_list_destroy(stmt_list, _GaAst_ListDestroyCb, NULL);
    }

    return ret;
}

static GaObject *
func_expr_type_invoke(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARG_COUNT_EXACT(vm, 2, argc)) {
        return NULL;
    }

    struct ast_node *body = ast_node_val(vm, args[1]);

    if (!body) {
        return NULL;
    }

    GaObject *iter = GaObj_ITER(args[0], vm);

    if (!iter) {
        return NULL;
    }

    GaObj_INC_REF(iter);

    GaObject *ret = NULL;
    _Ga_list_t *func_params = _Ga_list_new();
    _Ga_list_t *func_children = _Ga_list_new();

    while (GaObj_ITER_NEXT(iter, vm)) {
        GaObject *obj = GaObj_ITER_CUR(iter, vm);

        if (!obj) {
            goto cleanup;
        }

        GaObj_INC_REF(obj);

        struct ast_node *child_node = ast_node_val(vm, obj);

        if (!child_node) {
            GaObj_DEC_REF(obj);
            goto cleanup;
        }

        _Ga_list_push(func_params, child_node);
        _Ga_list_push(func_children, obj);
    }

    _Ga_list_push(func_children, GaObj_INC_REF(args[1]));

    struct ast_node *node = GaAst_NewAnonymousFunc(func_params, body);

    ret = GaObj_New(&ga_func_expr_type_inst, NULL);
    ret->super = GaObj_INC_REF(GaAstNode_New(node, func_children));
cleanup:
    if (iter) GaObj_DEC_REF(iter);
    if (!ret) {
        /* also destroy call_children... */
        _Ga_list_destroy(func_params, _GaAst_ListDestroyCb, NULL);
    }

    return ret;
}

static GaObject *
func_param_type_invoke(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_EXACT(vm, 1, (GaObject *[]) { GA_STR_TYPE }, argc,
                             args))
    {
        return NULL;
    }

    GaObject *param_name = GaObj_Super(args[0], &_GaStr_Type);
    GaObject *ret = GaObj_New(&ga_func_param_type_inst, NULL);

    ret->super = GaObj_INC_REF(GaAstNode_New(GaAst_NewFuncParam(GaStr_ToCString(param_name), 0), NULL));

    return ret;
}

static GaObject *
ident_type_invoke(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_EXACT(vm, 1, (GaObject *[]) { GA_STR_TYPE }, argc,
                             args))
    {
        return NULL;
    }

    GaObject *lit_val = GaObj_Super(args[0], &_GaStr_Type);
    GaObject *ret = GaObj_New(&ga_intlit_type_inst, NULL);

    ret->super = GaObj_INC_REF(GaAstNode_New(GaAst_NewSymbol(GaStr_ToCString(lit_val)), NULL));

    return ret;
}

static GaObject *
intlit_type_invoke(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_EXACT(vm, 1, (GaObject *[]) { GA_INT_TYPE }, argc,
                             args))
    {
        return NULL;
    }

    GaObject *lit_val = GaObj_Super(args[0], GA_INT_TYPE);
    GaObject *ret = GaObj_New(&ga_intlit_type_inst, NULL);

    ret->super = GaObj_INC_REF(GaAstNode_New(GaAst_NewInteger(GaInt_TO_I64(lit_val)), NULL));

    return ret;
}

static GaObject *
return_stmt_type_invoke(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_EXACT(vm, 1, (GaObject *[]) { GA_AST_TYPE }, argc,
                             args))
    {
        return NULL;
    }

    struct ast_node *return_val = ast_node_val(vm, args[0]);
    struct ast_node *node = GaAst_NewReturn(return_val);
    GaObject *ret = GaObj_New(&ga_return_stmt_type_inst, NULL);

    ret->super = GaObj_INC_REF(ast_node_new_1(node, args[0]));

    return ret;
}

static GaObject *
stringlit_type_invoke(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_EXACT(vm, 1, (GaObject *[]) { GA_STR_TYPE }, argc,
                             args))
    {
        return NULL;
    }

    GaObject *str_lit = GaObj_Super(args[0], &_GaStr_Type);
    GaObject *ret = GaObj_New(&ga_stringlit_type_inst, NULL);

    ret->super = GaObj_INC_REF(GaAstNode_New(GaAst_NewString(GaStr_ToStringBuilder(str_lit)), NULL));

    return ret;
}

static GaObject *
unaryop_type_invoke(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_EXACT(vm, 2, (GaObject *[]) { GA_INT_TYPE, GA_AST_TYPE },
                             argc, args))
    {
        return NULL;
    }

    GaObject *unaryop_type = GaObj_Super(args[0], &_GaInt_Type);
    struct ast_node *expr = ast_node_val(vm, args[1]);
    struct ast_node *node = GaAst_NewUnaryOp((unaryop_t)GaInt_TO_I64(unaryop_type), expr);
    GaObject *ret = GaObj_New(&ga_unaryop_type_inst, NULL);

    ret->super = GaObj_INC_REF(ast_node_new_1(node, args[1]));

    return ret;
}

static GaObject *
while_stmt_type_invoke(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_EXACT(vm, 2, (GaObject *[]) { GA_AST_TYPE, GA_AST_TYPE },
                             argc, args))
    {
        return NULL;
    }

    struct ast_node *cond = ast_node_val(vm, args[0]);
    struct ast_node *body = ast_node_val(vm, args[1]);
    struct ast_node *node = GaAst_NewWhile(cond, body);
    GaObject *ret = GaObj_New(&ga_while_stmt_type_inst, NULL);

    ret->super = GaObj_INC_REF(ast_node_new_2(node, args[0], args[1]));

    return ret;
}

static GaObject *
astnode_parse_str(GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_EXACT(vm, 1, (GaObject*[]){ GA_STR_TYPE }, argc,
                             args))
    {
        return NULL;
    }

    GaObject *arg_str = GaObj_Super(args[0], &_GaStr_Type);
    GaObject *ret = NULL;
    struct parser_state state;
    struct ast_node *root = GaParser_ParseString(&state, GaStr_ToCString(arg_str));
 
    if (!root) {
        goto cleanup;
    }

    ret = GaAstNode_New(root, NULL);

cleanup:
    GaParser_Fini(&state);
    return ret;
}

GaObject *
GaAstNode_CompileInline(GaObject *self, struct ga_proc *proc)
{
    struct compiler_state compiler;
    memset(&compiler, 0, sizeof(compiler));
    return GaAst_CompileInline(&compiler, proc, ast_node_val(NULL, self));
}

GaObject *
GaMod_OpenAst()
{
    static GaObject *mod = NULL;

    if (mod) {
        return mod;
    }

    mod = GaModule_New("ast", NULL, NULL);

    GaObj_SETATTR(mod, NULL, "parse_str", GaBuiltin_New(astnode_parse_str, NULL));
    GaObj_SETATTR(mod, NULL, "AstNode", &_GaAstNode_Type);
    GaObj_SETATTR(mod, NULL, "BinOp", &ga_binop_type_inst);
    GaObj_SETATTR(mod, NULL, "Call", &ga_call_type_inst);
    GaObj_SETATTR(mod, NULL, "CodeBlock", &ga_code_block_type_inst);
    GaObj_SETATTR(mod, NULL, "FuncExpr", &ga_func_expr_type_inst);
    GaObj_SETATTR(mod, NULL, "FuncParam", &ga_func_param_type_inst);
    GaObj_SETATTR(mod, NULL, "Ident", &ga_ident_type_inst);
    GaObj_SETATTR(mod, NULL, "IntLit", &ga_intlit_type_inst);
    GaObj_SETATTR(mod, NULL, "ReturnStmt", &ga_return_stmt_type_inst);
    GaObj_SETATTR(mod, NULL, "StringLit", &ga_stringlit_type_inst);
    GaObj_SETATTR(mod, NULL, "UnaryOp", &ga_unaryop_type_inst);
    GaObj_SETATTR(mod, NULL, "WhileStmt", &ga_while_stmt_type_inst);

    /* binop constants... */
    GaObj_SETATTR(mod, NULL, "BINOP_ADD", GaInt_FROM_I64((int64_t)BINOP_ADD));
    GaObj_SETATTR(mod, NULL, "BINOP_SUB", GaInt_FROM_I64((int64_t)BINOP_SUB));
    GaObj_SETATTR(mod, NULL, "BINOP_MUL", GaInt_FROM_I64((int64_t)BINOP_MUL));
    GaObj_SETATTR(mod, NULL, "BINOP_DIV", GaInt_FROM_I64((int64_t)BINOP_DIV));
    
    /* unary op constants */
    GaObj_SETATTR(mod, NULL, "UNARYOP_NOT", GaInt_FROM_I64((int64_t)UNARYOP_NOT));
    GaObj_SETATTR(mod, NULL, "UNARYOP_NEGATE", GaInt_FROM_I64((int64_t)UNARYOP_NEGATE));
    GaObj_SETATTR(mod, NULL, "UNARYOP_LOGICAL_NOT", GaInt_FROM_I64((int64_t)UNARYOP_LOGICAL_NOT));

    return mod;
}