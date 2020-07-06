#ifndef _GALLIUM_COMPILER_H
#define _GALLIUM_COMPILER_H

#include <compiler/parser.h>
#include <gallium/object.h>

#define COMPILER_SYNTAX_ERROR   1

struct compiler_state {
    struct parser_state parse_state;
    int                 comp_errno;
};

void                compiler_init(struct compiler_state *);
struct ga_obj   *   compiler_compile(struct compiler_state *, const char *);
void                compiler_explain(struct compiler_state *);

#endif
