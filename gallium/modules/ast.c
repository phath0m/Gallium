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
    struct ga_obj *self_ast = ga_obj_super(self, &ga_astnode_type_inst);

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
        vm_raise_exception(vm, ga_argument_error_new("compile() requires zero argument"));
        return NULL;
    }

    struct compiler_state compiler;
    memset(&compiler, 0, sizeof(compiler));

    struct ga_obj *ret = compiler_compile_ast(&compiler, ga_ast_node_val(self));

    return ret;
}

static struct ga_obj *
ga_ast_node_compile_inline_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 0) {
        vm_raise_exception(vm, ga_argument_error_new("compile() requires zero argument"));
        return NULL;
    }

    struct compiler_state compiler;
    memset(&compiler, 0, sizeof(compiler));

    struct ga_obj *ret = compiler_compile_inline(&compiler, vm->top->code, ga_ast_node_val(self));

    return ret;
}

static void
ga_ast_node_list_destroy_cb(void *v, void *s)
{
    GAOBJ_DEC_REF(v);
}

static void
ga_ast_node_destroy(struct ga_obj *self)
{
    struct ast_node_state *statep = self->un.statep;

    if (statep->children) {
        list_destroy(statep->children, ga_ast_node_list_destroy_cb, NULL);
    }
    
    ast_destroy(statep->node);
}

struct ga_obj *
ga_ast_node_new(struct ast_node *node, struct list *children)
{
    struct ga_obj *obj = ga_obj_new(&ga_astnode_type_inst, &ga_ast_node_ops);
    struct ast_node_state *statep = calloc(sizeof(struct ast_node_state), 1);

    statep->node = node;
    statep->children = children;
    
    obj->un.statep = statep;

    GAOBJ_SETATTR(obj, NULL, "compile", ga_builtin_new(ga_ast_node_compile_method, obj));
    GAOBJ_SETATTR(obj, NULL, "compile_inline", ga_builtin_new(ga_ast_node_compile_inline_method, obj));

    return obj;
}

static struct ga_obj *
ga_ast_node_new_1(struct ast_node *node, struct ga_obj *child)
{
    struct list *listp = list_new();

    list_append(listp, GAOBJ_INC_REF(child));

    return ga_ast_node_new(node, listp);
}

static struct ga_obj *
ga_ast_node_new_2(struct ast_node *node, struct ga_obj *child1, struct ga_obj *child2)
{
    struct list *listp = list_new();

    list_append(listp, GAOBJ_INC_REF(child1));
    list_append(listp, GAOBJ_INC_REF(child2));

    return ga_ast_node_new(node, listp);
}

static struct ga_obj *
ga_binop_type_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 3) {
        vm_raise_exception(vm, ga_argument_error_new("BinOp() requires three arguments"));
        return NULL;
    }

    struct ga_obj *binop_type = ga_obj_super(args[0], &ga_int_type_inst);
    struct ast_node *left = ga_ast_node_val(args[1]);
    struct ast_node *right = ga_ast_node_val(args[2]);

    if (!left || !right) {
        vm_raise_exception(vm, ga_type_error_new("ast.AstNode"));
        return NULL;
    }

    struct ast_node *node = bin_expr_new((binop_t)ga_int_to_i64(binop_type), left, right);
    struct ga_obj *ret = ga_obj_new(&ga_binop_type_inst, NULL);

    ret->super = GAOBJ_INC_REF(ga_ast_node_new_2(node, args[1], args[2]));

    return ret;
}

static struct ga_obj *
ga_call_type_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 2) {
        vm_raise_exception(vm, ga_argument_error_new("Call() requires two arguments"));
        return NULL;
    }

    struct ast_node *target = ga_ast_node_val(args[0]);

    if (!target) {
        vm_raise_exception(vm, ga_type_error_new("ast.AstNode"));
        return NULL;
    }

    struct ga_obj *iter = ga_obj_iter(args[1], vm);

    if (!iter) {
        vm_raise_exception(vm, ga_type_error_new("Iter"));
        return NULL;
    }

    GAOBJ_INC_REF(iter);

    struct ga_obj *ret = NULL;
    struct list *call_args = list_new();
    struct list *call_children = list_new();

    list_append(call_children, GAOBJ_INC_REF(args[0]));

    while (GAOBJ_ITER_NEXT(iter, vm)) {
        struct ga_obj *obj = GAOBJ_ITER_CUR(iter, vm);

        if (!obj) {
            vm_raise_exception(vm, ga_type_error_new("Iter"));
            goto cleanup;
        }

        GAOBJ_INC_REF(obj);

        struct ast_node *child_node = ga_ast_node_val(obj);

        if (!child_node) {
            GAOBJ_DEC_REF(obj);
            goto cleanup;
        }
        
        list_append(call_args, child_node);
        list_append(call_children, obj);
    }

    struct ast_node *node = call_expr_new(target, call_args);

    ret = ga_obj_new(&ga_call_type_inst, NULL);
    ret->super = GAOBJ_INC_REF(ga_ast_node_new(node, call_children));

cleanup:
    if (iter) GAOBJ_DEC_REF(iter);
    if (!ret) {
        /* also destroy call_children... */
        list_destroy(call_args, ast_list_destroy_cb, NULL);
    }

    return ret;
}

static struct ga_obj *
ga_code_block_type_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        vm_raise_exception(vm, ga_argument_error_new("CodeBlock() requires one arguments"));
        return NULL;
    }

    struct ga_obj *iter = ga_obj_iter(args[0], vm);

    if (!iter) {
        vm_raise_exception(vm, ga_type_error_new("Iter"));
        return NULL;
    }

    GAOBJ_INC_REF(iter);

    struct ga_obj *ret = NULL;
    struct list *stmt_list = list_new();
    struct list *children = list_new();

    while (GAOBJ_ITER_NEXT(iter, vm)) {
        struct ga_obj *obj = GAOBJ_ITER_CUR(iter, vm);

        if (!obj) {
            vm_raise_exception(vm, ga_type_error_new("Iter"));
            goto cleanup;
        }

        GAOBJ_INC_REF(obj);

        struct ast_node *child_node = ga_ast_node_val(obj);

        if (!child_node) {
            GAOBJ_DEC_REF(obj);
            goto cleanup;
        }

        list_append(stmt_list, child_node);
        list_append(children, obj);
    }

    struct ast_node *node = code_block_new(stmt_list);

    ret = ga_obj_new(&ga_code_block_type_inst, NULL);
    ret->super = GAOBJ_INC_REF(ga_ast_node_new(node, children));

cleanup:
    if (iter) GAOBJ_DEC_REF(iter);
    if (!ret) {
        list_destroy(stmt_list, ast_list_destroy_cb, NULL);
    }

    return ret;
}

static struct ga_obj *
ga_func_expr_type_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 2) {
        vm_raise_exception(vm, ga_argument_error_new("FuncExpr() requires two arguments"));
        return NULL;
    }

    struct ast_node *body = ga_ast_node_val(args[1]);

    if (!body) {
        vm_raise_exception(vm, ga_type_error_new("ast.AstNode"));
        return NULL;
    }

    struct ga_obj *iter = ga_obj_iter(args[0], vm);

    if (!iter) {
        vm_raise_exception(vm, ga_type_error_new("Iter"));
        return NULL;
    }

    GAOBJ_INC_REF(iter);

    struct ga_obj *ret = NULL;
    struct list *func_params = list_new();
    struct list *func_children = list_new();

    while (GAOBJ_ITER_NEXT(iter, vm)) {
        struct ga_obj *obj = GAOBJ_ITER_CUR(iter, vm);

        if (!obj) {
            vm_raise_exception(vm, ga_type_error_new("Iter"));
            goto cleanup;
        }

        GAOBJ_INC_REF(obj);

        struct ast_node *child_node = ga_ast_node_val(obj);

        if (!child_node) {
            vm_raise_exception(vm, ga_type_error_new("ast.AstNode"));
            GAOBJ_DEC_REF(obj);
            goto cleanup;
        }

        list_append(func_params, child_node);
        list_append(func_children, obj);
    }

    list_append(func_children, GAOBJ_INC_REF(args[1]));

    struct ast_node *node = func_expr_new(func_params, body);

    ret = ga_obj_new(&ga_func_expr_type_inst, NULL);
    ret->super = GAOBJ_INC_REF(ga_ast_node_new(node, func_children));
cleanup:
    if (iter) GAOBJ_DEC_REF(iter);
    if (!ret) {
        /* also destroy call_children... */
        list_destroy(func_params, ast_list_destroy_cb, NULL);
    }

    return ret;
}

static struct ga_obj *
ga_func_param_type_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        vm_raise_exception(vm, ga_argument_error_new("FuncParam() requires one argument"));
        return NULL;
    }

    struct ga_obj *param_name = ga_obj_super(args[0], &ga_str_type_inst);

    if (!param_name) {
        vm_raise_exception(vm, ga_type_error_new("Str"));
        return NULL;
    }

    struct ga_obj *ret = ga_obj_new(&ga_func_param_type_inst, NULL);

    ret->super = GAOBJ_INC_REF(ga_ast_node_new(func_param_new(ga_str_to_cstring(param_name)), NULL));

    return ret;
}

static struct ga_obj *
ga_ident_type_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        vm_raise_exception(vm, ga_argument_error_new("Ident() requires one argument"));
        return NULL;
    }

    struct ga_obj *lit_val = ga_obj_super(args[0], &ga_str_type_inst);

    if (!lit_val) {
        vm_raise_exception(vm, ga_type_error_new("Str"));
        return NULL;
    }

    struct ga_obj *ret = ga_obj_new(&ga_intlit_type_inst, NULL);

    ret->super = GAOBJ_INC_REF(ga_ast_node_new(symbol_term_new(ga_str_to_cstring(lit_val)), NULL));

    return ret;
}

static struct ga_obj *
ga_intlit_type_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        vm_raise_exception(vm, ga_argument_error_new("IntLit() requires one argument"));
        return NULL;
    }

    struct ga_obj *lit_val = ga_obj_super(args[0], &ga_int_type_inst);

    if (!lit_val) {
        vm_raise_exception(vm, ga_type_error_new("Int"));
        return NULL;
    }

    struct ga_obj *ret = ga_obj_new(&ga_intlit_type_inst, NULL);

    ret->super = GAOBJ_INC_REF(ga_ast_node_new(integer_term_new(ga_int_to_i64(lit_val)), NULL));

    return ret;
}

static struct ga_obj *
ga_return_stmt_type_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        vm_raise_exception(vm, ga_argument_error_new("ReturnStmt() requires one arguments"));
        return NULL;
    }

    struct ast_node *return_val = ga_ast_node_val(args[0]);

    if (!return_val) {
        vm_raise_exception(vm, ga_type_error_new("ast.AstNode"));
        return NULL;
    }

    struct ast_node *node = return_stmt_new(return_val);
    struct ga_obj *ret = ga_obj_new(&ga_return_stmt_type_inst, NULL);

    ret->super = GAOBJ_INC_REF(ga_ast_node_new_1(node, args[0]));

    return ret;
}

static struct ga_obj *
ga_stringlit_type_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        vm_raise_exception(vm, ga_argument_error_new("StringLit() requires one argument"));
        return NULL;
    }

    struct ga_obj *lit_val = ga_obj_super(args[0], &ga_str_type_inst);

    if (!lit_val) {
        vm_raise_exception(vm, ga_type_error_new("Str"));
        return NULL;
    }

    struct ga_obj *ret = ga_obj_new(&ga_stringlit_type_inst, NULL);

    ret->super = GAOBJ_INC_REF(ga_ast_node_new(string_term_new(ga_str_to_stringbuf(lit_val)), NULL));

    return ret;
}

static struct ga_obj *
ga_unaryop_type_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 2) {
        vm_raise_exception(vm, ga_argument_error_new("UnaryOp() requires three arguments"));
        return NULL;
    }

    struct ga_obj *unaryop_type = ga_obj_super(args[0], &ga_int_type_inst);
    struct ast_node *expr = ga_ast_node_val(args[1]);

    if (!expr) {
        vm_raise_exception(vm, ga_type_error_new("ast.AstNode"));
        return NULL;
    }

    struct ast_node *node = unary_expr_new((binop_t)ga_int_to_i64(unaryop_type), expr);
    struct ga_obj *ret = ga_obj_new(&ga_unaryop_type_inst, NULL);

    ret->super = GAOBJ_INC_REF(ga_ast_node_new_1(node, args[1]));

    return ret;
}

static struct ga_obj *
ga_while_stmt_type_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 2) {
        vm_raise_exception(vm, ga_argument_error_new("ReturnStmt() requires two arguments"));
        return NULL;
    }

    struct ast_node *cond = ga_ast_node_val(args[0]);
    struct ast_node *body = ga_ast_node_val(args[1]);

    if (!cond || !body) {
        vm_raise_exception(vm, ga_type_error_new("ast.AstNode"));
        return NULL;
    }

    struct ast_node *node = while_stmt_new(cond, body);
    struct ga_obj *ret = ga_obj_new(&ga_while_stmt_type_inst, NULL);

    ret->super = GAOBJ_INC_REF(ga_ast_node_new_2(node, args[0], args[1]));

    return ret;
}

static struct ga_obj *
ga_ast_parse_str(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        vm_raise_exception(vm, ga_argument_error_new("parse_str requires one argument"));
        return NULL;
    }

    struct ga_obj *arg_str = ga_obj_super(args[0], &ga_str_type_inst);

    if (!arg_str) {
        vm_raise_exception(vm, ga_type_error_new("Str"));
        return NULL;
    }

    struct ga_obj *ret = NULL;
    struct parser_state state;
    struct ast_node *root = parser_parse(&state, ga_str_to_cstring(arg_str));
 
    if (!root) {
        goto cleanup;
    }

    ret = ga_ast_node_new(root, NULL);

cleanup:
    parser_fini(&state);
    return ret;
}

struct ga_obj *
ga_ast_node_compile_inline(struct ga_obj *self, struct ga_proc *proc)
{
    struct compiler_state compiler;
    memset(&compiler, 0, sizeof(compiler));

    struct ga_obj *ret = compiler_compile_inline(&compiler, proc, ga_ast_node_val(self));

    return ret;
}

struct ga_obj *
ga_ast_mod_open()
{
    static struct ga_obj *mod = NULL;

    if (mod) {
        return mod;
    }

    mod = ga_mod_new("ast", NULL);

    GAOBJ_SETATTR(mod, NULL, "parse_str", ga_builtin_new(ga_ast_parse_str, NULL));
    GAOBJ_SETATTR(mod, NULL, "AstNode", &ga_astnode_type_inst);
    GAOBJ_SETATTR(mod, NULL, "BinOp", &ga_binop_type_inst);
    GAOBJ_SETATTR(mod, NULL, "Call", &ga_call_type_inst);
    GAOBJ_SETATTR(mod, NULL, "CodeBlock", &ga_code_block_type_inst);
    GAOBJ_SETATTR(mod, NULL, "FuncExpr", &ga_func_expr_type_inst);
    GAOBJ_SETATTR(mod, NULL, "FuncParam", &ga_func_param_type_inst);
    GAOBJ_SETATTR(mod, NULL, "Ident", &ga_ident_type_inst);
    GAOBJ_SETATTR(mod, NULL, "IntLit", &ga_intlit_type_inst);
    GAOBJ_SETATTR(mod, NULL, "ReturnStmt", &ga_return_stmt_type_inst);
    GAOBJ_SETATTR(mod, NULL, "StringLit", &ga_stringlit_type_inst);
    GAOBJ_SETATTR(mod, NULL, "UnaryOp", &ga_unaryop_type_inst);
    GAOBJ_SETATTR(mod, NULL, "WhileStmt", &ga_while_stmt_type_inst);

    /* binop constants... */
    GAOBJ_SETATTR(mod, NULL, "BINOP_ADD", ga_int_from_i64((int64_t)BINOP_ADD));
    GAOBJ_SETATTR(mod, NULL, "BINOP_SUB", ga_int_from_i64((int64_t)BINOP_SUB));
    GAOBJ_SETATTR(mod, NULL, "BINOP_MUL", ga_int_from_i64((int64_t)BINOP_MUL));
    GAOBJ_SETATTR(mod, NULL, "BINOP_DIV", ga_int_from_i64((int64_t)BINOP_DIV));
    
    /* unary op constants */
    GAOBJ_SETATTR(mod, NULL, "UNARYOP_NOT", ga_int_from_i64((int64_t)UNARYOP_NOT));
    GAOBJ_SETATTR(mod, NULL, "UNARYOP_NEGATE", ga_int_from_i64((int64_t)UNARYOP_NEGATE));
    GAOBJ_SETATTR(mod, NULL, "UNARYOP_LOGICAL_NOT", ga_int_from_i64((int64_t)UNARYOP_LOGICAL_NOT));

    return mod;
}
