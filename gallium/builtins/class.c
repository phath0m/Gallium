/*
 * class.c - Class type
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
#include <gallium/dict.h>
#include <gallium/object.h>
#include <gallium/vm.h>

GA_BUILTIN_TYPE_DECL(ga_class_type_inst, "Class", NULL);

static void         class_destroy(GaObject *);
static GaObject *   class_invoke(GaObject *, struct vm *, int, GaObject **);

static struct ga_obj_ops class_ops = {
    .destroy    =   class_destroy,
    .invoke     =   class_invoke
};

struct class_state {
    GaObject   *   ctr;
    GaObject   *   base;
};

static void
class_destroy(GaObject *self)
{
    struct class_state *statep = self->un.statep;

    if (statep->base) {
        GaObj_DEC_REF(statep->base);
    }

    if (statep->ctr) {
        GaObj_DEC_REF(statep->ctr);
    }
}

static GaObject *
class_invoke(GaObject *self, struct vm *vm, int argc, GaObject **args)
{
    GaObject *obj_inst = GaObj_INC_REF(GaObj_New(self, NULL));
    struct dict *dictp = &self->dict;
    struct dict_kvp *kvp;

    list_iter_t iter;
    GaHashMap_GetIter(dictp, &iter);

    while (GaIter_Next(&iter, (void**)&kvp)) {
        GaObject *method = kvp->val;

        if (method->type == &ga_func_type_inst || method->type == &ga_cfunc_type_inst) {
            GaObj_SETATTR(obj_inst, vm, kvp->key, GaMethod_New(obj_inst, kvp->val));
        } else {
            GaObj_SETATTR(obj_inst, vm, kvp->key, kvp->val);
        }
    }

    struct class_state *statep = self->un.statep;

    if (statep->ctr) {
        GaObject *new_args[128];

        for (int i = 0; i < argc; i++) {
            new_args[i + 1] = args[i];
        }

        new_args[0] = obj_inst;
        
        GaObject *res = GaObj_INVOKE(statep->ctr, vm, argc + 1, new_args);
        
        if (!res) goto error;

        GaObj_INC_REF(res);

        /*
            Commenting this out. I want expressions to be implicitly returned so this logic breaks things.
            Unsure if I'll re-add this check or simply prohibit the use of the return keyword in constructors... 
        if (res != &ga_null_inst) {
            GAOBJ_DEC_REF(res);
            vm_raise_exception(vm, ga_type_error_new("Null"));
            goto error;
        }
        */
        GaObj_DEC_REF(res);
        
        if (vm->unhandled_exception) goto error;
    }

    if (!statep->ctr || !obj_inst->super) {
        GaObject *super_inst = GaObj_INVOKE(statep->base, vm, 0, NULL);

        if (!super_inst) goto error;

        obj_inst->super = GaObj_INC_REF(super_inst);
    }

    return GaObj_MOVE_REF(obj_inst);
    
error:
    GaObj_DEC_REF(obj_inst);

    return NULL;
}

GaObject *
GaClass_Base(GaObject *self)
{
    struct class_state *statep = self->un.statep;

    return statep->base;
}

static void
apply_mixin(GaObject *obj, GaObject *mixin)
{
    struct dict_kvp *kvp;
    list_iter_t iter;

    GaHashMap_GetIter(&mixin->dict, &iter);

    while (GaIter_Next(&iter, (void**)&kvp)) {
        GaObj_SETATTR(obj, NULL, kvp->key, kvp->val);
    }
}

static void
apply_methods(const char *name, GaObject *obj,
            GaObject *mixin)
{
    struct class_state *statep = obj->un.statep;
    struct ga_dict_kvp *kvp;
    list_iter_t iter;
    GaDict_GetITer(mixin, &iter);

    while (GaIter_Next(&iter, (void**)&kvp)) {
        if (kvp->key->type != &ga_str_type_inst) {
            /* this shouldn't happen */
            return;
        }

        const char *str = GaStr_ToCString(kvp->key);

        if (name && strcmp(str, name) == 0) {
            statep->ctr = GaObj_INC_REF(kvp->val);
        } else {
            GaObj_SETATTR(obj, NULL, str, kvp->val);
        }
    }
}

GaObject *
GaClass_New(const char *name, GaObject *base,
             GaObject *mixin_list, GaObject *dict)
{
    struct class_state *statep = calloc(sizeof(struct class_state), 1);
    GaObject *clazz = GaObj_New(&ga_class_type_inst, &class_ops);
 
    clazz->super = GaObj_INC_REF(GaObj_NewType(name));
    statep->base = GaObj_INC_REF(base);
    clazz->un.statep = statep;

    /* this is sort of a dangerous function if I ever have any sort of mult-
     * threading so I'm not going to publically export this. Don't use this
     * function please.
     */
    extern int _GaList_GetCells(GaObject *, GaObject ***);

    GaObject **mixins;

    int num_mixins = _GaList_GetCells(mixin_list, &mixins);

    for (int i = 0; i < num_mixins; i++) {
        apply_mixin(clazz, mixins[i]);
    }

    apply_methods(name, clazz, dict);

    return clazz;
}