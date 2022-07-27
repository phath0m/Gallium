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

static void                 ga_builtin_destroy(struct ga_obj *);
static struct ga_obj    *   ga_builtin_invoke(struct ga_obj *, struct vm *, int, struct ga_obj **);

struct ga_obj_ops builtin_obj_ops = {
    .destroy    =   ga_builtin_destroy,
    .invoke     =   ga_builtin_invoke
};

struct ga_builtin_state {
    ga_cfunc_t        func;
    struct ga_obj   *   self;
};

static void
ga_builtin_destroy(struct ga_obj *self)
{
    struct ga_builtin_state *statep = self->un.statep;

    if (statep->self) {
        GaObj_DEC_REF(statep->self);
    }
}

struct ga_obj *
GaBuiltin_New(ga_cfunc_t func, struct ga_obj *self)
{
    struct ga_obj *ret = GaObj_New(&ga_cfunc_type_inst, &builtin_obj_ops);
    struct ga_builtin_state *statep = calloc(sizeof(struct ga_builtin_state), 1);

    if (self) {
        statep->self = GaObj_INC_REF(GaWeakRef_New(self));
    }

    statep->func = func;
    ret->un.statep = statep;

    return ret;
}

static struct ga_obj *
ga_builtin_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    struct ga_builtin_state *statep = self->un.statep;

    if (statep->self) {
        self = GaWeakRef_Val(statep->self);
        
        if (self == &ga_null_inst) {
            printf("BUG: Referenced 'self' object no longer exists!\n");
            return self;
        }
    }

    return statep->func(self, vm, argc, args);
}