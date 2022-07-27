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

static void             ga_method_destroy(struct ga_obj *);
static struct ga_obj *  ga_method_invoke(struct ga_obj *, struct vm *, int, struct ga_obj **);

GA_BUILTIN_TYPE_DECL(ga_method_type_inst, "Method", NULL);

struct ga_obj_ops ga_method_ops = {
    .destroy        =   ga_method_destroy,
    .invoke         =   ga_method_invoke
};

struct ga_method_state {
    struct ga_obj   *   self;
    struct ga_obj   *   func;
};

static void
ga_method_destroy(struct ga_obj *self)
{
    struct ga_method_state *statep = self->un.statep;

    GaObj_DEC_REF(statep->func);
    GaObj_DEC_REF(statep->self);

    free(statep);
}

static struct ga_obj *
ga_method_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    struct ga_method_state *statep = self->un.statep;
    struct ga_obj *new_args[128];

    for (int i = 0; i < argc; i++) {
        new_args[i + 1] = args[i];
    }

    new_args[0] = ga_weakref_val(statep->self);

    return GaObj_INVOKE(statep->func, vm, argc + 1, new_args);
}

struct ga_obj *
ga_method_new(struct ga_obj *self, struct ga_obj *func)
{
    struct ga_method_state *statep = calloc(sizeof(struct ga_method_state), 1);
    struct ga_obj *obj = GaObj_New(&ga_method_type_inst, &ga_method_ops);

    /* weak reference to self, not optimal... need to figure out a better way to deal with this*/
    statep->self = GaObj_INC_REF(ga_weakref_new(self));
    statep->func = GaObj_INC_REF(func);
    obj->un.statep = statep;

    return obj;
}
