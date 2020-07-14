#include <stdio.h>
#include <stdlib.h>
#include <compiler/parser.h>
#include <gallium/builtins.h>
#include <gallium/list.h>
#include <gallium/object.h>
#include <gallium/vm.h>

GA_BUILTIN_TYPE_DECL(ga_tokenstream_type_inst, "Tokenstream", NULL);

struct tokenstream_state {
    struct parser_state     parser_state;
    struct list         *   tokens;
};

static struct ga_obj *
tokenstream_accept_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        vm_raise_exception(vm, ga_argument_error_new("parse() requires one argument"));
        return NULL;
    }

    struct ga_obj *int_arg = ga_obj_super(args[0], &ga_int_type_inst);

    if (!int_arg) {
        vm_raise_exception(vm, ga_type_error_new("Int"));
        return NULL;
    }

    struct tokenstream_state *statep = self->un.statep;

    return ga_bool_from_bool(parser_accept_tok_class(&statep->parser_state, (token_class_t)ga_int_to_i64(int_arg)));
}

static struct ga_obj *
tokenstream_empty_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 0) {
        vm_raise_exception(vm, ga_argument_error_new("empty() requires zero arguments"));
        return NULL;
    }

    struct tokenstream_state *statep = self->un.statep;

    return ga_bool_from_bool(parser_peek_tok(&statep->parser_state) == NULL);
}

static struct ga_obj *
tokenstream_parse_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 0) {
        vm_raise_exception(vm, ga_argument_error_new("parse() requires zero arguments"));
        return NULL;
    }

    struct tokenstream_state *statep = self->un.statep;
    struct ast_node *node = parser_parse_all(&statep->parser_state);

    if (!node) {
        printf("syntax error\n");
        /* raise syntax exception */
        return NULL;
    }

    return ga_ast_node_new(node, NULL);
}

static struct ga_obj *
tokenstream_parse_expr_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 0) {
        vm_raise_exception(vm, ga_argument_error_new("parse_expr() requires zero arguments"));
        return NULL;
    }

    struct tokenstream_state *statep = self->un.statep;
    struct ast_node *node = parser_parse_expr(&statep->parser_state);

    if (!node) {
        /* raise syntax exception */
        return NULL;
    }

    return ga_ast_node_new(node, NULL);
}

static struct ga_obj *
tokenstream_parse_stmt_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 0) {
        vm_raise_exception(vm, ga_argument_error_new("parse_stmt() requires zero arguments"));
        return NULL;
    }

    struct tokenstream_state *statep = self->un.statep;
    struct ast_node *node = parser_parse_stmt(&statep->parser_state);

    if (!node) {
        /* raise syntax exception */
        return NULL;
    }

    return ga_ast_node_new(node, NULL);
}

struct ga_obj *
ga_tokenstream_new(struct list *tokens)
{
    struct ga_obj *obj = ga_obj_new(&ga_tokenstream_type_inst, NULL);
    struct tokenstream_state *statep = calloc(sizeof(struct tokenstream_state), 1);
    
    statep->tokens = tokens;
    obj->un.statep = statep;

    parser_init_lazy(&statep->parser_state, tokens);

    GAOBJ_SETATTR(obj, NULL, "parse", ga_builtin_new(tokenstream_parse_method, obj));
    GAOBJ_SETATTR(obj, NULL, "parse_expr", ga_builtin_new(tokenstream_parse_expr_method, obj));
    GAOBJ_SETATTR(obj, NULL, "parse_stmt", ga_builtin_new(tokenstream_parse_stmt_method, obj));
    GAOBJ_SETATTR(obj, NULL, "accept", ga_builtin_new(tokenstream_accept_method, obj));
    GAOBJ_SETATTR(obj, NULL, "empty", ga_builtin_new(tokenstream_empty_method, obj));

    return obj;
}

struct ga_obj *
ga_parser_mod_open()
{
    static struct ga_obj *mod = NULL;

    if (mod) {
        return mod;
    }

    mod = ga_mod_new("parser", NULL);

    GAOBJ_SETATTR(mod, NULL, "TOK_KEYWORD", ga_int_from_i64((int64_t)TOK_KEYWORD));
    GAOBJ_SETATTR(mod, NULL, "TOK_IDENT", ga_int_from_i64((int64_t)TOK_IDENT));
    GAOBJ_SETATTR(mod, NULL, "TOK_AND", ga_int_from_i64((int64_t)TOK_AND));
    GAOBJ_SETATTR(mod, NULL, "TOK_OR", ga_int_from_i64((int64_t)TOK_OR));
    GAOBJ_SETATTR(mod, NULL, "TOK_XOR", ga_int_from_i64((int64_t)TOK_XOR));
    GAOBJ_SETATTR(mod, NULL, "TOK_ADD", ga_int_from_i64((int64_t)TOK_ADD));
    GAOBJ_SETATTR(mod, NULL, "TOK_SUB", ga_int_from_i64((int64_t)TOK_SUB));
    GAOBJ_SETATTR(mod, NULL, "TOK_MUL", ga_int_from_i64((int64_t)TOK_MUL));
    GAOBJ_SETATTR(mod, NULL, "TOK_DIV", ga_int_from_i64((int64_t)TOK_DIV));
    GAOBJ_SETATTR(mod, NULL, "TOK_MOD", ga_int_from_i64((int64_t)TOK_MOD));
    GAOBJ_SETATTR(mod, NULL, "TOK_ASSIGN", ga_int_from_i64((int64_t)TOK_ASSIGN));
    GAOBJ_SETATTR(mod, NULL, "TOK_EQUALS", ga_int_from_i64((int64_t)TOK_EQUALS));
    GAOBJ_SETATTR(mod, NULL, "TOK_NOT_EQUALS", ga_int_from_i64((int64_t)TOK_NOT_EQUALS));
    GAOBJ_SETATTR(mod, NULL, "TOK_GT", ga_int_from_i64((int64_t)TOK_GREATER_THAN));
    GAOBJ_SETATTR(mod, NULL, "TOK_LT", ga_int_from_i64((int64_t)TOK_LESS_THAN));
    GAOBJ_SETATTR(mod, NULL, "TOK_GE", ga_int_from_i64((int64_t)TOK_GREATER_THAN_OR_EQU));
    GAOBJ_SETATTR(mod, NULL, "TOK_LE", ga_int_from_i64((int64_t)TOK_LESS_THAN_OR_EQU));
    GAOBJ_SETATTR(mod, NULL, "TOK_LOGICAL_AND", ga_int_from_i64((int64_t)TOK_LOGICAL_AND));
    GAOBJ_SETATTR(mod, NULL, "TOK_LOGICAL_OR", ga_int_from_i64((int64_t)TOK_LOGICAL_OR));
    GAOBJ_SETATTR(mod, NULL, "TOK_LOGICAL_NOT", ga_int_from_i64((int64_t)TOK_LOGICAL_NOT));
    GAOBJ_SETATTR(mod, NULL, "TOK_NOT", ga_int_from_i64((int64_t)TOK_NOT));
    GAOBJ_SETATTR(mod, NULL, "TOK_DOT", ga_int_from_i64((int64_t)TOK_DOT));
    GAOBJ_SETATTR(mod, NULL, "TOK_COMMA", ga_int_from_i64((int64_t)TOK_COMMA));
    GAOBJ_SETATTR(mod, NULL, "TOK_STRING", ga_int_from_i64((int64_t)TOK_STRING_LIT));
    GAOBJ_SETATTR(mod, NULL, "TOK_INT", ga_int_from_i64((int64_t)TOK_INT_LIT));
    GAOBJ_SETATTR(mod, NULL, "TOK_PHAT_ARROW", ga_int_from_i64((int64_t)TOK_PHAT_ARROW));
    GAOBJ_SETATTR(mod, NULL, "TOK_CLOSED_RANGE", ga_int_from_i64((int64_t)TOK_CLOSED_RANGE));
    GAOBJ_SETATTR(mod, NULL, "TOK_HALF_RANGE", ga_int_from_i64((int64_t)TOK_HALF_RANGE));
    GAOBJ_SETATTR(mod, NULL, "TOK_SHL", ga_int_from_i64((int64_t)TOK_SHL));
    GAOBJ_SETATTR(mod, NULL, "TOK_SHR", ga_int_from_i64((int64_t)TOK_SHR));
    GAOBJ_SETATTR(mod, NULL, "TOK_BACKTICK", ga_int_from_i64((int64_t)TOK_BACKTICK));
    GAOBJ_SETATTR(mod, NULL, "TOK_OPEN_BRACKET", ga_int_from_i64((int64_t)TOK_OPEN_BRACKET));
    GAOBJ_SETATTR(mod, NULL, "TOK_CLOSE_BRACKET", ga_int_from_i64((int64_t)TOK_CLOSE_BRACKET));

    return mod;
}

