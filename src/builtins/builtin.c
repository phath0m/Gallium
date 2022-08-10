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
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <gallium/builtins.h>
#include <gallium/object.h>
#include <gallium/vm.h>

GA_BUILTIN_TYPE_DECL(_GaBuiltin_Type, "Builtin", NULL);

static void         builtin_destroy(GaObject *);
static GaObject *   builtin_invoke(GaObject *, GaContext *, int, GaObject **);

static struct Ga_Operators builtin_ops = {
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
builtin_invoke(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    struct builtin_state *statep = self->un.statep;

    if (statep->self) {
        self = GaWeakRef_Val(statep->self);
        
        if (self == &_GaNull) {
            printf("BUG: Referenced 'self' object no longer exists!\n");
            return self;
        }
        GaObject *new_args[Ga_ARGUMENT_MAX];
        new_args[0] = self;
        assert(argc < Ga_ARGUMENT_MAX - 1);
        memcpy(&new_args[1], args, argc*sizeof(GaObject *));
        return statep->func(vm, argc + 1, new_args);
    } else {
        return statep->func(vm, argc, args);
    }
}

GaObject *
GaBuiltin_New(GaCFunc func, GaObject *self)
{
    GaObject *ret = GaObj_New(&_GaBuiltin_Type, &builtin_ops);
    struct builtin_state *statep = calloc(sizeof(struct builtin_state), 1);

    if (self) {
        statep->self = GaObj_INC_REF(GaWeakRef_New(self));
    }

    statep->func = func;
    ret->un.statep = statep;

    return ret;
}