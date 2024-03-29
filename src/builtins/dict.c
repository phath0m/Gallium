/*
 * dict.c - Gallium's builtin dictionary type
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
#include <gallium/builtins.h>
#include <gallium/dict.h>
#include <gallium/object.h>
#include <gallium/vm.h>

#define GA_DICT_HASH_SIZE   101

static GaObject *   dict_type_invoke(GaObject *, GaContext *, int, GaObject **);

GA_BUILTIN_TYPE_DECL(_GaDict_Type, "Dict", dict_type_invoke);

static void         dict_destroy(GaObject *);
static GaObject *   dict_getindex(GaObject *, GaContext *, GaObject *);
static void         dict_setindex(GaObject *, GaContext *, GaObject *,
                                  GaObject *);

static struct Ga_Operators dict_ops = {
    .destroy    =   dict_destroy,
    .getindex   =   dict_getindex,
    .setindex   =   dict_setindex
};

struct dict_state {
    _Ga_list_t *   hash_map[GA_DICT_HASH_SIZE];
    _Ga_list_t *   values;
};

static GaObject * 
dict_type_invoke(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARG_COUNT_EXACT(vm, 0, argc)) {
        return NULL;
    }
    return GaDict_New();
}

static void
ga_dict_list_destroy_cb(void *v, void *s)
{
    struct ga_dict_kvp *kvp = v;

    GaObj_DEC_REF(kvp->key);
    GaObj_DEC_REF(kvp->val);

    free(kvp);
}

static void
dict_destroy(GaObject *self)
{
    struct dict_state *statep = self->un.statep;

    for (int i = 0; i < GA_DICT_HASH_SIZE; i++) {
        _Ga_list_t *listp = statep->hash_map[i];

        if (listp) {
            _Ga_list_destroy(listp, ga_dict_list_destroy_cb, NULL);
        }
    }
    
    free(statep);
}

static GaObject *
dict_getindex(GaObject *self, GaContext *vm, GaObject *key)
{
    struct dict_state *statep = self->un.statep;
    int32_t hash = GaObj_HASH(self, vm) % GA_DICT_HASH_SIZE;
    _Ga_list_t *listp = statep->hash_map[hash];

    if (!listp) {
        GaEval_RaiseException(vm, GaErr_NewKeyError());
        return NULL;
    }

    struct ga_dict_kvp *kvp;
    _Ga_iter_t iter;
    _Ga_list_get_iter(listp, &iter);

    while (_Ga_iter_next(&iter, (void**)&kvp)) {
        if (GaObj_EQUALS(kvp->key, vm, key)) {
            return kvp->val;
        }
    }

    GaEval_RaiseException(vm, GaErr_NewKeyError());
    return NULL;
}

static void
dict_setindex(GaObject *self, GaContext *vm, GaObject *key, GaObject *val)
{
    struct dict_state *statep = self->un.statep;
    int32_t hash = GaObj_HASH(self, vm) % GA_DICT_HASH_SIZE;
    
    if (!statep->hash_map[hash]) {
        statep->hash_map[hash] = _Ga_list_new();
    }

    _Ga_list_t *listp = statep->hash_map[hash];

    struct ga_dict_kvp *kvp;
    _Ga_iter_t iter;
    _Ga_list_get_iter(listp, &iter);
    
    while (_Ga_iter_next(&iter, (void**)&kvp)) {
        if (GaObj_EQUALS(kvp->key, vm, key)) {
            GaObj_INC_REF(val);
            GaObj_DEC_REF(kvp->val);
            kvp->val = val;
            return;
        }
    }

    kvp = calloc(sizeof(struct ga_dict_kvp), 1);
    kvp->key = GaObj_INC_REF(key);
    kvp->val = GaObj_INC_REF(val);

    _Ga_list_push(listp, kvp);
    _Ga_list_push(statep->values, kvp);
}

GaObject *
GaDict_New()
{
    GaObject *obj = GaObj_New(&_GaDict_Type, &dict_ops);
    struct dict_state *statep = calloc(sizeof(struct dict_state), 1);

    statep->values = _Ga_list_new();
    obj->un.statep = statep;

    return obj;
}

void
GaDict_GetITer(GaObject *self, _Ga_iter_t *iter)
{
    struct dict_state *statep = self->un.statep;

    _Ga_list_get_iter(statep->values, iter);
}