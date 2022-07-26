#ifndef _GALLIUM_AST_H
#define _GALLIUM_AST_H

#include <stdbool.h>
#include <stdint.h>
#include <gallium/list.h>
#include <gallium/stringbuf.h>

typedef enum {
    AST_ASSIGN_EXPR,
    AST_CODE_BLOCK,
    AST_FUNC_DECL,
    AST_FUNC_PARAM,
    AST_IF_STMT,
    AST_RETURN_STMT,
    AST_TRY_STMT,
    AST_WHILE_STMT,
    AST_FOR_STMT,
    AST_CALL_EXPR,
    AST_BIN_EXPR,
    AST_UNARY_EXPR,
    AST_INTEGER_TERM,
    AST_STRING_TERM,
    AST_SYMBOL_TERM,
    AST_MEMBER_ACCESS_EXPR,
    AST_INDEX_ACCESS_EXPR,
    AST_KEY_VAL_EXPR,
    AST_DICT_EXPR,
    AST_TUPLE_EXPR,
    AST_CLASS_DECL,
    AST_LIST_EXPR,
    AST_FUNC_EXPR,
    AST_BOOL_TERM,
    AST_EMPTY_STMT,
    AST_BREAK_STMT,
    AST_CONTINUE_STMT,
    AST_CALL_MACRO_EXPR,
    AST_QUOTE_EXPR,
    AST_MATCH_EXPR,
    AST_MATCH_CASE,
    AST_USE_STMT,
    AST_WHEN_EXPR,
    AST_LIST_PATTERN,
    AST_OR_PATTERN,
    AST_ENUM_DECL,
    AST_MIXIN_DECL
} ast_class_t;

struct ast_node {
    ast_class_t type;
};

struct code_block {
    struct ast_node _header;
    struct list *   children;
};

struct class_decl {
    struct ast_node     _header;
    struct ast_node *   base;
    struct list     *   methods;
    struct list     *   mixins;
    char                name[];
};

struct enum_decl {
    struct ast_node     _header;
    struct list     *   values;
    char                name[];
};

struct mixin_decl {
    struct ast_node     _header;
    struct list     *   methods;
    char                name[];
};

struct func_decl {
    struct ast_node     _header;
    struct list *       parameters;
    struct ast_node *   body;
    char                name[];
};

struct func_param {
    struct ast_node _header;
    int             flags;
    char            name[];
};

struct macro_decl {
    struct ast_node     _header;
    struct list *       params;
    char                name[];
};

struct for_stmt {
    struct ast_node     _header;
    struct ast_node *   expr; 
    struct ast_node *   body;
    /* TODO: implement unpacking so we can unpack the result from the iterator into multiple variables... */
    char                var_name[];
};

struct if_stmt {
    struct ast_node     _header;
    struct ast_node *   cond;
    struct ast_node *   if_body;
    struct ast_node *   else_body;
};

struct return_stmt {
    struct ast_node     _header;
    struct ast_node *   val;
};

struct try_stmt {
    struct ast_node     _header;
    struct ast_node *   try_body;
    struct ast_node *   except_body;
    bool                has_var;
    char                var_name[];
};

struct use_stmt {
    struct ast_node     _header;
    bool                wildcard;
    struct list     *   imports;
    char                import_path[];
};

struct while_stmt {
    struct ast_node     _header;
    struct ast_node *   cond;
    struct ast_node *   body;
};

struct assign_expr {
    struct ast_node     _header;
    struct ast_node *   left;
    struct ast_node *   right;
};

struct call_expr {
    struct ast_node     _header;
    struct ast_node *   target;
    struct list     *   arguments;
};

struct call_macro_expr {
    struct ast_node     _header;
    struct ast_node *   target;
    struct list     *   token_list;
};

struct dict_expr {
    struct ast_node     _header;
    struct list     *   kvp_pairs;
};

struct index_access_expr {
    struct ast_node     _header;
    struct ast_node *   expr;
    struct ast_node *   key;
};

struct key_val_expr {
    struct ast_node     _header;
    struct ast_node *   key;
    struct ast_node *   val;
};

struct list_expr {
    struct ast_node     _header;
    struct list     *   items;
};

struct match_expr {
    struct ast_node     _header;
    struct ast_node *   expr;
    struct list     *   cases;
    struct ast_node *   default_case;
};

struct match_case {
    struct ast_node     _header;
    struct ast_node *   pattern;
    struct ast_node *   cond;
    struct ast_node *   value;
};

struct list_pattern {
    struct ast_node     _header;
    struct list     *   items;
};

struct or_pattern {
    struct ast_node     _header;
    struct list     *   items;
};

struct member_access_expr {
    struct ast_node     _header;
    struct ast_node *   expr;
    char                member[];
};

struct quote_expr {
    struct ast_node _header;
    struct list *   children;
};

struct tuple_expr {
    struct ast_node     _header;
    struct list     *   items;
};

struct when_expr {
    struct ast_node     _header;
    struct ast_node *   cond;
    struct ast_node *   true_val;
    struct ast_node *   false_val;
};

typedef enum {
    BINOP_ADD,
    BINOP_SUB,
    BINOP_MUL,
    BINOP_DIV,
    BINOP_MOD,
    BINOP_AND,
    BINOP_OR,
    BINOP_XOR,
    BINOP_EQUALS,
    BINOP_NOT_EQUALS,
    BINOP_LOGICAL_AND,
    BINOP_LOGICAL_OR,
    BINOP_GREATER_THAN,
    BINOP_LESS_THAN,
    BINOP_GREATER_THAN_OR_EQU,
    BINOP_LESS_THAN_OR_EQU,
    BINOP_MEMBER_ACCESS,
    BINOP_ASSIGN,
    BINOP_HALF_RANGE,
    BINOP_CLOSED_RANGE,
    BINOP_SHL,
    BINOP_SHR
} binop_t;

typedef enum {
    UNARYOP_NEGATE,
    UNARYOP_NOT,
    UNARYOP_LOGICAL_NOT
} unaryop_t;

struct bin_expr {
    struct ast_node     _header;
    struct ast_node *   left;
    struct ast_node *   right;
    binop_t             op;
};

struct unary_expr {
    struct ast_node     _header;
    struct ast_node *   expr;
    unaryop_t           op;
};

struct bool_term {
    struct ast_node     _header;
    bool                val;
};

struct integer_term {
    struct ast_node     _header;
    int64_t             val;
};

struct string_term {
    struct ast_node     _header;
    size_t              length;
    char                val[];
};

struct symbol_term {
    struct ast_node     _header;
    char                name[];
};


__attribute__((always_inline))
static inline bool 
AST_IS_TERMINAL(struct ast_node *node)
{
    switch (node->type) {
        case AST_SYMBOL_TERM:
        case AST_STRING_TERM:
        case AST_INTEGER_TERM:
            return true;
        default:
            return false;
    }
}

extern struct ast_node  ast_empty_stmt_inst;

struct ast_node *   code_block_new(struct list *);
struct ast_node *   class_decl_new(const char *, struct ast_node *, struct list *, struct list *);
struct ast_node *   enum_decl_new(const char *, struct list *);
struct ast_node *   mixin_decl_new(const char *, struct list *);
struct ast_node *   func_decl_new(const char *, struct list *, struct ast_node *);
struct ast_node *   func_expr_new(struct list *, struct ast_node *);
struct ast_node *   func_param_new(const char *);
struct ast_node *   macro_decl_new(const char *, struct list *, struct ast_node *);
struct ast_node *   break_stmt_new();
struct ast_node *   continue_stmt_new();
struct ast_node *   for_stmt_new(const char *, struct ast_node *, struct ast_node *);
struct ast_node *   if_stmt_new(struct ast_node *, struct ast_node *, struct ast_node *);
struct ast_node *   return_stmt_new(struct ast_node *);
struct ast_node *   try_stmt_new(struct ast_node *, struct ast_node *, const char *);
struct ast_node *   use_stmt_new(const char *, struct list *, bool);
struct ast_node *   while_stmt_new(struct ast_node *, struct ast_node *);
struct ast_node *   assign_expr_new(struct ast_node *, struct ast_node *);
struct ast_node *   call_expr_new(struct ast_node *, struct list *);
struct ast_node *   call_macro_expr_new(struct ast_node *, struct list *);
struct ast_node *   dict_expr_new(struct list *);
struct ast_node *   bin_expr_new(binop_t, struct ast_node *, struct ast_node *);
struct ast_node *   key_val_expr_new(struct ast_node *, struct ast_node *);
struct ast_node *   quote_expr_new(struct list *);
struct ast_node *   unary_expr_new(unaryop_t, struct ast_node *);
struct ast_node *   bool_term_new(bool);
struct ast_node *   integer_term_new(int64_t);
struct ast_node *   string_term_new(struct stringbuf *);
struct ast_node *   symbol_term_new(const char *);
struct ast_node *   list_expr_new(struct list *);
struct ast_node *   match_expr_new(struct ast_node *, struct list *, struct ast_node *);
struct ast_node *   match_case_new(struct ast_node *, struct ast_node *, struct ast_node *);
struct ast_node *   list_pattern_new(struct list *);
struct ast_node *   or_pattern_new(struct list *);
struct ast_node *   member_access_expr_new(struct ast_node *, const char *);
struct ast_node *   index_access_expr_new(struct ast_node *, struct ast_node *);
struct ast_node *   tuple_expr_new(struct list *);
struct ast_node *   when_expr_new(struct ast_node *, struct ast_node *, struct ast_node *);

typedef void (*ast_walk_t)  (struct ast_node *, void *);

void                        ast_destroy(struct ast_node *);
void                        ast_walk(struct ast_node *, ast_walk_t, void *);
void                        ast_list_destroy_cb(void *, void *);


#endif
