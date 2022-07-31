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

static GaObject *   weakref_type_invoke(GaObject *, GaContext *, int, GaObject **);

GA_BUILTIN_TYPE_DECL(_GaWeakRef_Type, "WeakRef", weakref_type_invoke);

static void         weakref_destroy(GaObject *);
static GaObject *   weakref_getattr(GaObject *, GaContext *, const char *);

static struct ga_obj_ops weakref_ops = {
    .destroy    =   weakref_destroy,
    .getattr    =   weakref_getattr
};

static GaObject *
weakref_type_invoke(GaObject *self, GaContext *vm, int argc, GaObject **args)
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

    if (ref != &_GaNull) {
        _Ga_list_remove(ref->weak_refs, &self->un.statep, NULL, NULL);
    }
}

static GaObject *
weakref_getattr(GaObject *self, GaContext *vm, const char *name)
{
    if (strcmp(name, "value") == 0) {
        return (struct ga_obj*)self->un.statep;
    }

    return NULL;
}

GaObject *
GaWeakRef_New(GaObject *ref)
{
    GaObject *weak = GaObj_New(&_GaWeakRef_Type, &weakref_ops);

    if (!ref->weak_refs) {
        ref->weak_refs = _Ga_list_new();
    }

    weak->un.statep = ref;

    _Ga_list_push(ref->weak_refs, &weak->un.statep);
    
    return weak;
}

GaObject *
GaWeakRef_Val(GaObject *self)
{
    GaObject *ref_cell = self->un.statep;

    return ref_cell;
}