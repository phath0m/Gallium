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

static GaObject *ga_binop_type_invoke(GaObject *, struct vm *, int, GaObject **);
static GaObject *ga_call_type_invoke(GaObject *, struct vm *, int, GaObject **);
static GaObject *ga_code_block_type_invoke(GaObject *, struct vm *, int, GaObject **);
static GaObject *ga_func_expr_type_invoke(GaObject *, struct vm *, int, GaObject **);
static GaObject *ga_func_param_type_invoke(GaObject *, struct vm *, int, GaObject **);
static GaObject *ga_ident_type_invoke(GaObject *, struct vm *, int, GaObject **);
static GaObject *ga_intlit_type_invoke(GaObject *, struct vm *, int, GaObject **);
static GaObject *ga_return_stmt_type_invoke(GaObject *, struct vm *, int, GaObject **);
static GaObject *ga_stringlit_type_invoke(GaObject *, struct vm *, int, GaObject **);
static GaObject *ga_unaryop_type_invoke(GaObject *, struct vm *, int, GaObject **);
static GaObject *ga_while_stmt_type_invoke(GaObject *, struct vm *, int, GaObject **);

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
GA_BUILTIN_TYPE_DECL(_GaAstNode_Type, "AstNode", NULL);

static void ga_ast_node_destroy(GaObject *);

static struct ga_obj_ops ga_ast_node_ops = {
    .destroy    =   ga_ast_node_destroy
};


struct ast_node_state {
    struct ast_node *   node;
    struct list     *   children;
};

struct ast_node *
ga_ast_node_val(GaObject *self)
{
    GaObject *self_ast = GaObj_Super(self, &_GaAstNode_Type);

    if (!self_ast) {
        return NULL;
    }

    struct ast_node_state *statep = self_ast->un.statep;

    return statep->node;
}

static GaObject *
ga_ast_node_compile_method(GaObject *self, struct vm *vm, int argc, GaObject **args)
{
    if (argc != 0) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("compile() requires zero argument"));
        return NULL;
    }

    struct compiler_state compiler;
    memset(&compiler, 0, sizeof(compiler));

    GaObject *ret = GaAst_Compile(&compiler, ga_ast_node_val(self));

    return ret;
}

static GaObject *
ga_ast_node_compile_inline_method(GaObject *self, struct vm *vm, int argc, GaObject **args)
{
    if (argc != 0) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("compile() requires zero argument"));
        return NULL;
    }

    struct compiler_state compiler;
    memset(&compiler, 0, sizeof(compiler));

    GaObject *ret = GaAst_CompileInline(&compiler, vm->top->code, ga_ast_node_val(self));

    return ret;
}

static void
ga_ast_node_list_destroy_cb(void *v, void *s)
{
    GaObj_DEC_REF(v);
}

static void
ga_ast_node_destroy(GaObject *self)
{
    struct ast_node_state *statep = self->un.statep;

    if (statep->children) {
        GaLinkedList_Destroy(statep->children, ga_ast_node_list_destroy_cb, NULL);
    }
    
    GaAst_Destroy(statep->node);
}

GaObject *
ga_ast_node_new(struct ast_node *node, struct list *children)
{
    GaObject *obj = GaObj_New(&_GaAstNode_Type, &ga_ast_node_ops);
    struct ast_node_state *statep = calloc(sizeof(struct ast_node_state), 1);

    statep->node = node;
    statep->children = children;
    
    obj->un.statep = statep;

    GaObj_SETATTR(obj, NULL, "compile", GaBuiltin_New(ga_ast_node_compile_method, obj));
    GaObj_SETATTR(obj, NULL, "compile_inline", GaBuiltin_New(ga_ast_node_compile_inline_method, obj));

    return obj;
}

static GaObject *
ga_ast_node_new_1(struct ast_node *node, GaObject *child)
{
    struct list *listp = GaLinkedList_New();

    GaLinkedList_Push(listp, GaObj_INC_REF(child));

    return ga_ast_node_new(node, listp);
}

static GaObject *
ga_ast_node_new_2(struct ast_node *node, GaObject *child1, GaObject *child2)
{
    struct list *listp = GaLinkedList_New();

    GaLinkedList_Push(listp, GaObj_INC_REF(child1));
    GaLinkedList_Push(listp, GaObj_INC_REF(child2));

    return ga_ast_node_new(node, listp);
}

static GaObject *
ga_binop_type_invoke(GaObject *self, struct vm *vm, int argc, GaObject **args)
{
    if (argc != 3) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("BinOp() requires three arguments"));
        return NULL;
    }

    GaObject *binop_type = GaObj_Super(args[0], &_GaInt_Type);
    struct ast_node *left = ga_ast_node_val(args[1]);
    struct ast_node *right = ga_ast_node_val(args[2]);

    if (!left || !right) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("ast.AstNode"));
        return NULL;
    }

    struct ast_node *node = GaAst_NewBinOp((binop_t)GaInt_TO_I64(binop_type), left, right);
    GaObject *ret = GaObj_New(&ga_binop_type_inst, NULL);

    ret->super = GaObj_INC_REF(ga_ast_node_new_2(node, args[1], args[2]));

    return ret;
}

static GaObject *
ga_call_type_invoke(GaObject *self, struct vm *vm, int argc, GaObject **args)
{
    if (argc != 2) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("Call() requires two arguments"));
        return NULL;
    }

    struct ast_node *target = ga_ast_node_val(args[0]);

    if (!target) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("ast.AstNode"));
        return NULL;
    }

    GaObject *iter = GaObj_ITER(args[1], vm);

    if (!iter) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Iter"));
        return NULL;
    }

    GaObj_INC_REF(iter);

    GaObject *ret = NULL;
    struct list *call_args = GaLinkedList_New();
    struct list *call_children = GaLinkedList_New();

    GaLinkedList_Push(call_children, GaObj_INC_REF(args[0]));

    while (GaObj_ITER_NEXT(iter, vm)) {
        GaObject *obj = GaObj_ITER_CUR(iter, vm);

        if (!obj) {
            GaEval_RaiseException(vm, GaErr_NewTypeError("Iter"));
            goto cleanup;
        }

        GaObj_INC_REF(obj);

        struct ast_node *child_node = ga_ast_node_val(obj);

        if (!child_node) {
            GaObj_DEC_REF(obj);
            goto cleanup;
        }
        
        GaLinkedList_Push(call_args, child_node);
        GaLinkedList_Push(call_children, obj);
    }

    struct ast_node *node = GaAst_NewCall(target, call_args);

    ret = GaObj_New(&ga_call_type_inst, NULL);
    ret->super = GaObj_INC_REF(ga_ast_node_new(node, call_children));

cleanup:
    if (iter) GaObj_DEC_REF(iter);
    if (!ret) {
        /* also destroy call_children... */
        GaLinkedList_Destroy(call_args, _GaAst_ListDestroyCb, NULL);
    }

    return ret;
}

static GaObject *
ga_code_block_type_invoke(GaObject *self, struct vm *vm, int argc, GaObject **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("CodeBlock() requires one arguments"));
        return NULL;
    }

    GaObject *iter = GaObj_ITER(args[0], vm);

    if (!iter) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Iter"));
        return NULL;
    }

    GaObj_INC_REF(iter);

    GaObject *ret = NULL;
    struct list *stmt_list = GaLinkedList_New();
    struct list *children = GaLinkedList_New();

    while (GaObj_ITER_NEXT(iter, vm)) {
        GaObject *obj = GaObj_ITER_CUR(iter, vm);

        if (!obj) {
            GaEval_RaiseException(vm, GaErr_NewTypeError("Iter"));
            goto cleanup;
        }

        GaObj_INC_REF(obj);

        struct ast_node *child_node = ga_ast_node_val(obj);

        if (!child_node) {
            GaObj_DEC_REF(obj);
            goto cleanup;
        }

        GaLinkedList_Push(stmt_list, child_node);
        GaLinkedList_Push(children, obj);
    }

    struct ast_node *node = GaAst_NewCodeBlock(stmt_list);

    ret = GaObj_New(&ga_code_block_type_inst, NULL);
    ret->super = GaObj_INC_REF(ga_ast_node_new(node, children));

cleanup:
    if (iter) GaObj_DEC_REF(iter);
    if (!ret) {
        GaLinkedList_Destroy(stmt_list, _GaAst_ListDestroyCb, NULL);
    }

    return ret;
}

static GaObject *
ga_func_expr_type_invoke(GaObject *self, struct vm *vm, int argc, GaObject **args)
{
    if (argc != 2) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("FuncExpr() requires two arguments"));
        return NULL;
    }

    struct ast_node *body = ga_ast_node_val(args[1]);

    if (!body) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("ast.AstNode"));
        return NULL;
    }

    GaObject *iter = GaObj_ITER(args[0], vm);

    if (!iter) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Iter"));
        return NULL;
    }

    GaObj_INC_REF(iter);

    GaObject *ret = NULL;
    struct list *func_params = GaLinkedList_New();
    struct list *func_children = GaLinkedList_New();

    while (GaObj_ITER_NEXT(iter, vm)) {
        GaObject *obj = GaObj_ITER_CUR(iter, vm);

        if (!obj) {
            GaEval_RaiseException(vm, GaErr_NewTypeError("Iter"));
            goto cleanup;
        }

        GaObj_INC_REF(obj);

        struct ast_node *child_node = ga_ast_node_val(obj);

        if (!child_node) {
            GaEval_RaiseException(vm, GaErr_NewTypeError("ast.AstNode"));
            GaObj_DEC_REF(obj);
            goto cleanup;
        }

        GaLinkedList_Push(func_params, child_node);
        GaLinkedList_Push(func_children, obj);
    }

    GaLinkedList_Push(func_children, GaObj_INC_REF(args[1]));

    struct ast_node *node = GaAst_NewAnonymousFunc(func_params, body);

    ret = GaObj_New(&ga_func_expr_type_inst, NULL);
    ret->super = GaObj_INC_REF(ga_ast_node_new(node, func_children));
cleanup:
    if (iter) GaObj_DEC_REF(iter);
    if (!ret) {
        /* also destroy call_children... */
        GaLinkedList_Destroy(func_params, _GaAst_ListDestroyCb, NULL);
    }

    return ret;
}

static GaObject *
ga_func_param_type_invoke(GaObject *self, struct vm *vm, int argc, GaObject **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("FuncParam() requires one argument"));
        return NULL;
    }

    GaObject *param_name = GaObj_Super(args[0], &_GaStr_Type);

    if (!param_name) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Str"));
        return NULL;
    }

    GaObject *ret = GaObj_New(&ga_func_param_type_inst, NULL);

    ret->super = GaObj_INC_REF(ga_ast_node_new(GaAst_NewFuncParam(GaStr_ToCString(param_name)), NULL));

    return ret;
}

static GaObject *
ga_ident_type_invoke(GaObject *self, struct vm *vm, int argc, GaObject **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("Ident() requires one argument"));
        return NULL;
    }

    GaObject *lit_val = GaObj_Super(args[0], &_GaStr_Type);

    if (!lit_val) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Str"));
        return NULL;
    }

    GaObject *ret = GaObj_New(&ga_intlit_type_inst, NULL);

    ret->super = GaObj_INC_REF(ga_ast_node_new(GaAst_NewSymbol(GaStr_ToCString(lit_val)), NULL));

    return ret;
}

static GaObject *
ga_intlit_type_invoke(GaObject *self, struct vm *vm, int argc, GaObject **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("IntLit() requires one argument"));
        return NULL;
    }

    GaObject *lit_val = GaObj_Super(args[0], &_GaInt_Type);

    if (!lit_val) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Int"));
        return NULL;
    }

    GaObject *ret = GaObj_New(&ga_intlit_type_inst, NULL);

    ret->super = GaObj_INC_REF(ga_ast_node_new(GaAst_NewInteger(GaInt_TO_I64(lit_val)), NULL));

    return ret;
}

static GaObject *
ga_return_stmt_type_invoke(GaObject *self, struct vm *vm, int argc, GaObject **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("ReturnStmt() requires one arguments"));
        return NULL;
    }

    struct ast_node *return_val = ga_ast_node_val(args[0]);

    if (!return_val) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("ast.AstNode"));
        return NULL;
    }

    struct ast_node *node = GaAst_NewReturn(return_val);
    GaObject *ret = GaObj_New(&ga_return_stmt_type_inst, NULL);

    ret->super = GaObj_INC_REF(ga_ast_node_new_1(node, args[0]));

    return ret;
}

static GaObject *
ga_stringlit_type_invoke(GaObject *self, struct vm *vm, int argc, GaObject **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("StringLit() requires one argument"));
        return NULL;
    }

    GaObject *lit_val = GaObj_Super(args[0], &_GaStr_Type);

    if (!lit_val) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Str"));
        return NULL;
    }

    GaObject *ret = GaObj_New(&ga_stringlit_type_inst, NULL);

    ret->super = GaObj_INC_REF(ga_ast_node_new(GaAst_NewString(GaStr_ToStringBuilder(lit_val)), NULL));

    return ret;
}

static GaObject *
ga_unaryop_type_invoke(GaObject *self, struct vm *vm, int argc, GaObject **args)
{
    if (argc != 2) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("UnaryOp() requires three arguments"));
        return NULL;
    }

    GaObject *unaryop_type = GaObj_Super(args[0], &_GaInt_Type);
    struct ast_node *expr = ga_ast_node_val(args[1]);

    if (!expr) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("ast.AstNode"));
        return NULL;
    }

    struct ast_node *node = GaAst_NewUnaryOp((unaryop_t)GaInt_TO_I64(unaryop_type), expr);
    GaObject *ret = GaObj_New(&ga_unaryop_type_inst, NULL);

    ret->super = GaObj_INC_REF(ga_ast_node_new_1(node, args[1]));

    return ret;
}

static GaObject *
ga_while_stmt_type_invoke(GaObject *self, struct vm *vm, int argc, GaObject **args)
{
    if (argc != 2) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("ReturnStmt() requires two arguments"));
        return NULL;
    }

    struct ast_node *cond = ga_ast_node_val(args[0]);
    struct ast_node *body = ga_ast_node_val(args[1]);

    if (!cond || !body) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("ast.AstNode"));
        return NULL;
    }

    struct ast_node *node = GaAst_NewWhile(cond, body);
    GaObject *ret = GaObj_New(&ga_while_stmt_type_inst, NULL);

    ret->super = GaObj_INC_REF(ga_ast_node_new_2(node, args[0], args[1]));

    return ret;
}

static GaObject *
ga_ast_parse_str(GaObject *self, struct vm *vm, int argc, GaObject **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("parse_str requires one argument"));
        return NULL;
    }

    GaObject *arg_str = GaObj_Super(args[0], &_GaStr_Type);

    if (!arg_str) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Str"));
        return NULL;
    }

    GaObject *ret = NULL;
    struct parser_state state;
    struct ast_node *root = GaParser_ParseString(&state, GaStr_ToCString(arg_str));
 
    if (!root) {
        goto cleanup;
    }

    ret = ga_ast_node_new(root, NULL);

cleanup:
    GaParser_Fini(&state);
    return ret;
}

GaObject *
ga_ast_node_compile_inline(GaObject *self, struct ga_proc *proc)
{
    struct compiler_state compiler;
    memset(&compiler, 0, sizeof(compiler));

    GaObject *ret = GaAst_CompileInline(&compiler, proc, ga_ast_node_val(self));

    return ret;
}

GaObject *
GaMod_OpenAst()
{
    static GaObject *mod = NULL;

    if (mod) {
        return mod;
    }

    mod = GaModule_New("ast", NULL, NULL);

    GaObj_SETATTR(mod, NULL, "parse_str", GaBuiltin_New(ga_ast_parse_str, NULL));
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
