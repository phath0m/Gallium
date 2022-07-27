/*
 * weakref.c - Gallium's weak reference type.
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
#include <gallium/list.h>

static GaObject *   weakref_type_invoke(GaObject *, struct vm *, int, GaObject **);

GA_BUILTIN_TYPE_DECL(ga_weakref_type_inst, "WeakRef", weakref_type_invoke);

static void         weakref_destroy(GaObject *);
static GaObject *   weakref_getattr(GaObject *, struct vm *, const char *);

static struct ga_obj_ops weakref_ops = {
    .destroy    =   weakref_destroy,
    .getattr    =   weakref_getattr
};

static GaObject *
weakref_type_invoke(GaObject *self, struct vm *vm, int argc, GaObject **args)
{
    if (argc != 1) {
        /* exception */
        return NULL;
    }
    
    return GaWeakRef_New(args[0]);
}

static void
weakref_destroy(GaObject *self)
{
    GaObject *ref = self->un.statep;

    if (ref != &ga_null_inst) {
        GaLinkedList_Remove(ref->weak_refs, &self->un.statep, NULL, NULL);
    }
}

static GaObject *
weakref_getattr(GaObject *self, struct vm *vm, const char *name)
{
    if (strcmp(name, "value") == 0) {
        return (struct ga_obj*)self->un.statep;
    }

    return NULL;
}

GaObject *
GaWeakRef_New(GaObject *ref)
{
    GaObject *weak = GaObj_New(&ga_weakref_type_inst, &weakref_ops);

    if (!ref->weak_refs) {
        ref->weak_refs = GaLinkedList_New();
    }

    weak->un.statep = ref;

    GaLinkedList_Push(ref->weak_refs, &weak->un.statep);
    
    return weak;
}

GaObject *
GaWeakRef_Val(GaObject *self)
{
    GaObject *ref_cell = self->un.statep;

    return ref_cell;
}