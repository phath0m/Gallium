#ifndef _GALLIUM_COMPILER_H
#define _GALLIUM_COMPILER_H

#include <gallium/object.h>
#include <gallium/parser.h>
#include <gallium/vm.h>

#define COMPILER_SYNTAX_ERROR   1

struct compiler_state {
    struct ga_mod_data  *   mod_data;
    struct parser_state     parse_state;
    int                     comp_errno;
};

struct ast_node;

void                compiler_init(struct compiler_state *);
struct ga_obj   *   GaCode_Compile(struct compiler_state *, const char *);
struct ga_obj   *   GaAst_Compile(struct compiler_state *, struct ast_node *);
struct ga_obj   *   GaAst_CompileInline(struct compiler_state *, struct ga_proc *, struct ast_node *);
void                compiler_explain(struct compiler_state *);

#endif
