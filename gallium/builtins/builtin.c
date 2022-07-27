/*
 * builtin.c - Builtin function type
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <stdio.h>
#include <stdlib.h>
#include <gallium/builtins.h>
#include <gallium/object.h>
#include <gallium/vm.h>

GA_BUILTIN_TYPE_DECL(ga_cfunc_type_inst, "Builtin", NULL);

static void         builtin_destroy(GaObject *);
static GaObject *   builtin_invoke(GaObject *, struct vm *, int, GaObject **);

static struct ga_obj_ops builtin_ops = {
    .destroy    =   builtin_destroy,
    .invoke     =   builtin_invoke
};

struct builtin_state {
    GaCFunc        func;
    GaObject   *   self;
};

static void
builtin_destroy(GaObject *self)
{
    struct builtin_state *statep = self->un.statep;

    if (statep->self) {
        GaObj_DEC_REF(statep->self);
    }
}

static GaObject *
builtin_invoke(GaObject *self, struct vm *vm, int argc, GaObject **args)
{
    struct builtin_state *statep = self->un.statep;

    if (statep->self) {
        self = GaWeakRef_Val(statep->self);
        
        if (self == &ga_null_inst) {
            printf("BUG: Referenced 'self' object no longer exists!\n");
            return self;
        }
    }

    return statep->func(self, vm, argc, args);
}

GaObject *
GaBuiltin_New(GaCFunc func, GaObject *self)
{
    GaObject *ret = GaObj_New(&ga_cfunc_type_inst, &builtin_ops);
    struct builtin_state *statep = calloc(sizeof(struct builtin_state), 1);

    if (self) {
        statep->self = GaObj_INC_REF(GaWeakRef_New(self));
    }

    statep->func = func;
    ret->un.statep = statep;

    return ret;
}