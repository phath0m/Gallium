/*
 * ast.c - Responsible for initializing varying AST structures
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <gallium/builtins.h>
#include <gallium/list.h>
#include <gallium/stringbuf.h>
#include "ast.h"
#include "compiler.h"

struct ast_node ast_empty_stmt_inst = {
    .type = AST_EMPTY_STMT
};

static GaObject *
ga_ast_node_compile_method(GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_EXACT(vm, 1, (GaObject*[]){ GA_AST_TYPE }, argc,
                             args))
    {
        return NULL;
    }
    struct ast_node *self = (struct ast_node *)GaObj_Super(args[0], GA_AST_TYPE);
    struct compiler_state compiler;
    memset(&compiler, 0, sizeof(compiler));
    return GaAst_Compile(vm, &compiler, self);
}

static GaObject *
ga_ast_node_compile_inline_method(GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_EXACT(vm, 1, (GaObject*[]){ GA_AST_TYPE }, argc,
                             args))
    {
        return NULL;
    }
    struct ast_node *self = (struct ast_node *)GaObj_Super(args[0], GA_AST_TYPE);
    struct compiler_state compiler;
    memset(&compiler, 0, sizeof(compiler));
    return GaAst_CompileInline(vm, vm->top->code, self);
}

static void
assign_methods(GaObject *target, GaObject *self)
{
    GaObj_SETATTR(target, NULL, "compile",
                  GaBuiltin_New(ga_ast_node_compile_method, self));
    GaObj_SETATTR(target, NULL, "compile_inline",
                  GaBuiltin_New(ga_ast_node_compile_inline_method, self));
}

static void ast_destroy(GaObject *);

static struct ast_node *
ast_node_new(ast_class_t type, size_t size)
{
    static struct Ga_Operators operators = {
        .destroy = ast_destroy
    };
    struct ast_node *node = (struct ast_node*)GaObj_NewEx(GA_AST_TYPE, &operators, size);
    node->type = type;
    assign_methods((GaObject *)node, (GaObject *)node);
    return node;
}

/* is this a hack? I'm not sure. Makes my life easier though */
#define AST_NODE_NEW(t, c)  (t*)ast_node_new((c), sizeof(t))

/* Helper method, increment ref counts */
static void
inc_child_refs(_Ga_list_t *children)
{
    struct ast_node *node;
    _Ga_iter_t iter;
    _Ga_list_get_iter(children, &iter);
    while (_Ga_iter_next(&iter, (void**)&node)) {
        if (node) GaObj_INC_REF(&node->object);
    }
}

static void
dec_child_refs(_Ga_list_t *children)
{
    struct ast_node *node;
    _Ga_iter_t iter;
    _Ga_list_get_iter(children, &iter);
    while (_Ga_iter_next(&iter, (void**)&node)) {
        if (node) GaObj_DEC_REF(&node->object);
    }
}

struct ast_node *
GaAst_NewCodeBlock(_Ga_list_t *children)
{
    struct code_block *node = AST_NODE_NEW(struct code_block, AST_CODE_BLOCK);
    node->children = children;
    inc_child_refs(children);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewClass(const char *name, struct ast_node *base, _Ga_list_t *mixins, _Ga_list_t *methods)
{
    size_t name_len = strlen(name);
    struct class_decl *node = (struct class_decl*)ast_node_new(AST_CLASS_DECL,
            sizeof(struct class_decl) + name_len + 1);
    strcpy(node->name, name);
    node->base = base;
    node->mixins = mixins;
    node->methods = methods;
    if (base) GaObj_INC_REF(&base->object);
    inc_child_refs(methods);
    inc_child_refs(mixins);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewEnum(const char *name, _Ga_list_t *values)
{
    size_t name_len = strlen(name);
    struct enum_decl *node = (struct enum_decl*)ast_node_new(AST_ENUM_DECL,
            sizeof(struct enum_decl) + name_len + 1);
    strcpy(node->name, name);
    node->values = values;
    inc_child_refs(values);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewMixin(const char *name, _Ga_list_t *methods)
{
    size_t name_len = strlen(name);
    struct mixin_decl *node = (struct mixin_decl*)ast_node_new(AST_MIXIN_DECL,
            sizeof(struct mixin_decl) + name_len + 1);
    strcpy(node->name, name);
    node->methods = methods;
    inc_child_refs(methods);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewFunc(const char *name, _Ga_list_t *parameters, struct ast_node *body)
{
    size_t name_len = strlen(name);
    struct func_decl *node = (struct func_decl*)ast_node_new(AST_FUNC_DECL,
            sizeof(struct func_decl) + name_len + 1);

    strcpy(node->name, name);
    node->parameters = parameters;
    node->body = body;
    inc_child_refs(parameters);
    GaObj_INC_REF(&body->object);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewAnonymousFunc(_Ga_list_t *parameters, struct ast_node *body)
{
    const char *name = "__anonymous__";
    size_t name_len = strlen(name);
    struct func_decl *node = (struct func_decl*)ast_node_new(AST_FUNC_EXPR,
            sizeof(struct func_decl) + name_len + 1);

    strncpy(node->name, name, name_len + 1);
    node->parameters = parameters;
    node->body = body;
    inc_child_refs(parameters);
    GaObj_INC_REF(&body->object);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewFuncParam(const char *name, int flags)
{
    size_t name_len = strlen(name);
    struct func_param *param = (struct func_param*)ast_node_new(AST_FUNC_PARAM,
            sizeof(struct func_param) + name_len + 1);
    strcpy(param->name, name);
    param->flags = flags;
    return (struct ast_node*)param;
}

struct ast_node *
GaAst_NewBreak()
{
    return AST_NODE_NEW(struct ast_node, AST_BREAK_STMT);
}

struct ast_node *
GaAst_NewContinue()
{
    return AST_NODE_NEW(struct ast_node, AST_CONTINUE_STMT);
}

struct ast_node *
GaAst_NewFor(const char *var_name, struct ast_node *expr, struct ast_node *body)
{
    size_t name_len = strlen(var_name);
    struct for_stmt *node = (struct for_stmt*)ast_node_new(AST_FOR_STMT,
            sizeof(struct for_stmt) + name_len + 1);
    strcpy(node->var_name, var_name);
    node->expr = expr;
    node->body = body;
    GaObj_INC_REF(&expr->object);
    GaObj_INC_REF(&body->object);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewIf(struct ast_node *cond, struct ast_node *if_body, struct ast_node *else_body)
{
    struct if_stmt *node = AST_NODE_NEW(struct if_stmt, AST_IF_STMT);
    node->cond = cond;
    node->if_body = if_body;
    node->else_body = else_body;
    GaObj_INC_REF(&cond->object);
    GaObj_INC_REF(&if_body->object);
    if (else_body) GaObj_INC_REF(&else_body->object);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewReturn(struct ast_node *val)
{
    struct return_stmt *node = AST_NODE_NEW(struct return_stmt, AST_RETURN_STMT);
    node->val = val;
    GaObj_INC_REF(&val->object);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewTry(struct ast_node *try_body, struct ast_node *except_body, const char *varname)
{
    size_t name_len = 0;

    if (varname) {
        name_len = strlen(varname);
    }

    struct try_stmt *node = (struct try_stmt*)ast_node_new(AST_TRY_STMT,
            sizeof(struct try_stmt) + name_len + 1);

    node->try_body = try_body;
    node->except_body = except_body;

    if (varname) {
        node->has_var = true;
        strcpy(node->var_name, varname);
    }
    GaObj_INC_REF(&try_body->object);
    GaObj_INC_REF(&except_body->object);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewUse(const char *path, _Ga_list_t *imports, bool wildcard)
{
    size_t path_len = strlen(path);
    struct use_stmt *node = (struct use_stmt*)ast_node_new(AST_USE_STMT,
            sizeof(struct use_stmt) + path_len + 1);
    strcpy(node->import_path, path);

    node->imports = imports;
    node->wildcard = wildcard;
    if (imports) inc_child_refs(imports);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewWhile(struct ast_node *cond, struct ast_node *body)
{
    struct while_stmt *node = AST_NODE_NEW(struct while_stmt, AST_WHILE_STMT);
    node->cond = cond;
    node->body = body;
    GaObj_INC_REF(&cond->object);
    GaObj_INC_REF(&body->object);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewWith(struct ast_node *expr, struct ast_node *body)
{
    struct with_stmt *node = AST_NODE_NEW(struct with_stmt, AST_WITH_STMT);
    node->expr = expr;
    node->body = body;
    GaObj_INC_REF(&expr->object);
    GaObj_INC_REF(&body->object);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewCall(struct ast_node *target, _Ga_list_t *arguments, int flags)
{
    struct call_expr *node = AST_NODE_NEW(struct call_expr, AST_CALL_EXPR);
    node->arguments = arguments;
    node->target = target;
    node->flags = flags;
    inc_child_refs(arguments);
    GaObj_INC_REF(&target->object);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewMacro(struct ast_node *target, _Ga_list_t *token_list)
{
    struct call_macro_expr *node = AST_NODE_NEW(struct call_macro_expr, AST_CALL_MACRO_EXPR);
    node->token_list = token_list;
    node->target = target;
    GaObj_INC_REF(&target->object);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewExpr(struct ast_node *left, struct ast_node *right)
{
    struct assign_expr *node = AST_NODE_NEW(struct assign_expr, AST_ASSIGN_EXPR);
    node->left = left;
    node->right = right;
    GaObj_INC_REF(&left->object);
    GaObj_INC_REF(&right->object);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewBinOp(binop_t op, struct ast_node *left, struct ast_node *right)
{
    struct bin_expr *node = AST_NODE_NEW(struct bin_expr, AST_BIN_EXPR);
    node->left = left;
    node->right = right;
    node->op = op;
    GaObj_INC_REF(&left->object);
    GaObj_INC_REF(&right->object);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewDict(_Ga_list_t *kvp_pairs)
{
    struct dict_expr *node = AST_NODE_NEW(struct dict_expr, AST_DICT_EXPR);
    node->kvp_pairs = kvp_pairs;
    inc_child_refs(kvp_pairs);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewKeyValuePair(struct ast_node *key, struct ast_node *val)
{
    struct key_val_expr *node = AST_NODE_NEW(struct key_val_expr, AST_KEY_VAL_EXPR);
    node->key = key;
    node->val = val;
    GaObj_INC_REF(&key->object);
    GaObj_INC_REF(&val->object);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewUnaryOp(unaryop_t op, struct ast_node *expr)
{
    struct unary_expr *node = AST_NODE_NEW(struct unary_expr, AST_UNARY_EXPR);
    node->expr = expr;
    node->op = op;
    GaObj_INC_REF(&expr->object);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewBool(bool val)
{
    struct bool_term *node = AST_NODE_NEW(struct bool_term, AST_BOOL_TERM);
    node->val = val;
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewFloat(double val)
{
    struct float_term *node = AST_NODE_NEW(struct float_term, AST_FLOAT_TERM);
    node->val = val;
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewInteger(int64_t val)
{
    struct integer_term *node = AST_NODE_NEW(struct integer_term, AST_INTEGER_TERM);
    node->val = val;
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewString(struct stringbuf *sb)
{
    size_t str_len = STRINGBUF_LEN(sb);
    struct string_term *node = (struct string_term*)ast_node_new(AST_STRING_TERM,
            sizeof(struct string_term) + str_len + 1);
    memcpy(node->val, STRINGBUF_VALUE(sb), str_len);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewSymbol(const char *name)
{
    size_t name_len = strlen(name);
    struct symbol_term *node = (struct symbol_term*)ast_node_new(AST_SYMBOL_TERM,
            sizeof(struct symbol_term) + name_len + 1);
    strcpy(node->name, name);;
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewIndexer(struct ast_node *expr, struct ast_node *key)
{
    struct index_access_expr *node = AST_NODE_NEW(struct index_access_expr, AST_INDEX_ACCESS_EXPR);
    node->expr = expr;
    node->key = key;
    GaObj_INC_REF(&expr->object);
    GaObj_INC_REF(&key->object);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewList(_Ga_list_t *items)
{
    struct list_expr *node = AST_NODE_NEW(struct list_expr, AST_LIST_EXPR);
    node->items = items;
    inc_child_refs(items);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewMatch(struct ast_node *expr, _Ga_list_t *cases, struct ast_node *default_case)
{
    struct match_expr *node = AST_NODE_NEW(struct match_expr, AST_MATCH_EXPR);
    node->expr = expr;
    node->cases = cases;
    node->default_case = default_case;
    GaObj_INC_REF(&expr->object);
    if (default_case) GaObj_INC_REF(&default_case->object);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewCase(struct ast_node *pattern, struct ast_node *cond, struct ast_node *value)
{
    struct match_case *node = AST_NODE_NEW(struct match_case, AST_MATCH_CASE);
    node->pattern = pattern;
    node->cond = cond;
    node->value = value;
    GaObj_INC_REF(&pattern->object);
    if (cond) GaObj_INC_REF(&cond->object);
    GaObj_INC_REF(&value->object);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewListPattern(_Ga_list_t *items)
{
    struct list_expr *node = AST_NODE_NEW(struct list_expr, AST_LIST_PATTERN);
    node->items = items;
    inc_child_refs(items);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewOrPattern(_Ga_list_t *items)
{
    struct list_expr *node = AST_NODE_NEW(struct list_expr, AST_OR_PATTERN);
    node->items = items;
    inc_child_refs(items);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewMemberAccess(struct ast_node *expr, const char *member)
{
    size_t member_len = strlen(member);
    struct member_access_expr *node = (struct member_access_expr*)ast_node_new(AST_MEMBER_ACCESS_EXPR,
            sizeof(struct member_access_expr) + member_len + 1);
    strcpy(node->member, member);
    node->expr = expr;
    GaObj_INC_REF(&expr->object);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewQuote(_Ga_list_t *children)
{
    struct quote_expr *node = AST_NODE_NEW(struct quote_expr, AST_QUOTE_EXPR);
    node->children = children;
    inc_child_refs(children);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewTuple(_Ga_list_t *items)
{
    struct tuple_expr *node = AST_NODE_NEW(struct tuple_expr, AST_TUPLE_EXPR);
    node->items = items;
    inc_child_refs(items);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewWhen(struct ast_node *true_val, struct ast_node *cond, struct ast_node *false_val)
{
    struct when_expr *node = AST_NODE_NEW(struct when_expr, AST_WHEN_EXPR);
    node->true_val = true_val;
    node->cond = cond;
    node->false_val = false_val;
    GaObj_INC_REF(&true_val->object);
    GaObj_INC_REF(&cond->object);
    GaObj_INC_REF(&false_val->object);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewRaise(struct ast_node *expr)
{
    struct raise_stmt *node = AST_NODE_NEW(struct raise_stmt, AST_RAISE_STMT);
    node->expr = expr;
    GaObj_INC_REF(&expr->object);
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewLet(const char *name, struct ast_node *right)
{
    size_t name_len = strlen(name);
    struct let_stmt *node = (struct let_stmt*)ast_node_new(AST_LET_STMT,
            sizeof(struct let_stmt) + name_len + 1);
    strcpy(node->var_name, name);
    node->right = right;
    GaObj_INC_REF(&right->object);
    return (struct ast_node*)node;
}

GaObject *_GaAst_type = NULL;

#define AST_DEC_REF(a) GaObj_DEC_REF(&(a)->object)

static void
ast_destroy(GaObject *self)
{
    struct ast_node *ast = (struct ast_node *)self;

    switch (ast->type) {
        case AST_ASSIGN_EXPR: {
            struct assign_expr *expr = (struct assign_expr*)ast;
            AST_DEC_REF(expr->left);
            AST_DEC_REF(expr->right);
            break;
        }
        case AST_BIN_EXPR: {
            struct bin_expr *expr = (struct bin_expr*)ast;
            AST_DEC_REF(expr->left);
            AST_DEC_REF(expr->right);
            break;   
        }
        case AST_CALL_EXPR: {
            struct call_expr *expr = (struct call_expr*)ast;
            AST_DEC_REF(expr->target);
            dec_child_refs(expr->arguments);
            break;
        }
        case AST_CALL_MACRO_EXPR: {
            struct call_macro_expr *expr = (struct call_macro_expr*)ast;
            AST_DEC_REF(expr->target);
            break;
        }
        case AST_CLASS_DECL: {
            struct class_decl *decl = (struct class_decl*)ast;
            dec_child_refs(decl->methods);
            dec_child_refs(decl->mixins);
            if (decl->base) AST_DEC_REF(decl->base);
            break;
        }
        case AST_CODE_BLOCK: {
            struct code_block *block = (struct code_block*)ast;
            dec_child_refs(block->children);
            break;   
        }
        case AST_DICT_EXPR: {
            struct dict_expr *expr = (struct dict_expr *)ast;
            dec_child_refs(expr->kvp_pairs);
            break;
        }
        case AST_ENUM_DECL: {
            struct enum_decl *decl = (struct enum_decl *)ast;
            dec_child_refs(decl->values);
            break;   
        }
        case AST_FOR_STMT: {
            struct for_stmt *stmt = (struct for_stmt *)ast;
            AST_DEC_REF(stmt->expr);
            AST_DEC_REF(stmt->body);
            break;
        }
        case AST_FUNC_EXPR:
        case AST_FUNC_DECL: {
            struct func_decl *decl = (struct func_decl *)ast;
            AST_DEC_REF(decl->body);
            dec_child_refs(decl->parameters);
            break;
        }
        case AST_IF_STMT: {
            struct if_stmt *stmt = (struct if_stmt *)ast;
            AST_DEC_REF(stmt->cond);
            AST_DEC_REF(stmt->if_body);
            if (stmt->else_body) AST_DEC_REF(stmt->else_body);
            break;
        }
        case AST_INDEX_ACCESS_EXPR: {
            struct index_access_expr *expr = (struct index_access_expr *)ast;
            AST_DEC_REF(expr->key);
            AST_DEC_REF(expr->expr);
            break;
        }
        case AST_KEY_VAL_EXPR: {
            struct key_val_expr *kvp = (struct key_val_expr *)ast;
            AST_DEC_REF(kvp->key);
            AST_DEC_REF(kvp->val);
            break;
        }
        case AST_LET_STMT: {
            struct let_stmt *stmt = (struct let_stmt *)ast;
            AST_DEC_REF(stmt->right);
            break;
        }
        case AST_LIST_EXPR: {
            struct list_expr *expr = (struct list_expr *)ast;
            dec_child_refs(expr->items);
            break;
        }
        case AST_LIST_PATTERN: {
            struct list_pattern *pattern = (struct list_pattern *)ast;
            dec_child_refs(pattern->items);
            break;
        }
        case AST_MATCH_CASE: {
            struct match_case *expr = (struct match_case *)ast;
            if (expr->cond) AST_DEC_REF(expr->cond);
            AST_DEC_REF(expr->pattern);
            AST_DEC_REF(expr->value);
            break;
        }
        case AST_MATCH_EXPR: {
            struct match_expr *expr = (struct match_expr *)ast;
            AST_DEC_REF(expr->expr);
            if (expr->default_case) AST_DEC_REF(expr->default_case);
            dec_child_refs(expr->cases);
            break;
        }
        case AST_MEMBER_ACCESS_EXPR: {
            struct member_access_expr *expr = (struct member_access_expr *)ast;
            AST_DEC_REF(expr->expr);
            break;
        }
        case AST_MIXIN_DECL: {
            struct mixin_decl *decl = (struct mixin_decl *)ast;
            dec_child_refs(decl->methods);
            break;
        }
        case AST_OR_PATTERN: {
            struct or_pattern *pattern = (struct or_pattern *)ast;
            dec_child_refs(pattern->items);
            break;
        }
        case AST_RAISE_STMT: {
            struct raise_stmt *stmt = (struct raise_stmt *)ast;
            AST_DEC_REF(stmt->expr);
            break;
        }
        case AST_RETURN_STMT: {
            struct return_stmt *stmt = (struct return_stmt *)ast;
            AST_DEC_REF(stmt->val);
            break;
        }
        case AST_TRY_STMT: {
            struct try_stmt *stmt = (struct try_stmt *)ast;
            AST_DEC_REF(stmt->try_body);
            AST_DEC_REF(stmt->except_body);
            break;
        }
        case AST_TUPLE_EXPR: {
            struct tuple_expr *expr = (struct tuple_expr *)ast;
            dec_child_refs(expr->items);
            break;
        }
        case AST_UNARY_EXPR: {
            struct unary_expr *expr = (struct unary_expr *)ast;
            AST_DEC_REF(expr->expr);
            break;
        }
        case AST_USE_STMT: {
            struct use_stmt *stmt = (struct use_stmt *)ast;
            if (stmt->imports) dec_child_refs(stmt->imports);
            break;
        }
        case AST_WHEN_EXPR: {
            struct when_expr *expr = (struct when_expr *)ast;
            AST_DEC_REF(expr->cond);
            AST_DEC_REF(expr->true_val);
            AST_DEC_REF(expr->false_val);
            break;
        }
        case AST_WHILE_STMT: {
            struct while_stmt *stmt = (struct while_stmt *)ast;
            AST_DEC_REF(stmt->cond);
            AST_DEC_REF(stmt->body);
            break;
        }
        case AST_WITH_STMT: {
            struct with_stmt *stmt = (struct with_stmt *)ast;
            AST_DEC_REF(stmt->expr);
            AST_DEC_REF(stmt->body);
            break;
        }
        default: {
            break;
        }
    }
}

GaObject *
_GaAst_init()
{
    _GaAst_type = GaObj_NewType("Ast", NULL);
    assign_methods(_GaAst_type, NULL);
    return GaObj_INC_REF(_GaAst_type);
}

void
_GaAst_fini()
{
    GaObj_XDEC_REF(_GaAst_type);
}

void
GaAst_Destroy(struct ast_node *root)
{
    GaObj_DEC_REF(&root->object);
}