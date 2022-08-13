/*
 * tuple.c - Gallium's builtin tuple type.
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
#include <assert.h>
#include <stdlib.h>
#include <gallium/builtins.h>
#include <gallium/object.h>
#include <gallium/stringbuf.h>
#include <gallium/vec.h>
#include <gallium/vm.h>

GA_BUILTIN_TYPE_DECL(ga_tuple_typedef_inst, "Tuple", NULL);
GA_BUILTIN_TYPE_DECL(ga_tuple_iter_type_inst, "TupleIter", NULL);

static void         tuple_destroy(GaObject *);
static GaObject *   tuple_getindex(GaObject *, GaContext *, GaObject *);
static GaObject *   tuple_iter(GaObject *, GaContext *);
static GaObject *   tuple_str(GaObject *, GaContext *);

static struct Ga_Operators tuple_ops = {
    .destroy    =   tuple_destroy,
    .getindex   =   tuple_getindex,
    .iter       =   tuple_iter,
    .str        =   tuple_str
};

struct tuple_state {
    int             size;
    GaObject    *   elems[];
};

static void         tuple_iter_destroy(GaObject *);
static bool         tuple_iter_next(GaObject *, GaContext *);
static GaObject *   tuple_iter_cur(GaObject *, GaContext *);

static struct Ga_Operators tuple_iter_ops = {
    .destroy    =   tuple_iter_destroy,
    .iter_next  =   tuple_iter_next,
    .iter_cur   =   tuple_iter_cur
};

struct tuple_iter_state {
    GaObject            *   tuple;
    struct tuple_state  *   tuple_state;
    int                     index;
};

static void
tuple_destroy(GaObject *self)
{
    struct tuple_state *statep = self->un.statep;
    for (int i = 0; i < statep->size; i++) {
        GaObj_DEC_REF(statep->elems[i]);
    }
    free(statep);
}

static GaObject *
tuple_getindex(GaObject *self, GaContext *vm, GaObject *key)
{
    struct tuple_state *statep = self->un.statep;

    if (!Ga_ENSURE_TYPE(vm, key, GA_INT_TYPE)) {
        return NULL;
    }

    if (key->un.state_u32 < statep->size) {
        return statep->elems[key->un.state_u32];
    }

    GaEval_RaiseException(vm, GaErr_NewIndexError("Tuple index out of range"));

    return NULL;
}

static void
tuple_iter_destroy(GaObject *self)
{
    struct tuple_iter_state *statep = self->un.statep;
    GaObj_DEC_REF(statep->tuple);
    free(statep);
}

static bool
tuple_iter_next(GaObject *self, GaContext *vm)
{
    struct tuple_iter_state *statep = self->un.statep;

    statep->index++;
    
    return statep->index < statep->tuple_state->size;
}

static GaObject *
tuple_iter_cur(GaObject *self, GaContext *vm)
{
    struct tuple_iter_state *statep = self->un.statep;

    return statep->tuple_state->elems[statep->index];
}

static GaObject *
tuple_iter_new(GaObject *tuple)
{
    GaObject *obj = GaObj_New(&ga_tuple_iter_type_inst, &tuple_iter_ops);
    struct tuple_iter_state *statep = calloc(sizeof(struct tuple_iter_state), 1);
    
    statep->index = -1;
    statep->tuple = GaObj_INC_REF(tuple);
    statep->tuple_state = tuple->un.statep;
    obj->un.statep = statep;

    return obj;
}

static GaObject *
tuple_iter(GaObject *self, GaContext *vm)
{
    return tuple_iter_new(self);
}

static GaObject *
tuple_str(GaObject *self, GaContext *vm)
{
    GaObject *iter_obj = tuple_iter(self, vm);
    assert(iter_obj != NULL);

    GaObj_INC_REF(iter_obj);

    struct stringbuf *sb = GaStringBuilder_New();

    GaStringBuilder_Append(sb, "(");

    for (int i = 0; tuple_iter_next(iter_obj, vm); i++) {
        if (i != 0) GaStringBuilder_Append(sb, ", ");
        GaObject *in_obj = GaObj_INC_REF(tuple_iter_cur(iter_obj, vm));

        assert(in_obj != NULL);

        GaObject *elem_str = GaObj_INC_REF(GaObj_Super(GaObj_STR(in_obj, vm), GA_STR_TYPE));

        assert(elem_str != NULL);

        GaStringBuilder_Append(sb, GaStr_ToCString(elem_str));

        GaObj_DEC_REF(in_obj);
        GaObj_DEC_REF(elem_str);
    }

    GaStringBuilder_Append(sb, ")");

    GaObj_DEC_REF(iter_obj);

    return GaStr_FromStringBuilder(sb);
}

GaObject *
GaTuple_New(int nelems)
{
    GaObject *obj = GaObj_New(&ga_tuple_typedef_inst, &tuple_ops);
    struct tuple_state *statep = calloc(sizeof(struct tuple_state) + nelems*sizeof(struct Ga_Object*), 1);
    statep->size = nelems;
    obj->un.statep = statep;
    return obj;
}

GaObject *
GaTuple_FromVec(struct vec *vec)
{
    GaObject *ret = GaTuple_New(vec->used_cells);
    for (int i = 0; i < vec->used_cells; i++) {
        GaTuple_InitElem(ret, i, vec->cells[i]);
    }
    return ret;
}

GaObject *
GaTuple_GetElem(GaObject *self, int elem)
{
    struct tuple_state *statep = self->un.statep;
    if (elem < statep->size) {
        return statep->elems[elem];
    }
    return NULL;
}

int
GaTuple_GetSize(GaObject *self)
{
    struct tuple_state *statep = self->un.statep;
    return statep->size;
}

void
GaTuple_InitElem(GaObject *self, int elem, GaObject *obj)
{
    struct tuple_state *statep = self->un.statep;
    statep->elems[elem] = GaObj_INC_REF(obj);
}