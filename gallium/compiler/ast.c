#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <compiler/ast.h>
#include <gallium/list.h>
#include <gallium/stringbuf.h>

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
code_block_new(struct list *children)
{
    struct code_block *node = AST_NODE_NEW(struct code_block, AST_CODE_BLOCK);
    node->children = children;
    return (struct ast_node*)node;
}

struct ast_node *
class_decl_new(const char *name, struct ast_node *base, struct list *methods)
{
    size_t name_len = strlen(name);
    struct class_decl *node = (struct class_decl*)ast_node_new(AST_CLASS_DECL,
            sizeof(struct class_decl) + name_len + 1);
    strncpy(node->name, name, name_len);
    node->base = base;
    node->methods = methods;
    return (struct ast_node*)node;
}

struct ast_node *
func_decl_new(const char *name, struct list *parameters, struct ast_node *body)
{
    size_t name_len = strlen(name);
    struct func_decl *node = (struct func_decl*)ast_node_new(AST_FUNC_DECL,
            sizeof(struct func_decl) + name_len + 1);

    strncpy(node->name, name, name_len);
    node->parameters = parameters;
    node->body = body;
    return (struct ast_node*)node;
}

struct ast_node *
func_expr_new(struct list *parameters, struct ast_node *body)
{
    struct func_decl *node = AST_NODE_NEW(struct func_decl, AST_FUNC_EXPR);
    node->parameters = parameters;
    node->body = body;
    return (struct ast_node*)node;
}

struct ast_node *
func_param_new(const char *name)
{
    size_t name_len = strlen(name);
    struct func_param *param = (struct func_param*)ast_node_new(AST_FUNC_PARAM,
            sizeof(struct func_param) + name_len + 1);
    strncpy(param->name, name, name_len);
    return (struct ast_node*)param;
}

/*
struct ast_node *
macro_decl_new(const char *name, struct list *parameters, struct ast_node *body)
{
    size_t name_len = strlen(name);
    struct macro_decl *node = (struct macro_decl*)ast_node_new(AST_MACRO_DECL,
            sizeof(struct macro_decl) + name_len + 1);

    strncpy(node->name, name, name_len);
    node->parameters = parameters;
    node->body = body;
    return (struct ast_node*)node;
}*/

struct ast_node *
break_stmt_new()
{
    return AST_NODE_NEW(struct ast_node, AST_BREAK_STMT);
}

struct ast_node *
continue_stmt_new()
{
    return AST_NODE_NEW(struct ast_node, AST_CONTINUE_STMT);
}

struct ast_node *
for_stmt_new(const char *var_name, struct ast_node *expr, struct ast_node *body)
{
    size_t name_len = strlen(var_name);
    struct for_stmt *node = (struct for_stmt*)ast_node_new(AST_FOR_STMT,
            sizeof(struct for_stmt) + name_len + 1);
    strncpy(node->var_name, var_name, name_len);
    node->expr = expr;
    node->body = body;
    return (struct ast_node*)node;
}

struct ast_node *
if_stmt_new(struct ast_node *cond, struct ast_node *if_body, struct ast_node *else_body)
{
    struct if_stmt *node = AST_NODE_NEW(struct if_stmt, AST_IF_STMT);
    node->cond = cond;
    node->if_body = if_body;
    node->else_body = else_body;
    return (struct ast_node*)node;
}

struct ast_node *
return_stmt_new(struct ast_node *val)
{
    struct return_stmt *node = AST_NODE_NEW(struct return_stmt, AST_RETURN_STMT);
    node->val = val;
    return (struct ast_node*)node;
}

struct ast_node *
try_stmt_new(struct ast_node *try_body, struct ast_node *except_body, const char *varname)
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
        strncpy(node->var_name, varname, name_len);
    }

    return (struct ast_node*)node;
}

struct ast_node *
while_stmt_new(struct ast_node *cond, struct ast_node *body)
{
    struct while_stmt *node = AST_NODE_NEW(struct while_stmt, AST_WHILE_STMT);
    node->cond = cond;
    node->body = body;
    return (struct ast_node*)node;
}

struct ast_node *
call_expr_new(struct ast_node *target, struct list *arguments)
{
    struct call_expr *node = AST_NODE_NEW(struct call_expr, AST_CALL_EXPR);
    node->arguments = arguments;
    node->target = target;
    return (struct ast_node*)node;
}

struct ast_node *
call_macro_expr_new(struct ast_node *target, struct list *expr_list, struct list *token_list)
{
    struct call_macro_expr *node = AST_NODE_NEW(struct call_macro_expr, AST_CALL_MACRO_EXPR);
    node->expr_list = expr_list;
    node->token_list = token_list;
    node->target = target;
    return (struct ast_node*)node;
}

struct ast_node *
assign_expr_new(struct ast_node *left, struct ast_node *right)
{
    struct assign_expr *node = AST_NODE_NEW(struct assign_expr, AST_ASSIGN_EXPR);
    node->left = left;
    node->right = right;
    return (struct ast_node*)node;
}

struct ast_node *
bin_expr_new(binop_t op, struct ast_node *left, struct ast_node *right)
{
    struct bin_expr *node = AST_NODE_NEW(struct bin_expr, AST_BIN_EXPR);
    node->left = left;
    node->right = right;
    node->op = op;
    return (struct ast_node*)node;
}

struct ast_node *
dict_expr_new(struct list *kvp_pairs)
{
    struct dict_expr *node = AST_NODE_NEW(struct dict_expr, AST_DICT_EXPR);
    node->kvp_pairs = kvp_pairs;
    return (struct ast_node*)node;
}

struct ast_node *
key_val_expr_new(struct ast_node *key, struct ast_node *val)
{
    struct key_val_expr *node = AST_NODE_NEW(struct key_val_expr, AST_KEY_VAL_EXPR);
    node->key = key;
    node->val = val;
    return (struct ast_node*)node;
}

struct ast_node *
unary_expr_new(unaryop_t op, struct ast_node *expr)
{
    struct unary_expr *node = AST_NODE_NEW(struct unary_expr, AST_UNARY_EXPR);
    node->expr = expr;
    node->op = op;
    return (struct ast_node*)node;
}

struct ast_node *
bool_term_new(bool val)
{
    struct bool_term *node = AST_NODE_NEW(struct bool_term, AST_BOOL_TERM);
    node->val = val;
    return (struct ast_node*)node;
}

struct ast_node *
integer_term_new(int64_t val)
{
    struct integer_term *node = AST_NODE_NEW(struct integer_term, AST_INTEGER_TERM);
    node->val = val;
    return (struct ast_node*)node;
}

struct ast_node *
string_term_new(struct stringbuf *sb)
{
    size_t str_len = STRINGBUF_LEN(sb);
    struct string_term *node = (struct string_term*)ast_node_new(AST_STRING_TERM,
            sizeof(struct string_term) + str_len + 1);
    memcpy(node->val, STRINGBUF_VALUE(sb), str_len);
    return (struct ast_node*)node;
}

struct ast_node *
symbol_term_new(const char *name)
{
    size_t name_len = strlen(name);
    struct symbol_term *node = (struct symbol_term*)ast_node_new(AST_SYMBOL_TERM,
            sizeof(struct symbol_term) + name_len + 1);
    strncpy(node->name, name, name_len);
    return (struct ast_node*)node;
}

struct ast_node *
index_access_expr_new(struct ast_node *expr, struct ast_node *key)
{
    struct index_access_expr *node = AST_NODE_NEW(struct index_access_expr, AST_INDEX_ACCESS_EXPR);
    node->expr = expr;
    node->key = key;
    return (struct ast_node*)node;
}

struct ast_node *
list_expr_new(struct list *items)
{
    struct list_expr *node = AST_NODE_NEW(struct list_expr, AST_LIST_EXPR);
    node->items = items;
    return (struct ast_node*)node;
}

struct ast_node *
member_access_expr_new(struct ast_node *expr, const char *member)
{
    size_t member_len = strlen(member);
    struct member_access_expr *node = (struct member_access_expr*)ast_node_new(AST_MEMBER_ACCESS_EXPR,
            sizeof(struct member_access_expr) + member_len + 1);
    strncpy(node->member, member, member_len);
    node->expr = expr;
    return (struct ast_node*)node;
}

struct ast_node *
quote_expr_new(struct list *children)
{
    struct quote_expr *node = AST_NODE_NEW(struct quote_expr, AST_QUOTE_EXPR);
    node->children = children;
    return (struct ast_node*)node;
}

struct ast_node *
tuple_expr_new(struct list *items)
{
    struct tuple_expr *node = AST_NODE_NEW(struct tuple_expr, AST_TUPLE_EXPR);
    node->items = items;
    return (struct ast_node*)node;
}

static void
ast_destroy_cb(struct ast_node *node, void *statep)
{
    if (node != &ast_empty_stmt_inst) {
        free(node);
    }
}

void
ast_destroy(struct ast_node *root)
{
    ast_walk(root, ast_destroy_cb, NULL);
}

void
ast_walk(struct ast_node *root, ast_walk_t walk_func, void *statep)
{
    switch (root->type) {
        case AST_CODE_BLOCK: {
            struct code_block *block = (struct code_block*)root;
            struct ast_node *child;
            list_iter_t iter;
            list_get_iter(block->children, &iter);

            while (iter_next_elem(&iter, (void**)&child)) {
                ast_walk(child, walk_func, statep);
            }
            walk_func(root, statep);
            break;
        }
        case AST_IF_STMT: {
            struct if_stmt *stmt = (struct if_stmt*)root;
            ast_walk(stmt->cond, walk_func, statep);
            ast_walk(stmt->if_body, walk_func, statep);
            
            if (stmt->else_body) {
                ast_walk(stmt->else_body, walk_func, statep);
            }

            walk_func(root, statep);
            break;
        }
        case AST_WHILE_STMT: {
            struct while_stmt *stmt = (struct while_stmt*)root;
            ast_walk(stmt->cond, walk_func, statep);
            ast_walk(stmt->body, walk_func, statep);
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
            ast_walk(expr->left, walk_func, statep);
            ast_walk(expr->right, walk_func, statep);
            walk_func(root, statep);
            break;
        }
        case AST_CALL_EXPR: {
            struct call_expr *expr = (struct call_expr*)root;
            struct ast_node *arg;
            list_iter_t iter;
            list_get_iter(expr->arguments, &iter);

            while (iter_next_elem(&iter, (void**)&arg)) {
                ast_walk(arg, walk_func, statep);
            }
            walk_func(root, statep);
            break;
        }
        case AST_DICT_EXPR: {
            struct dict_expr *expr = (struct dict_expr*)root;
            struct ast_node *kvp;
            list_iter_t iter;
            list_get_iter(expr->kvp_pairs, &iter);

            while (iter_next_elem(&iter, (void**)&kvp)) {
                ast_walk(kvp, walk_func, statep);
            }

            walk_func(root, statep);
            break;
        }
        case AST_KEY_VAL_EXPR: {
            struct key_val_expr *expr = (struct key_val_expr*)root;
            ast_walk(expr->key, walk_func, statep);
            ast_walk(expr->val, walk_func, statep);
            ast_walk(root, walk_func, statep);
            break;
        }
        default:
            break;
    }
}
