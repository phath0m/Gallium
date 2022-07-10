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

static struct ga_obj *  ga_dict_type_invoke(struct ga_obj *, struct vm *, int, struct ga_obj **);

GA_BUILTIN_TYPE_DECL(ga_dict_type_inst, "Dict", ga_dict_type_invoke);

static void             ga_dict_destroy(struct ga_obj *);
static struct ga_obj *  ga_dict_getindex(struct ga_obj *, struct vm *, struct ga_obj *);
static void             ga_dict_setindex(struct ga_obj *, struct vm *, struct ga_obj *, struct ga_obj *);

static struct ga_obj_ops ga_dict_ops = {
    .destroy    =   ga_dict_destroy,
    .getindex   =   ga_dict_getindex,
    .setindex   =   ga_dict_setindex
};

struct ga_dict_state {
    struct list *   hash_map[GA_DICT_HASH_SIZE];
    struct list *   values;
};

static struct ga_obj * 
ga_dict_type_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    return ga_dict_new();
}

static void
ga_dict_list_destroy_cb(void *v, void *s)
{
    struct ga_dict_kvp *kvp = v;

    GAOBJ_DEC_REF(kvp->key);
    GAOBJ_DEC_REF(kvp->val);

    free(kvp);
}

static void
ga_dict_destroy(struct ga_obj *self)
{
    struct ga_dict_state *statep = self->un.statep;

    for (int i = 0; i < GA_DICT_HASH_SIZE; i++) {
        struct list *listp = statep->hash_map[i];

        if (listp) {
            list_destroy(listp, ga_dict_list_destroy_cb, NULL);
        }
    }
    
    free(statep);
}

static struct ga_obj *
ga_dict_getindex(struct ga_obj *self, struct vm *vm, struct ga_obj *key)
{
    struct ga_dict_state *statep = self->un.statep;
    int32_t hash = GAOBJ_HASH(self, vm) % GA_DICT_HASH_SIZE;
    struct list *listp = statep->hash_map[hash];
   

    if (!listp) {
        vm_raise_exception(vm, ga_key_error_new());
        return NULL;
    }

    struct ga_dict_kvp *kvp;
    list_iter_t iter;
    list_get_iter(listp, &iter);

    while (iter_next_elem(&iter, (void**)&kvp)) {
        if (GAOBJ_EQUALS(kvp->key, vm, key)) {
            return kvp->val;
        }
    }

    vm_raise_exception(vm, ga_key_error_new());
    return NULL;
}

static void
ga_dict_setindex(struct ga_obj *self, struct vm *vm, struct ga_obj *key, struct ga_obj *val)
{
    struct ga_dict_state *statep = self->un.statep;
    int32_t hash = GAOBJ_HASH(self, vm) % GA_DICT_HASH_SIZE;
    
    if (!statep->hash_map[hash]) {
        statep->hash_map[hash] = list_new();
    }

    struct list *listp = statep->hash_map[hash];

    struct ga_dict_kvp *kvp;
    list_iter_t iter;
    list_get_iter(listp, &iter);
    
    while (iter_next_elem(&iter, (void**)&kvp)) {
        if (GAOBJ_EQUALS(kvp->key, vm, key)) {
            GAOBJ_INC_REF(val);
            GAOBJ_DEC_REF(kvp->val);
            kvp->val = val;
            return;
        }
    }

    kvp = calloc(sizeof(struct ga_dict_kvp), 1);
    kvp->key = GAOBJ_INC_REF(key);
    kvp->val = GAOBJ_INC_REF(val);

    list_append(listp, kvp);
    list_append(statep->values, kvp);
}

struct ga_obj *
ga_dict_new()
{
    struct ga_obj *obj = ga_obj_new(&ga_dict_type_inst, &ga_dict_ops);
    struct ga_dict_state *statep = calloc(sizeof(struct ga_dict_state), 1);

    statep->values = list_new();
    obj->un.statep = statep;

    return obj;
}

void
ga_dict_get_iter(struct ga_obj *self, list_iter_t *iter)
{
    struct ga_dict_state *statep = self->un.statep;

    list_get_iter(statep->values, iter);
}
