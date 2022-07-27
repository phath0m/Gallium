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
        GaEval_RaiseException(vm, GaErr_NewArgumentError("filter() requires two arguments"));
        return NULL;
    }

    struct ga_obj *func = args[1];
    struct ga_obj *collection = args[0];
    struct ga_obj *iter_obj = GaObj_ITER(collection, vm);

    if (!iter_obj) return NULL;

    GaObj_INC_REF(iter_obj);

    struct ga_obj *ret = NULL;
    struct list *listp = GaLinkedList_New();
    struct ga_obj *in_obj = NULL;

    while (GaObj_ITER_NEXT(iter_obj, vm)) {
        in_obj = GaObj_INC_REF(GaObj_ITER_CUR(iter_obj, vm));

        if (!in_obj) goto cleanup;

        struct ga_obj *res = GaObj_INVOKE(func, vm, 1, &in_obj);

        if (!res) goto cleanup;

        GaObj_INC_REF(res);

        if (GaObj_IS_TRUE(res, vm)) {
            GaLinkedList_Push(listp, GaObj_INC_REF(in_obj));
        }

        GaObj_DEC_REF(in_obj);
        GaObj_DEC_REF(res);
    }

    ret = GaTuple_New(LIST_COUNT(listp));

    int i = 0;
    list_iter_t iter;
    GaLinkedList_GetIter(listp, &iter);

    while (GaIter_Next(&iter, (void**)&in_obj)) {
        GaTuple_InitElem(ret, i++, GaObj_MOVE_REF(in_obj));
    }

cleanup:
    GaObj_DEC_REF(iter_obj);
    GaLinkedList_Destroy(listp, NULL, NULL);
    return ret;
}

static struct ga_obj *
enumerable_map(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 2) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("map() requires two arguments"));
        return NULL;
    }

    struct ga_obj *func = args[1];
    struct ga_obj *iter_obj = GaObj_ITER(args[0], vm);

    if (!iter_obj) return NULL;

    GaObj_INC_REF(iter_obj);

    struct ga_obj *ret = NULL;
    struct list *listp = GaLinkedList_New();
    struct ga_obj *in_obj = NULL;
    struct ga_obj *out_obj = NULL;

    while (GaObj_ITER_NEXT(iter_obj, vm)) {
        in_obj = GaObj_INC_REF(GaObj_ITER_CUR(iter_obj, vm));

        if (!in_obj) goto cleanup;
        
        out_obj = GaObj_INVOKE(func, vm, 1, &in_obj);

        GaObj_DEC_REF(in_obj);

        if (!out_obj) goto cleanup;

        GaObj_INC_REF(out_obj);
        GaLinkedList_Push(listp, out_obj);
    }

    ret = GaTuple_New(LIST_COUNT(listp));

    int i = 0;
    list_iter_t iter;
    GaLinkedList_GetIter(listp, &iter);

    while (GaIter_Next(&iter, (void**)&out_obj)) {
        GaTuple_InitElem(ret, i++, GaObj_MOVE_REF(out_obj));
    }

cleanup:
    GaObj_DEC_REF(iter_obj);
    GaLinkedList_Destroy(listp, NULL, NULL);

    return ret;
}

static struct ga_obj *
enumerable_len(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("len() requires one argument"));
        return NULL;
    }

    int i = 0;
    struct ga_obj *cur = NULL;
    struct ga_obj *iter_obj = GaObj_ITER(args[0], vm);

    if (!iter_obj) {
        return NULL;
    }

    GaObj_INC_REF(iter_obj);

    while (GaObj_ITER_NEXT(iter_obj, vm)) {
        cur = GaObj_INC_REF(GaObj_ITER_CUR(iter_obj, vm));
        GaObj_DEC_REF(cur);
        i++;
    }

    GaObj_DEC_REF(iter_obj);

    return GaInt_FROM_I64(i);
}

struct ga_obj *
GaEnumerable_New()
{
    struct ga_obj *obj = GaObj_New(&ga_enumerable_type_inst, NULL);

    GaObj_SETATTR(obj, NULL, "filter", GaBuiltin_New(enumerable_filter, NULL));
    GaObj_SETATTR(obj, NULL, "map", GaBuiltin_New(enumerable_map, NULL));
    GaObj_SETATTR(obj, NULL, "count", GaBuiltin_New(enumerable_len, NULL));

    return obj;
}