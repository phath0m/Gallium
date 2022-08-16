#ifndef _AST_H
#define _AST_H

#include <stdbool.h>
#include <stdint.h>
#include <gallium/list.h>
#include <gallium/object.h>
#include <gallium/stringbuf.h>

#define AST_CALL_PACKED     0x01    /* Whether or not a call node's last argument is to be "unpacked" */

#define AST_FUNC_VARIADIC   0x01    /* Whether or not a function parameter is for a storing variadic arguments */
#define AST_FUNC_KEYWORD    0x02    /* Whether or not a function parameter is for storing keyword arguments */

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
    AST_MIXIN_DECL,
    AST_RAISE_STMT,
    AST_LET_STMT,
    AST_FLOAT_TERM,
    AST_WITH_STMT
} ast_class_t;

struct ast_node {
    GaObject    object;
    ast_class_t type;
};

struct code_block {
    struct ast_node _header;
    _Ga_list_t *   children;
};

struct class_decl {
    struct ast_node     _header;
    struct ast_node *   base;
    _Ga_list_t      *   methods;
    _Ga_list_t      *   mixins;
    char                name[];
};

struct enum_decl {
    struct ast_node     _header;
    _Ga_list_t      *   values;
    char                name[];
};

struct mixin_decl {
    struct ast_node     _header;
    _Ga_list_t      *   methods;
    char                name[];
};

struct func_decl {
    struct ast_node     _header;
    _Ga_list_t      *   parameters;
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
    _Ga_list_t  *       params;
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
    _Ga_list_t      *   imports;
    char                import_path[];
};

struct while_stmt {
    struct ast_node     _header;
    struct ast_node *   cond;
    struct ast_node *   body;
};

struct with_stmt {
    struct ast_node     _header;
    struct ast_node *   expr;
    struct ast_node *   body;
};

struct let_stmt {
    struct ast_node     _header;
    struct ast_node *   right;
    char                var_name[];
};

struct assign_expr {
    struct ast_node     _header;
    struct ast_node *   left;
    struct ast_node *   right;
};

struct call_expr {
    struct ast_node     _header;
    struct ast_node *   target;
    int                 flags; /* Flags (Currently just whether or not to unpack the last argument) */
    _Ga_list_t      *   arguments;
};

struct call_macro_expr {
    struct ast_node     _header;
    struct ast_node *   target;
    _Ga_list_t      *   token_list;
};

struct dict_expr {
    struct ast_node     _header;
    _Ga_list_t      *   kvp_pairs;
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
    _Ga_list_t      *   items;
};

struct match_expr {
    struct ast_node     _header;
    struct ast_node *   expr;
    _Ga_list_t      *   cases;
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
    _Ga_list_t      *   items;
};

struct or_pattern {
    struct ast_node     _header;
    _Ga_list_t      *   items;
};

struct member_access_expr {
    struct ast_node     _header;
    struct ast_node *   expr;
    char                member[];
};

struct quote_expr {
    struct ast_node _header;
    _Ga_list_t  *   children;
};

struct tuple_expr {
    struct ast_node     _header;
    _Ga_list_t      *   items;
};

struct when_expr {
    struct ast_node     _header;
    struct ast_node *   cond;
    struct ast_node *   true_val;
    struct ast_node *   false_val;
};

struct raise_stmt {
    struct ast_node     _header;
    struct ast_node *   expr;
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

struct float_term {
    struct ast_node     _header;
    double              val;
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

struct ast_node *   GaAst_NewCodeBlock(_Ga_list_t *);
struct ast_node *   GaAst_NewClass(const char *, struct ast_node *, _Ga_list_t *, _Ga_list_t *);
struct ast_node *   GaAst_NewEnum(const char *, _Ga_list_t *);
struct ast_node *   GaAst_NewMixin(const char *, _Ga_list_t *);
struct ast_node *   GaAst_NewFunc(const char *, _Ga_list_t *, struct ast_node *);
struct ast_node *   GaAst_NewAnonymousFunc(_Ga_list_t *, struct ast_node *);
struct ast_node *   GaAst_NewFuncParam(const char *, int);
struct ast_node *   GaAst_NewBreak();
struct ast_node *   GaAst_NewContinue();
struct ast_node *   GaAst_NewFor(const char *, struct ast_node *, struct ast_node *);
struct ast_node *   GaAst_NewIf(struct ast_node *, struct ast_node *, struct ast_node *);
struct ast_node *   GaAst_NewReturn(struct ast_node *);
struct ast_node *   GaAst_NewRaise(struct ast_node *);
struct ast_node *   GaAst_NewTry(struct ast_node *, struct ast_node *, const char *);
struct ast_node *   GaAst_NewUse(const char *, _Ga_list_t *, bool);
struct ast_node *   GaAst_NewWhile(struct ast_node *, struct ast_node *);
struct ast_node *   GaAst_NewWith(struct ast_node *, struct ast_node *);
struct ast_node *   GaAst_NewExpr(struct ast_node *, struct ast_node *);
struct ast_node *   GaAst_NewCall(struct ast_node *, _Ga_list_t *, int);
struct ast_node *   GaAst_NewMacro(struct ast_node *, _Ga_list_t *);
struct ast_node *   GaAst_NewDict(_Ga_list_t *);
struct ast_node *   GaAst_NewBinOp(binop_t, struct ast_node *, struct ast_node *);
struct ast_node *   GaAst_NewKeyValuePair(struct ast_node *, struct ast_node *);
struct ast_node *   GaAst_NewQuote(_Ga_list_t *);
struct ast_node *   GaAst_NewUnaryOp(unaryop_t, struct ast_node *);
struct ast_node *   GaAst_NewBool(bool);
struct ast_node *   GaAst_NewFloat(double);
struct ast_node *   GaAst_NewInteger(int64_t);
struct ast_node *   GaAst_NewString(struct stringbuf *);
struct ast_node *   GaAst_NewSymbol(const char *);
struct ast_node *   GaAst_NewList(_Ga_list_t *);
struct ast_node *   GaAst_NewMatch(struct ast_node *, _Ga_list_t *, struct ast_node *);
struct ast_node *   GaAst_NewCase(struct ast_node *, struct ast_node *, struct ast_node *);
struct ast_node *   GaAst_NewListPattern(_Ga_list_t *);
struct ast_node *   GaAst_NewOrPattern(_Ga_list_t *);
struct ast_node *   GaAst_NewMemberAccess(struct ast_node *, const char *);
struct ast_node *   GaAst_NewIndexer(struct ast_node *, struct ast_node *);
struct ast_node *   GaAst_NewTuple(_Ga_list_t *);
struct ast_node *   GaAst_NewWhen(struct ast_node *, struct ast_node *, struct ast_node *);
struct ast_node *   GaAst_NewLet(const char *, struct ast_node *);

typedef void (*ast_walk_t)  (struct ast_node *, void *);

void                        GaAst_Destroy(struct ast_node *);
void                        GaAst_Walk(struct ast_node *, ast_walk_t, void *);
void                        _GaAst_ListDestroyCb(void *, void *);

#endif
