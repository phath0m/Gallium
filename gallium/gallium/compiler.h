#ifndef _GALLIUM_COMPILER_H
#define _GALLIUM_COMPILER_H

#include <compiler/parser.h>
#include <runtime/bytecode.h>
#include <gallium/object.h>

#define COMPILER_SYNTAX_ERROR   1

struct compiler_state {
    struct ga_mod_data  *   mod_data;
    struct parser_state     parse_state;
    int                     comp_errno;
};

struct ast_node;

void                compiler_init(struct compiler_state *);
struct ga_obj   *   compiler_compile(struct compiler_state *, const char *);
struct ga_obj   *   compiler_compile_ast(struct compiler_state *, struct ast_node *);
struct ga_obj   *   compiler_compile_inline(struct compiler_state *, struct ga_proc *, struct ast_node *);
void                compiler_explain(struct compiler_state *);

#endif
