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

    return obj;
}


