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
#include <gallium/list.h>
#include <gallium/stringbuf.h>
#include "ast.h"

struct ast_node ast_empty_stmt_inst = {
    .type = AST_EMPTY_STMT
};

static struct ast_node *
ast_node_new(ast_class_t type, size_t size)
{
    struct ast_node *node = calloc(size, 1);
    node->type = type;
    return node;
}

/* is this a hack? I'm not sure. Makes my life easier though */
#define AST_NODE_NEW(t, c)  (t*)ast_node_new((c), sizeof(t))

struct ast_node *
GaAst_NewCodeBlock(_Ga_list_t *children)
{
    struct code_block *node = AST_NODE_NEW(struct code_block, AST_CODE_BLOCK);
    node->children = children;
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
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewFuncParam(const char *name)
{
    size_t name_len = strlen(name);
    struct func_param *param = (struct func_param*)ast_node_new(AST_FUNC_PARAM,
            sizeof(struct func_param) + name_len + 1);
    strcpy(param->name, name);
    return (struct ast_node*)param;
}

/*
struct ast_node *
macro_decl_new(const char *name, _Ga_list_t *parameters, struct ast_node *body)
{
    size_t name_len = strlen(name);
    struct macro_decl *node = (struct macro_decl*)ast_node_new(AST_MACRO_DECL,
            sizeof(struct macro_decl) + name_len + 1);

    strncpy(node->name, name, name_len + 1);
    node->parameters = parameters;
    node->body = body;
    return (struct ast_node*)node;
}*/

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
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewIf(struct ast_node *cond, struct ast_node *if_body, struct ast_node *else_body)
{
    struct if_stmt *node = AST_NODE_NEW(struct if_stmt, AST_IF_STMT);
    node->cond = cond;
    node->if_body = if_body;
    node->else_body = else_body;
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewReturn(struct ast_node *val)
{
    struct return_stmt *node = AST_NODE_NEW(struct return_stmt, AST_RETURN_STMT);
    node->val = val;
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

    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewWhile(struct ast_node *cond, struct ast_node *body)
{
    struct while_stmt *node = AST_NODE_NEW(struct while_stmt, AST_WHILE_STMT);
    node->cond = cond;
    node->body = body;
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewCall(struct ast_node *target, _Ga_list_t *arguments)
{
    struct call_expr *node = AST_NODE_NEW(struct call_expr, AST_CALL_EXPR);
    node->arguments = arguments;
    node->target = target;
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewMacro(struct ast_node *target, _Ga_list_t *token_list)
{
    struct call_macro_expr *node = AST_NODE_NEW(struct call_macro_expr, AST_CALL_MACRO_EXPR);
    node->token_list = token_list;
    node->target = target;
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewExpr(struct ast_node *left, struct ast_node *right)
{
    struct assign_expr *node = AST_NODE_NEW(struct assign_expr, AST_ASSIGN_EXPR);
    node->left = left;
    node->right = right;
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewBinOp(binop_t op, struct ast_node *left, struct ast_node *right)
{
    struct bin_expr *node = AST_NODE_NEW(struct bin_expr, AST_BIN_EXPR);
    node->left = left;
    node->right = right;
    node->op = op;
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewDict(_Ga_list_t *kvp_pairs)
{
    struct dict_expr *node = AST_NODE_NEW(struct dict_expr, AST_DICT_EXPR);
    node->kvp_pairs = kvp_pairs;
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewKeyValuePair(struct ast_node *key, struct ast_node *val)
{
    struct key_val_expr *node = AST_NODE_NEW(struct key_val_expr, AST_KEY_VAL_EXPR);
    node->key = key;
    node->val = val;
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewUnaryOp(unaryop_t op, struct ast_node *expr)
{
    struct unary_expr *node = AST_NODE_NEW(struct unary_expr, AST_UNARY_EXPR);
    node->expr = expr;
    node->op = op;
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
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewList(_Ga_list_t *items)
{
    struct list_expr *node = AST_NODE_NEW(struct list_expr, AST_LIST_EXPR);
    node->items = items;
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewMatch(struct ast_node *expr, _Ga_list_t *cases, struct ast_node *default_case)
{
    struct match_expr *node = AST_NODE_NEW(struct match_expr, AST_MATCH_EXPR);
    node->expr = expr;
    node->cases = cases;
    node->default_case = default_case;
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewCase(struct ast_node *pattern, struct ast_node *cond, struct ast_node *value)
{
    struct match_case *node = AST_NODE_NEW(struct match_case, AST_MATCH_CASE);
    node->pattern = pattern;
    node->cond = cond;
    node->value = value;
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewListPattern(_Ga_list_t *items)
{
    struct list_expr *node = AST_NODE_NEW(struct list_expr, AST_LIST_PATTERN);
    node->items = items;
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewOrPattern(_Ga_list_t *items)
{
    struct list_expr *node = AST_NODE_NEW(struct list_expr, AST_OR_PATTERN);
    node->items = items;
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
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewQuote(_Ga_list_t *children)
{
    struct quote_expr *node = AST_NODE_NEW(struct quote_expr, AST_QUOTE_EXPR);
    node->children = children;
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewTuple(_Ga_list_t *items)
{
    struct tuple_expr *node = AST_NODE_NEW(struct tuple_expr, AST_TUPLE_EXPR);
    node->items = items;
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewWhen(struct ast_node *true_val, struct ast_node *cond, struct ast_node *false_val)
{
    struct when_expr *node = AST_NODE_NEW(struct when_expr, AST_WHEN_EXPR);
    node->true_val = true_val;
    node->cond = cond;
    node->false_val = false_val;
    return (struct ast_node*)node;
}

struct ast_node *
GaAst_NewRaise(struct ast_node *expr)
{
    struct raise_stmt *node = AST_NODE_NEW(struct raise_stmt, AST_RAISE_STMT);
    node->expr = expr;
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
    return (struct ast_node*)node;
}

static void
_GaAst_AST_DESTROY_CB(struct ast_node *node, void *statep)
{
    if (node != &ast_empty_stmt_inst) free(node);
}

void
GaAst_Destroy(struct ast_node *root)
{
    GaAst_Walk(root, _GaAst_AST_DESTROY_CB, NULL);
}

void
GaAst_Walk(struct ast_node *root, ast_walk_t walk_func, void *statep)
{
    switch (root->type) {
        case AST_CODE_BLOCK: {
            struct code_block *block = (struct code_block*)root;
            struct ast_node *child;
            _Ga_iter_t iter;
            _Ga_list_get_iter(block->children, &iter);

            while (_Ga_iter_next(&iter, (void**)&child)) {
                GaAst_Walk(child, walk_func, statep);
            }
            walk_func(root, statep);
            break;
        }
        case AST_IF_STMT: {
            struct if_stmt *stmt = (struct if_stmt*)root;
            GaAst_Walk(stmt->cond, walk_func, statep);
            GaAst_Walk(stmt->if_body, walk_func, statep);
            
            if (stmt->else_body) {
                GaAst_Walk(stmt->else_body, walk_func, statep);
            }

            walk_func(root, statep);
            break;
        }
        case AST_WHILE_STMT: {
            struct while_stmt *stmt = (struct while_stmt*)root;
            GaAst_Walk(stmt->cond, walk_func, statep);
            GaAst_Walk(stmt->body, walk_func, statep);
            walk_func(root, statep);
            break;
        }
        case AST_FUNC_DECL:
        case AST_INTEGER_TERM:
        case AST_STRING_TERM:
        case AST_SYMBOL_TERM:
            walk_func(root, statep);
            break;
        case AST_BIN_EXPR: {
            struct bin_expr *expr = (struct bin_expr*)root;
            GaAst_Walk(expr->left, walk_func, statep);
            GaAst_Walk(expr->right, walk_func, statep);
            walk_func(root, statep);
            break;
        }
        case AST_CALL_EXPR: {
            struct call_expr *expr = (struct call_expr*)root;
            struct ast_node *arg;
            _Ga_iter_t iter;
            _Ga_list_get_iter(expr->arguments, &iter);

            while (_Ga_iter_next(&iter, (void**)&arg)) {
                GaAst_Walk(arg, walk_func, statep);
            }
            walk_func(root, statep);
            break;
        }
        case AST_DICT_EXPR: {
            struct dict_expr *expr = (struct dict_expr*)root;
            struct ast_node *kvp;
            _Ga_iter_t iter;
            _Ga_list_get_iter(expr->kvp_pairs, &iter);

            while (_Ga_iter_next(&iter, (void**)&kvp)) {
                GaAst_Walk(kvp, walk_func, statep);
            }

            walk_func(root, statep);
            break;
        }
        case AST_KEY_VAL_EXPR: {
            struct key_val_expr *expr = (struct key_val_expr*)root;
            GaAst_Walk(expr->key, walk_func, statep);
            GaAst_Walk(expr->val, walk_func, statep);
            GaAst_Walk(root, walk_func, statep);
            break;
        }
        default:
            break;
    }
}