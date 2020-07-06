#ifndef _EMIT_COMPILER_H
#define _EMIT_COMPILER_H

#include <gallium/object.h>

struct compiler {

};



void                compiler_init(struct compiler *);
struct ga_obj   *   compiler_compile(struct compiler *, const char *);

#endif
