/*
 * object.c - Gallium base object implementation
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
#include <string.h>
#include <gallium/builtins.h>
#include <gallium/dict.h>
#include <gallium/object.h>
#include <gallium/pool.h>
#include <gallium/vm.h>

#ifdef DEBUG_OBJECT_HEAP
struct list *ga_obj_all = NULL;
#endif

static struct pool          ga_obj_pool = {
    .size   =   sizeof(struct ga_obj),
};

static void             ga_type_destroy(GaObject *);
static GaObject    *    type_invoke(GaObject *, struct vm *, int, GaObject **);
static GaObject    *    obj_invoke(GaObject *, struct vm *, int, GaObject **);

struct ga_obj_ops ga_typedef_ops = {
    .destroy    =   ga_type_destroy,
    .invoke     =   type_invoke,
    .match      =   _GaType_Match
};

struct ga_obj_ops ga_obj_type_ops = {
    .invoke     =   obj_invoke
};

GaObject   ga_type_type_inst = {
    .ref_count  =   1,
    .type       =   NULL,
    .obj_ops    =   &ga_typedef_ops,
};

GaObject   _GaObj_Type = {
    .ref_count  =   1,
    .type       =   &ga_type_type_inst,
    .obj_ops    =   &ga_obj_type_ops,
};

struct ga_obj_statistics ga_obj_stat;

static void
ga_type_destroy(GaObject *self)
{
    free(self->un.statep);
}

static GaObject *
type_invoke(GaObject *self, struct vm *vm, int argc, GaObject **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("Type() requires at least one argument"));
        return NULL;
    }

    if (args[0]->type != &_GaStr_Type) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Str"));
        return NULL;
    }

    const char *name = GaStr_ToCString(args[0]);

    return GaObj_NewType(name);
}

bool
_GaType_Match(GaObject *self, struct vm *vm, GaObject *obj)
{
    return GaObj_IsInstanceOf(obj, self);
}

GaObject *
GaObj_NewType(const char *name)
{
    GaObject *type = GaObj_New(&ga_type_type_inst, NULL);
    size_t name_len = strlen(name);
    char *name_buf = calloc(name_len, 1);

    strcpy(name_buf, name);
    type->un.statep = name_buf;

    return type;
}

const char *
GaObj_TypeName(GaObject *type)
{
    GaObject *type_inst = GaObj_Super(type, &ga_type_type_inst);
    return (const char*)type_inst->un.statep;
}

static GaObject *
obj_invoke(GaObject *self, struct vm *vm, int argc, GaObject **args)
{
    GaObject *type = &_GaObj_Type;

    if (argc == 1) {
        type = args[0];
    }

    return GaObj_New(type, NULL);
}

static void
dict_destroy_cb(void *p, void *s)
{
    GaObject *obj = p;
    GaObj_DEC_REF(obj);
}

void
GaObj_Destroy(GaObject *self)
{
    GaObject *type = self->type;
    GaObject *super = self->super;

#ifdef DEBUG_OBJECT_HEAP
    list_remove(ga_obj_all, self, NULL, NULL);
#endif

    if (self->weak_refs) {
        GaObject **ref;
        list_iter_t iter;
        GaLinkedList_GetIter(self->weak_refs, &iter);

        while (GaIter_Next(&iter, (void**)&ref)) {
            *ref = &_GaNull;
        }
        
        GaLinkedList_Destroy(self->weak_refs, NULL, NULL);
    }

    if (self->obj_ops && self->obj_ops->destroy) {
        self->obj_ops->destroy(self);
    }

    GaHashMap_Fini(&self->dict, dict_destroy_cb, NULL);

    GaPool_PUT(&ga_obj_pool, self);

    if (super) {
        GaObj_DEC_REF(super);
    }
    GaObj_DEC_REF(type);
    ga_obj_stat.obj_count--;
}

GaObject *
GaObj_New(GaObject *type, struct ga_obj_ops *ops)
{
    if (!type) {
        /* generic object... default to generic object type */
        type = &_GaObj_Type;
    }

    GaObject *obj = GaPool_GET(&ga_obj_pool);

    obj->type = GaObj_INC_REF(type);
    obj->obj_ops = ops;
    
    obj->super = NULL;
    obj->ref_count = 0;
    obj->weak_refs = NULL;
    
    bzero(&obj->dict, sizeof(obj->dict));
    
    ga_obj_stat.obj_count++;
#ifdef DEBUG_OBJECT_HEAP
    if (!ga_obj_all) ga_obj_all = list_new();
    list_append(ga_obj_all, obj);
#endif
    return obj;
}

void
GaObj_Print(GaObject *self, struct vm *vm)
{
    GaObject *str_val = GaObj_STR(self, vm);
    if (str_val) {
        GaObj_INC_REF(str_val);
        printf("%s\n", GaStr_ToCString(str_val));
        GaObj_DEC_REF(str_val);
    }
}

GaObject *
GaObj_Super(GaObject *self, GaObject *type)
{
    GaObject *super = self;
    while (super) {
        if (super->type == type) {
            return super;
        }

        super = super->super;
    }
    return NULL;
}

bool
GaObj_IsInstanceOf(GaObject *self, GaObject *type)
{
    GaObject *super = self;
    while (super) {
        if (super->type == type) {
            return true;
        }
        
        super = super->super;
    }
    return NULL;
}