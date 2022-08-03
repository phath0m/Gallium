#ifndef _GALLIUM_COMPILER_H
#define _GALLIUM_COMPILER_H

#include <gallium/object.h>
#include <gallium/vm.h>
#include "parser.h"

#define COMPILER_SYNTAX_ERROR   1

struct compiler_state {
    struct ga_mod_data  *   mod_data;
    struct parser_state     parse_state;
    int                     comp_errno;
    GaContext           *   ctx;
};

struct ast_node;

void             compiler_init(GaContext *, struct compiler_state *);
GaObject    *    GaCode_Compile(GaContext *, const char *);
GaObject    *    GaAst_Compile(GaContext *, struct compiler_state *, struct ast_node *);
GaObject    *    GaAst_CompileInline(GaContext *, struct ga_proc *, struct ast_node *);
void             compiler_explain(struct compiler_state *);

#endif
