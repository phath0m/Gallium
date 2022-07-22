/*
 * enumerable.c - Gallium's builtin enumerable mixin.
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
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <gallium/builtins.h>
#include <gallium/object.h>
#include <gallium/vm.h>

GA_BUILTIN_TYPE_DECL(ga_enumerable_type_inst, "Enumerable", NULL);

static struct ga_obj *
enumerable_filter(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 2) {
        vm_raise_exception(vm, ga_argument_error_new("filter() requires two arguments"));
        return NULL;
    }

    struct ga_obj *func = args[1];
    struct ga_obj *collection = args[0];
    struct ga_obj *iter_obj = ga_obj_iter(collection, vm);

    if (!iter_obj) return NULL;

    GAOBJ_INC_REF(iter_obj);

    struct ga_obj *ret = NULL;
    struct list *listp = list_new();
    struct ga_obj *in_obj = NULL;

    while (GAOBJ_ITER_NEXT(iter_obj, vm)) {
        in_obj = GAOBJ_INC_REF(GAOBJ_ITER_CUR(iter_obj, vm));

        if (!in_obj) goto cleanup;

        struct ga_obj *res = GAOBJ_INVOKE(func, vm, 1, &in_obj);

        if (!res) goto cleanup;

        GAOBJ_INC_REF(res);

        if (GAOBJ_IS_TRUE(res, vm)) {
            list_append(listp, GAOBJ_INC_REF(in_obj));
        }

        GAOBJ_DEC_REF(in_obj);
        GAOBJ_DEC_REF(res);
    }

    ret = ga_tuple_new(LIST_COUNT(listp));

    int i = 0;
    list_iter_t iter;
    list_get_iter(listp, &iter);

    while (iter_next_elem(&iter, (void**)&in_obj)) {
        ga_tuple_init_elem(ret, i++, GAOBJ_MOVE_REF(in_obj));
    }

cleanup:
    GAOBJ_DEC_REF(iter_obj);
    list_destroy(listp, NULL, NULL);
    return ret;
}

static struct ga_obj *
enumerable_map(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 2) {
        vm_raise_exception(vm, ga_argument_error_new("map() requires two arguments"));
        return NULL;
    }

    struct ga_obj *func = args[1];
    struct ga_obj *iter_obj = ga_obj_iter(args[0], vm);

    if (!iter_obj) return NULL;

    GAOBJ_INC_REF(iter_obj);

    struct ga_obj *ret = NULL;
    struct list *listp = list_new();
    struct ga_obj *in_obj = NULL;
    struct ga_obj *out_obj = NULL;

    while (GAOBJ_ITER_NEXT(iter_obj, vm)) {
        in_obj = GAOBJ_INC_REF(GAOBJ_ITER_CUR(iter_obj, vm));

        if (!in_obj) goto cleanup;
        
        out_obj = GAOBJ_INVOKE(func, vm, 1, &in_obj);

        GAOBJ_DEC_REF(in_obj);

        if (!out_obj) goto cleanup;

        GAOBJ_INC_REF(out_obj);
        list_append(listp, out_obj);
    }

    ret = ga_tuple_new(LIST_COUNT(listp));

    int i = 0;
    list_iter_t iter;
    list_get_iter(listp, &iter);

    while (iter_next_elem(&iter, (void**)&out_obj)) {
        ga_tuple_init_elem(ret, i++, GAOBJ_MOVE_REF(out_obj));
    }

cleanup:
    GAOBJ_DEC_REF(iter_obj);
    list_destroy(listp, NULL, NULL);

    return ret;
}

static struct ga_obj *
enumerable_len(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        vm_raise_exception(vm, ga_argument_error_new("len() requires one argument"));
        return NULL;
    }

    int i = 0;
    struct ga_obj *cur = NULL;
    struct ga_obj *iter_obj = ga_obj_iter(args[0], vm);

    if (!iter_obj) {
        return NULL;
    }

    GAOBJ_INC_REF(iter_obj);

    while (GAOBJ_ITER_NEXT(iter_obj, vm)) {
        cur = GAOBJ_INC_REF(GAOBJ_ITER_CUR(iter_obj, vm));
        GAOBJ_DEC_REF(cur);
        i++;
    }

    GAOBJ_DEC_REF(iter_obj);

    return GA_INT_FROM_I64(i);
}

struct ga_obj *
ga_enumerable_new()
{
    struct ga_obj *obj = ga_obj_new(&ga_enumerable_type_inst, NULL);

    GAOBJ_SETATTR(obj, NULL, "filter", ga_builtin_new(enumerable_filter, NULL));
    GAOBJ_SETATTR(obj, NULL, "map", ga_builtin_new(enumerable_map, NULL));
    GAOBJ_SETATTR(obj, NULL, "count", ga_builtin_new(enumerable_len, NULL));

    return obj;
}