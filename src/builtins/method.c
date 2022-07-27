/*
 * method.c - Method type. The difference between functions and methods in
 * Gallium is that methods are bound to a self type, whilst functions are not.
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
#include <stdlib.h>
#include <string.h>
#include <gallium/builtins.h>
#include <gallium/object.h>
#include <gallium/vm.h>

static void         method_destroy(GaObject *);
static GaObject *   method_invoke(GaObject *, struct vm *, int, GaObject **);

GA_BUILTIN_TYPE_DECL(ga_method_type_inst, "Method", NULL);

static struct ga_obj_ops method_ops = {
    .destroy        =   method_destroy,
    .invoke         =   method_invoke
};

struct method_state {
    GaObject   *   self;
    GaObject   *   func;
};

static void
method_destroy(GaObject *self)
{
    struct method_state *statep = self->un.statep;

    GaObj_DEC_REF(statep->func);
    GaObj_DEC_REF(statep->self);

    free(statep);
}

static GaObject *
method_invoke(GaObject *self, struct vm *vm, int argc, GaObject **args)
{
    struct method_state *statep = self->un.statep;
    GaObject *new_args[128];

    for (int i = 0; i < argc; i++) {
        new_args[i + 1] = args[i];
    }

    new_args[0] = GaWeakRef_Val(statep->self);

    return GaObj_INVOKE(statep->func, vm, argc + 1, new_args);
}

GaObject *
GaMethod_New(GaObject *self, GaObject *func)
{
    struct method_state *statep = calloc(sizeof(struct method_state), 1);
    GaObject *obj = GaObj_New(&ga_method_type_inst, &method_ops);

    /* weak reference to self, not optimal... need to figure out a better way to deal with this*/
    statep->self = GaObj_INC_REF(GaWeakRef_New(self));
    statep->func = GaObj_INC_REF(func);
    obj->un.statep = statep;

    return obj;
}