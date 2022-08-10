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
_Ga_list_t *ga_obj_all = NULL;
#endif

struct pool          ga_obj_pool = {
    .size   =   sizeof(struct ga_obj),
};

static void             ga_type_destroy(GaObject *);
static GaObject    *    type_invoke(GaObject *, GaContext *, int, GaObject **);
static GaObject    *    obj_invoke(GaObject *, GaContext *, int, GaObject **);

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
type_invoke(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_EXACT(vm, 1, (GaObject *[]){ GA_STR_TYPE }, argc, args)) {
        return NULL;
    }
    const char *name = GaStr_ToCString(args[0]);
    return GaObj_NewType(name, NULL);
}

bool
_GaType_Match(GaObject *self, GaContext *vm, GaObject *obj)
{
    return GaObj_IsInstanceOf(obj, self);
}

GaObject *
GaObj_NewType(const char *name, struct ga_obj_ops *ops)
{
    static struct ga_obj_ops default_type_ops = {
        .match = _GaType_Match
    };

    GaObject *type = GaObj_New(&ga_type_type_inst, ops);
    size_t name_len = strlen(name);
    char *name_buf = calloc(name_len+1, 1);
    memcpy(name_buf, name, name_len);

    type->un.statep = name_buf;

    if (!ops) {
        type->obj_ops = &default_type_ops;
    }
    else if (!ops->match) {
        ops->match = _GaType_Match;
    }

    return type;
}

const char *
GaObj_TypeName(GaObject *type)
{
    GaObject *type_inst = GaObj_Super(type, &ga_type_type_inst);

    if (type_inst && type_inst->un.statep) {
        return (const char*)type_inst->un.statep;
    }

    return "Object";
}

static GaObject *
obj_invoke(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    GaObject *type = &_GaObj_Type;

    if (argc > 0) {
        type = args[0];
    }

    GaObject *obj = GaObj_New(type, NULL);

    if (argc == 2) {
        GaObj_Assign(obj, args[1]);
    }

    return obj;
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
    _Ga_list_remove(ga_obj_all, self, NULL, NULL);
#endif

    if (self->weak_refs) {
        GaObject **ref;
        _Ga_iter_t iter;
        _Ga_list_get_iter(self->weak_refs, &iter);

        while (_Ga_iter_next(&iter, (void**)&ref)) {
            *ref = &_GaNull;
        }
        
        _Ga_list_destroy(self->weak_refs, NULL, NULL);
    }

    if (self->obj_ops && self->obj_ops->destroy) {
        self->obj_ops->destroy(self);
    }

    _Ga_hashmap_fini(&self->dict, dict_destroy_cb, NULL);

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
    obj->gc_ref_count = 0;
    obj->weak_refs = NULL;
    obj->generation = 0;

    bzero(&obj->dict, sizeof(obj->dict));
    
    ga_obj_stat.obj_count++;
#ifdef DEBUG_OBJECT_HEAP
    if (!ga_obj_all) ga_obj_all = _Ga_list_new();
    _Ga_list_push(ga_obj_all, obj);
#endif
    return obj;
}

void
GaObj_Assign(GaObject *obj, GaObject *values)
{
    struct ga_dict_kvp *kvp;
    _Ga_iter_t iter;
    GaDict_GetITer(values, &iter);

    while (_Ga_iter_next(&iter, (void**)&kvp)) {
        if (kvp->key->type != GA_STR_TYPE) {
            /* this shouldn't happen */
            return;
        }

        const char *str = GaStr_ToCString(kvp->key);
        GaObj_SETATTR(obj, NULL, str, kvp->val);
    }
}

void
GaObj_Print(GaObject *self, GaContext *vm)
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

/* List of objects that may be removed by the garbage collector */
static _Ga_list_t gc_candidates;

static int gc_gen2_count = 0;
static int gc_gen3_count = 0;

/*
 * Callback for garbage collector. Increment gc_ref_count, walk through all
 * child objects, then decrement gc_ref_count.
 * 
 * Any circular references will be detected by gc_ref_count having a value
 * higher than 1.
 * 
 * If a circular reference is found, push it to the gc_canididates list.
 */
static void
gc_cb(GaContext *ctx, GaObject *obj)
{
    obj->gc_ref_count++;
    if (obj->gc_ref_count > 1) {
        if (obj->gc_ref_count >= obj->ref_count &&
            !_Ga_list_contains(&gc_candidates, obj))
        {
            _Ga_list_push(&gc_candidates, obj);
        }
    } else {
        GaObj_GC_TRANSVERSE(obj, ctx, gc_cb);
    }
    obj->gc_ref_count--;
}

void
GaObj_CollectGarbage(GaContext *ctx)
{   
     struct pool_ent *i = ga_obj_pool.allocated_items;
    /* Walk through all objects */
    while (i) {
        GaObject *o = (GaObject *)i->data;
        GaObj_GC_TRANSVERSE(o, ctx, gc_cb);
        i = i->next;
    }
    /* Decrement one ref-count from all objects which have a circular ref */
    _Ga_iter_t iter;
    _Ga_list_get_iter(&gc_candidates, &iter);
    GaObject *obj = NULL;
    while (_Ga_iter_next(&iter, (void**)(&obj))) {
        GaObj_DEC_REF(obj);
    }
    _Ga_list_fini(&gc_candidates, NULL, NULL);

    /* Assign all survivors to the next generation and increment the counters */
    gc_gen2_count = 0;
    gc_gen3_count = 0;
    i = ga_obj_pool.allocated_items;
    while (i) {
        GaObject *o = (GaObject *)i->data;
        if (o->obj_ops && o->obj_ops->gc_tranverse) {
            if (o->generation < 2) {
                o->generation++;
            }
            switch (o->generation) {
                case 1:
                    gc_gen2_count++;
                    break;
                case 2:
                    gc_gen3_count++;
                    break;
                default:
                    break;
            }
        }
        i = i->next;
    }
}

void
GaObj_TryCollectGarbage(GaContext *ctx)
{
    int gen1_count = ga_obj_pool.num_items - (gc_gen2_count + gc_gen3_count);
    if (gen1_count > 300) {
        GaObj_CollectGarbage(ctx);
    }
}