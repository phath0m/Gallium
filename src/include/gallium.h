#ifndef _GALLIUM_H
#define _GALLIUM_H

#include <gallium/builtins.h>
#include <gallium/compiler.h>
#include <gallium/object.h>
#include <gallium/stringbuf.h>
#include <gallium/vm.h>

GaContext   *   Ga_New();
void            Ga_Close(GaContext *);
GaObject    *   Ga_DoFile(GaContext *, const char *);
GaObject    *   Ga_DoString(GaContext *, const char *);
void            Ga_SetGlobal(GaContext *, const char *, GaObject *);

#endif