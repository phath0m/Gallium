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
#include <gallium/ast.h>
#include <gallium/builtins.h>
#include <gallium/compiler.h>
#include <gallium/object.h>
#include <gallium/parser.h>
#include <gallium/vm.h>

static struct ga_obj *ga_binop_type_invoke(struct ga_obj *, struct vm *, int, struct ga_obj **);
static struct ga_obj *ga_call_type_invoke(struct ga_obj *, struct vm *, int, struct ga_obj **);
static struct ga_obj *ga_code_block_type_invoke(struct ga_obj *, struct vm *, int, struct ga_obj **);
static struct ga_obj *ga_func_expr_type_invoke(struct ga_obj *, struct vm *, int, struct ga_obj **);
static struct ga_obj *ga_func_param_type_invoke(struct ga_obj *, struct vm *, int, struct ga_obj **);
static struct ga_obj *ga_ident_type_invoke(struct ga_obj *, struct vm *, int, struct ga_obj **);
static struct ga_obj *ga_intlit_type_invoke(struct ga_obj *, struct vm *, int, struct ga_obj **);
static struct ga_obj *ga_return_stmt_type_invoke(struct ga_obj *, struct vm *, int, struct ga_obj **);
static struct ga_obj *ga_stringlit_type_invoke(struct ga_obj *, struct vm *, int, struct ga_obj **);
static struct ga_obj *ga_unaryop_type_invoke(struct ga_obj *, struct vm *, int, struct ga_obj **);
static struct ga_obj *ga_while_stmt_type_invoke(struct ga_obj *, struct vm *, int, struct ga_obj **);

GA_BUILTIN_TYPE_DECL(ga_binop_type_inst, "BinOp", ga_binop_type_invoke);
GA_BUILTIN_TYPE_DECL(ga_call_type_inst, "Call", ga_call_type_invoke);
GA_BUILTIN_TYPE_DECL(ga_code_block_type_inst, "CodeBlock", ga_code_block_type_invoke);
GA_BUILTIN_TYPE_DECL(ga_func_expr_type_inst, "FuncExpr", ga_func_expr_type_invoke);
GA_BUILTIN_TYPE_DECL(ga_func_param_type_inst, "FuncParam", ga_func_param_type_invoke);
GA_BUILTIN_TYPE_DECL(ga_ident_type_inst, "Ident", ga_ident_type_invoke);
GA_BUILTIN_TYPE_DECL(ga_intlit_type_inst, "IntLit", ga_intlit_type_invoke);
GA_BUILTIN_TYPE_DECL(ga_return_stmt_type_inst, "ReturnStmt", ga_return_stmt_type_invoke);
GA_BUILTIN_TYPE_DECL(ga_stringlit_type_inst, "StringLit", ga_stringlit_type_invoke);
GA_BUILTIN_TYPE_DECL(ga_unaryop_type_inst, "UnaryOp", ga_unaryop_type_invoke);
GA_BUILTIN_TYPE_DECL(ga_while_stmt_type_inst, "WhileStmt", ga_while_stmt_type_invoke);
GA_BUILTIN_TYPE_DECL(ga_astnode_type_inst, "AstNode", NULL);

static void ga_ast_node_destroy(struct ga_obj *);

static struct ga_obj_ops ga_ast_node_ops = {
    .destroy    =   ga_ast_node_destroy
};


struct ast_node_state {
    struct ast_node *   node;
    struct list     *   children;
};

struct ast_node *
ga_ast_node_val(struct ga_obj *self)
{
    struct ga_obj *self_ast = GaObj_Super(self, &ga_astnode_type_inst);

    if (!self_ast) {
        return NULL;
    }

    struct ast_node_state *statep = self_ast->un.statep;

    return statep->node;
}

static struct ga_obj *
ga_ast_node_compile_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 0) {
        GaEval_RaiseException(vm, ga_argument_error_new("compile() requires zero argument"));
        return NULL;
    }

    struct compiler_state compiler;
    memset(&compiler, 0, sizeof(compiler));

    struct ga_obj *ret = GaAst_Compile(&compiler, ga_ast_node_val(self));

    return ret;
}

static struct ga_obj *
ga_ast_node_compile_inline_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 0) {
        GaEval_RaiseException(vm, ga_argument_error_new("compile() requires zero argument"));
        return NULL;
    }

    struct compiler_state compiler;
    memset(&compiler, 0, sizeof(compiler));

    struct ga_obj *ret = GaAst_CompileInline(&compiler, vm->top->code, ga_ast_node_val(self));

    return ret;
}

static void
ga_ast_node_list_destroy_cb(void *v, void *s)
{
    GaObj_DEC_REF(v);
}

static void
ga_ast_node_destroy(struct ga_obj *self)
{
    struct ast_node_state *statep = self->un.statep;

    if (statep->children) {
        GaList_Destroy(statep->children, ga_ast_node_list_destroy_cb, NULL);
    }
    
    GaAst_Destroy(statep->node);
}

struct ga_obj *
ga_ast_node_new(struct ast_node *node, struct list *children)
{
    struct ga_obj *obj = GaObj_New(&ga_astnode_type_inst, &ga_ast_node_ops);
    struct ast_node_state *statep = calloc(sizeof(struct ast_node_state), 1);

    statep->node = node;
    statep->children = children;
    
    obj->un.statep = statep;

    GaObj_SETATTR(obj, NULL, "compile", ga_builtin_new(ga_ast_node_compile_method, obj));
    GaObj_SETATTR(obj, NULL, "compile_inline", ga_builtin_new(ga_ast_node_compile_inline_method, obj));

    return obj;
}

static struct ga_obj *
ga_ast_node_new_1(struct ast_node *node, struct ga_obj *child)
{
    struct list *listp = GaList_New();

    GaList_Push(listp, GaObj_INC_REF(child));

    return ga_ast_node_new(node, listp);
}

static struct ga_obj *
ga_ast_node_new_2(struct ast_node *node, struct ga_obj *child1, struct ga_obj *child2)
{
    struct list *listp = GaList_New();

    GaList_Push(listp, GaObj_INC_REF(child1));
    GaList_Push(listp, GaObj_INC_REF(child2));

    return ga_ast_node_new(node, listp);
}

static struct ga_obj *
ga_binop_type_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 3) {
        GaEval_RaiseException(vm, ga_argument_error_new("BinOp() requires three arguments"));
        return NULL;
    }

    struct ga_obj *binop_type = GaObj_Super(args[0], &ga_int_type_inst);
    struct ast_node *left = ga_ast_node_val(args[1]);
    struct ast_node *right = ga_ast_node_val(args[2]);

    if (!left || !right) {
        GaEval_RaiseException(vm, ga_type_error_new("ast.AstNode"));
        return NULL;
    }

    struct ast_node *node = GaAst_NewBinOp((binop_t)GA_INT_TO_I64(binop_type), left, right);
    struct ga_obj *ret = GaObj_New(&ga_binop_type_inst, NULL);

    ret->super = GaObj_INC_REF(ga_ast_node_new_2(node, args[1], args[2]));

    return ret;
}

static struct ga_obj *
ga_call_type_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 2) {
        GaEval_RaiseException(vm, ga_argument_error_new("Call() requires two arguments"));
        return NULL;
    }

    struct ast_node *target = ga_ast_node_val(args[0]);

    if (!target) {
        GaEval_RaiseException(vm, ga_type_error_new("ast.AstNode"));
        return NULL;
    }

    struct ga_obj *iter = GaObj_ITER(args[1], vm);

    if (!iter) {
        GaEval_RaiseException(vm, ga_type_error_new("Iter"));
        return NULL;
    }

    GaObj_INC_REF(iter);

    struct ga_obj *ret = NULL;
    struct list *call_args = GaList_New();
    struct list *call_children = GaList_New();

    GaList_Push(call_children, GaObj_INC_REF(args[0]));

    while (GaObj_ITER_NEXT(iter, vm)) {
        struct ga_obj *obj = GaObj_ITER_CUR(iter, vm);

        if (!obj) {
            GaEval_RaiseException(vm, ga_type_error_new("Iter"));
            goto cleanup;
        }

        GaObj_INC_REF(obj);

        struct ast_node *child_node = ga_ast_node_val(obj);

        if (!child_node) {
            GaObj_DEC_REF(obj);
            goto cleanup;
        }
        
        GaList_Push(call_args, child_node);
        GaList_Push(call_children, obj);
    }

    struct ast_node *node = GaAst_NewCall(target, call_args);

    ret = GaObj_New(&ga_call_type_inst, NULL);
    ret->super = GaObj_INC_REF(ga_ast_node_new(node, call_children));

cleanup:
    if (iter) GaObj_DEC_REF(iter);
    if (!ret) {
        /* also destroy call_children... */
        GaList_Destroy(call_args, _GaAst_ListDestroyCb, NULL);
    }

    return ret;
}

static struct ga_obj *
ga_code_block_type_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, ga_argument_error_new("CodeBlock() requires one arguments"));
        return NULL;
    }

    struct ga_obj *iter = GaObj_ITER(args[0], vm);

    if (!iter) {
        GaEval_RaiseException(vm, ga_type_error_new("Iter"));
        return NULL;
    }

    GaObj_INC_REF(iter);

    struct ga_obj *ret = NULL;
    struct list *stmt_list = GaList_New();
    struct list *children = GaList_New();

    while (GaObj_ITER_NEXT(iter, vm)) {
        struct ga_obj *obj = GaObj_ITER_CUR(iter, vm);

        if (!obj) {
            GaEval_RaiseException(vm, ga_type_error_new("Iter"));
            goto cleanup;
        }

        GaObj_INC_REF(obj);

        struct ast_node *child_node = ga_ast_node_val(obj);

        if (!child_node) {
            GaObj_DEC_REF(obj);
            goto cleanup;
        }

        GaList_Push(stmt_list, child_node);
        GaList_Push(children, obj);
    }

    struct ast_node *node = GaAst_NewCodeBlock(stmt_list);

    ret = GaObj_New(&ga_code_block_type_inst, NULL);
    ret->super = GaObj_INC_REF(ga_ast_node_new(node, children));

cleanup:
    if (iter) GaObj_DEC_REF(iter);
    if (!ret) {
        GaList_Destroy(stmt_list, _GaAst_ListDestroyCb, NULL);
    }

    return ret;
}

static struct ga_obj *
ga_func_expr_type_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 2) {
        GaEval_RaiseException(vm, ga_argument_error_new("FuncExpr() requires two arguments"));
        return NULL;
    }

    struct ast_node *body = ga_ast_node_val(args[1]);

    if (!body) {
        GaEval_RaiseException(vm, ga_type_error_new("ast.AstNode"));
        return NULL;
    }

    struct ga_obj *iter = GaObj_ITER(args[0], vm);

    if (!iter) {
        GaEval_RaiseException(vm, ga_type_error_new("Iter"));
        return NULL;
    }

    GaObj_INC_REF(iter);

    struct ga_obj *ret = NULL;
    struct list *func_params = GaList_New();
    struct list *func_children = GaList_New();

    while (GaObj_ITER_NEXT(iter, vm)) {
        struct ga_obj *obj = GaObj_ITER_CUR(iter, vm);

        if (!obj) {
            GaEval_RaiseException(vm, ga_type_error_new("Iter"));
            goto cleanup;
        }

        GaObj_INC_REF(obj);

        struct ast_node *child_node = ga_ast_node_val(obj);

        if (!child_node) {
            GaEval_RaiseException(vm, ga_type_error_new("ast.AstNode"));
            GaObj_DEC_REF(obj);
            goto cleanup;
        }

        GaList_Push(func_params, child_node);
        GaList_Push(func_children, obj);
    }

    GaList_Push(func_children, GaObj_INC_REF(args[1]));

    struct ast_node *node = GaAst_NewAnonymousFunc(func_params, body);

    ret = GaObj_New(&ga_func_expr_type_inst, NULL);
    ret->super = GaObj_INC_REF(ga_ast_node_new(node, func_children));
cleanup:
    if (iter) GaObj_DEC_REF(iter);
    if (!ret) {
        /* also destroy call_children... */
        GaList_Destroy(func_params, _GaAst_ListDestroyCb, NULL);
    }

    return ret;
}

static struct ga_obj *
ga_func_param_type_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, ga_argument_error_new("FuncParam() requires one argument"));
        return NULL;
    }

    struct ga_obj *param_name = GaObj_Super(args[0], &ga_str_type_inst);

    if (!param_name) {
        GaEval_RaiseException(vm, ga_type_error_new("Str"));
        return NULL;
    }

    struct ga_obj *ret = GaObj_New(&ga_func_param_type_inst, NULL);

    ret->super = GaObj_INC_REF(ga_ast_node_new(GaAst_NewFuncParam(ga_str_to_cstring(param_name)), NULL));

    return ret;
}

static struct ga_obj *
ga_ident_type_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, ga_argument_error_new("Ident() requires one argument"));
        return NULL;
    }

    struct ga_obj *lit_val = GaObj_Super(args[0], &ga_str_type_inst);

    if (!lit_val) {
        GaEval_RaiseException(vm, ga_type_error_new("Str"));
        return NULL;
    }

    struct ga_obj *ret = GaObj_New(&ga_intlit_type_inst, NULL);

    ret->super = GaObj_INC_REF(ga_ast_node_new(GaAst_NewSymbol(ga_str_to_cstring(lit_val)), NULL));

    return ret;
}

static struct ga_obj *
ga_intlit_type_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, ga_argument_error_new("IntLit() requires one argument"));
        return NULL;
    }

    struct ga_obj *lit_val = GaObj_Super(args[0], &ga_int_type_inst);

    if (!lit_val) {
        GaEval_RaiseException(vm, ga_type_error_new("Int"));
        return NULL;
    }

    struct ga_obj *ret = GaObj_New(&ga_intlit_type_inst, NULL);

    ret->super = GaObj_INC_REF(ga_ast_node_new(GaAst_NewInteger(GA_INT_TO_I64(lit_val)), NULL));

    return ret;
}

static struct ga_obj *
ga_return_stmt_type_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, ga_argument_error_new("ReturnStmt() requires one arguments"));
        return NULL;
    }

    struct ast_node *return_val = ga_ast_node_val(args[0]);

    if (!return_val) {
        GaEval_RaiseException(vm, ga_type_error_new("ast.AstNode"));
        return NULL;
    }

    struct ast_node *node = GaAst_NewReturn(return_val);
    struct ga_obj *ret = GaObj_New(&ga_return_stmt_type_inst, NULL);

    ret->super = GaObj_INC_REF(ga_ast_node_new_1(node, args[0]));

    return ret;
}

static struct ga_obj *
ga_stringlit_type_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, ga_argument_error_new("StringLit() requires one argument"));
        return NULL;
    }

    struct ga_obj *lit_val = GaObj_Super(args[0], &ga_str_type_inst);

    if (!lit_val) {
        GaEval_RaiseException(vm, ga_type_error_new("Str"));
        return NULL;
    }

    struct ga_obj *ret = GaObj_New(&ga_stringlit_type_inst, NULL);

    ret->super = GaObj_INC_REF(ga_ast_node_new(GaAst_NewString(ga_str_to_stringbuf(lit_val)), NULL));

    return ret;
}

static struct ga_obj *
ga_unaryop_type_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 2) {
        GaEval_RaiseException(vm, ga_argument_error_new("UnaryOp() requires three arguments"));
        return NULL;
    }

    struct ga_obj *unaryop_type = GaObj_Super(args[0], &ga_int_type_inst);
    struct ast_node *expr = ga_ast_node_val(args[1]);

    if (!expr) {
        GaEval_RaiseException(vm, ga_type_error_new("ast.AstNode"));
        return NULL;
    }

    struct ast_node *node = GaAst_NewUnaryOp((unaryop_t)GA_INT_TO_I64(unaryop_type), expr);
    struct ga_obj *ret = GaObj_New(&ga_unaryop_type_inst, NULL);

    ret->super = GaObj_INC_REF(ga_ast_node_new_1(node, args[1]));

    return ret;
}

static struct ga_obj *
ga_while_stmt_type_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 2) {
        GaEval_RaiseException(vm, ga_argument_error_new("ReturnStmt() requires two arguments"));
        return NULL;
    }

    struct ast_node *cond = ga_ast_node_val(args[0]);
    struct ast_node *body = ga_ast_node_val(args[1]);

    if (!cond || !body) {
        GaEval_RaiseException(vm, ga_type_error_new("ast.AstNode"));
        return NULL;
    }

    struct ast_node *node = GaAst_NewWhile(cond, body);
    struct ga_obj *ret = GaObj_New(&ga_while_stmt_type_inst, NULL);

    ret->super = GaObj_INC_REF(ga_ast_node_new_2(node, args[0], args[1]));

    return ret;
}

static struct ga_obj *
ga_ast_parse_str(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, ga_argument_error_new("parse_str requires one argument"));
        return NULL;
    }

    struct ga_obj *arg_str = GaObj_Super(args[0], &ga_str_type_inst);

    if (!arg_str) {
        GaEval_RaiseException(vm, ga_type_error_new("Str"));
        return NULL;
    }

    struct ga_obj *ret = NULL;
    struct parser_state state;
    struct ast_node *root = GaParser_ParseString(&state, ga_str_to_cstring(arg_str));
 
    if (!root) {
        goto cleanup;
    }

    ret = ga_ast_node_new(root, NULL);

cleanup:
    GaParser_Fini(&state);
    return ret;
}

struct ga_obj *
ga_ast_node_compile_inline(struct ga_obj *self, struct ga_proc *proc)
{
    struct compiler_state compiler;
    memset(&compiler, 0, sizeof(compiler));

    struct ga_obj *ret = GaAst_CompileInline(&compiler, proc, ga_ast_node_val(self));

    return ret;
}

struct ga_obj *
ga_ast_mod_open()
{
    static struct ga_obj *mod = NULL;

    if (mod) {
        return mod;
    }

    mod = ga_mod_new("ast", NULL, NULL);

    GaObj_SETATTR(mod, NULL, "parse_str", ga_builtin_new(ga_ast_parse_str, NULL));
    GaObj_SETATTR(mod, NULL, "AstNode", &ga_astnode_type_inst);
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
    GaObj_SETATTR(mod, NULL, "BINOP_ADD", GA_INT_FROM_I64((int64_t)BINOP_ADD));
    GaObj_SETATTR(mod, NULL, "BINOP_SUB", GA_INT_FROM_I64((int64_t)BINOP_SUB));
    GaObj_SETATTR(mod, NULL, "BINOP_MUL", GA_INT_FROM_I64((int64_t)BINOP_MUL));
    GaObj_SETATTR(mod, NULL, "BINOP_DIV", GA_INT_FROM_I64((int64_t)BINOP_DIV));
    
    /* unary op constants */
    GaObj_SETATTR(mod, NULL, "UNARYOP_NOT", GA_INT_FROM_I64((int64_t)UNARYOP_NOT));
    GaObj_SETATTR(mod, NULL, "UNARYOP_NEGATE", GA_INT_FROM_I64((int64_t)UNARYOP_NEGATE));
    GaObj_SETATTR(mod, NULL, "UNARYOP_LOGICAL_NOT", GA_INT_FROM_I64((int64_t)UNARYOP_LOGICAL_NOT));

    return mod;
}
