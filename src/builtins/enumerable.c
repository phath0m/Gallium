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

static GaObject *
enumerable_filter(GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARG_COUNT_EXACT(vm, 2, argc)) {
        return NULL;
    }

    GaObject *func = args[1];
    GaObject *collection = args[0];
    GaObject *iter_obj = GaObj_ITER(collection, vm);

    if (!iter_obj) return NULL;

    GaObj_INC_REF(iter_obj);

    GaObject *ret = NULL;
    _Ga_list_t *listp = _Ga_list_new();
    GaObject *in_obj = NULL;

    while (GaObj_ITER_NEXT(iter_obj, vm)) {
        in_obj = GaObj_INC_REF(GaObj_ITER_CUR(iter_obj, vm));

        if (!in_obj) goto cleanup;

        GaObject *res = GaObj_INVOKE(func, vm, 1, &in_obj);

        if (!res) goto cleanup;

        GaObj_INC_REF(res);

        if (GaObj_IS_TRUE(res, vm)) {
            _Ga_list_push(listp, GaObj_INC_REF(in_obj));
        }

        GaObj_DEC_REF(in_obj);
        GaObj_DEC_REF(res);
    }

    ret = GaTuple_New(_Ga_LIST_COUNT(listp));

    int i = 0;
    _Ga_iter_t iter;
    _Ga_list_get_iter(listp, &iter);

    while (_Ga_iter_next(&iter, (void**)&in_obj)) {
        GaTuple_InitElem(ret, i++, GaObj_MOVE_REF(in_obj));
    }

cleanup:
    GaObj_DEC_REF(iter_obj);
    _Ga_list_destroy(listp, NULL, NULL);
    return ret;
}

static GaObject *
enumerable_map(GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARG_COUNT_EXACT(vm, 2, argc)) {
        return NULL;
    }

    GaObject *func = args[1];
    GaObject *iter_obj = GaObj_ITER(args[0], vm);

    if (!iter_obj) return NULL;

    GaObj_INC_REF(iter_obj);

    _Ga_list_t *listp = _Ga_list_new();
    GaObject *ret = NULL;
    GaObject *in_obj = NULL;
    GaObject *out_obj = NULL;

    while (GaObj_ITER_NEXT(iter_obj, vm)) {
        in_obj = GaObj_INC_REF(GaObj_ITER_CUR(iter_obj, vm));

        if (!in_obj) goto cleanup;
        
        out_obj = GaObj_INVOKE(func, vm, 1, &in_obj);

        GaObj_DEC_REF(in_obj);

        if (!out_obj) goto cleanup;

        GaObj_INC_REF(out_obj);
        _Ga_list_push(listp, out_obj);
    }

    ret = GaTuple_New(_Ga_LIST_COUNT(listp));

    int i = 0;
    _Ga_iter_t iter;
    _Ga_list_get_iter(listp, &iter);

    while (_Ga_iter_next(&iter, (void**)&out_obj)) {
        GaTuple_InitElem(ret, i++, GaObj_MOVE_REF(out_obj));
    }

cleanup:
    GaObj_DEC_REF(iter_obj);
    _Ga_list_destroy(listp, NULL, NULL);

    return ret;
}

static GaObject *
enumerable_len(GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARG_COUNT_EXACT(vm, 1, argc)) {
        return NULL;
    }

    int i = 0;
    GaObject *cur = NULL;
    GaObject *iter_obj = GaObj_ITER(args[0], vm);

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

GaObject *
GaEnumerable_New()
{
    GaObject *obj = GaObj_New(&ga_enumerable_type_inst, NULL);

    GaObj_SETATTR(obj, NULL, "filter", GaBuiltin_New(enumerable_filter, NULL));
    GaObj_SETATTR(obj, NULL, "map", GaBuiltin_New(enumerable_map, NULL));
    GaObj_SETATTR(obj, NULL, "count", GaBuiltin_New(enumerable_len, NULL));

    return obj;
}