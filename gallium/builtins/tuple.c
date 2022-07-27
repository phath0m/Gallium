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
#include <gallium/vm.h>

GA_BUILTIN_TYPE_DECL(tuple_typedef_inst, "Tuple", NULL);
GA_BUILTIN_TYPE_DECL(ga_tuple_iter_type_inst, "TupleIter", NULL);

static void                 ga_tuple_destroy(struct ga_obj *);
static struct ga_obj    *   ga_tuple_getindex(struct ga_obj *, struct vm *, struct ga_obj *);
static struct ga_obj    *   ga_tuple_iter(struct ga_obj *, struct vm *);
static struct ga_obj    *   ga_tuple_str(struct ga_obj *, struct vm *);

struct ga_obj_ops   tuple_obj_ops = {
    .destroy    =   ga_tuple_destroy,
    .getindex   =   ga_tuple_getindex,
    .iter       =   ga_tuple_iter,
    .str        =   ga_tuple_str
};

struct tuple_state {
    int                 size;
    struct ga_obj   *   elems[];
};

static void                 ga_tuple_iter_destroy(struct ga_obj *);
static bool                 ga_tuple_iter_next(struct ga_obj *, struct vm *);
static struct ga_obj    *   ga_tuple_iter_cur(struct ga_obj *, struct vm *);

struct ga_obj_ops   tuple_iter_obj_ops = {
    .destroy    =   ga_tuple_iter_destroy,
    .iter_next  =   ga_tuple_iter_next,
    .iter_cur   =   ga_tuple_iter_cur
};

struct tuple_iter_state {
    struct ga_obj       *   tuple;
    struct tuple_state  *   tuple_state;
    int                     index;
};

static void
ga_tuple_destroy(struct ga_obj *self)
{
    struct tuple_state *statep = self->un.statep;

    for (int i = 0; i < statep->size; i++) {
        GaObj_DEC_REF(statep->elems[i]);
    }

    free(statep);
}

static struct ga_obj *
ga_tuple_getindex(struct ga_obj *self, struct vm *vm, struct ga_obj *key)
{
    struct tuple_state *statep = self->un.statep;

    if (key->type != &ga_int_type_inst) {
        GaEval_RaiseException(vm, ga_type_error_new("Int"));
        return NULL;
    }

    if (key->un.state_u32 < statep->size) {
        return statep->elems[key->un.state_u32];
    }

    GaEval_RaiseException(vm, ga_index_error_new("Index out of range"));

    return NULL;
}

static void
ga_tuple_iter_destroy(struct ga_obj *self)
{
    struct tuple_iter_state *statep = self->un.statep;
    GaObj_DEC_REF(statep->tuple);
    free(statep);
}

static bool
ga_tuple_iter_next(struct ga_obj *self, struct vm *vm)
{
    struct tuple_iter_state *statep = self->un.statep;

    statep->index++;
    
    return statep->index < statep->tuple_state->size;
}

static struct ga_obj *
ga_tuple_iter_cur(struct ga_obj *self, struct vm *vm)
{
    struct tuple_iter_state *statep = self->un.statep;

    return statep->tuple_state->elems[statep->index];
}

static struct ga_obj *
ga_tuple_iter_new(struct ga_obj *tuple)
{
    struct ga_obj *obj = GaObj_New(&ga_tuple_iter_type_inst, &tuple_iter_obj_ops);
    struct tuple_iter_state *statep = calloc(sizeof(struct tuple_iter_state), 1);
    
    statep->index = -1;
    statep->tuple = GaObj_INC_REF(tuple);
    statep->tuple_state = tuple->un.statep;
    obj->un.statep = statep;

    return obj;
}

static struct ga_obj *
ga_tuple_iter(struct ga_obj *self, struct vm *vm)
{
    return ga_tuple_iter_new(self);
}

static struct ga_obj *
ga_tuple_str(struct ga_obj *self, struct vm *vm)
{
    struct ga_obj *iter_obj = ga_tuple_iter(self, vm);
    assert(iter_obj != NULL);

    GaObj_INC_REF(iter_obj);

    struct stringbuf *sb = GaStringBuilder_New();

    GaStringBuilder_Append(sb, "(");

    for (int i = 0; ga_tuple_iter_next(iter_obj, vm); i++) {
        if (i != 0) GaStringBuilder_Append(sb, ", ");
        struct ga_obj *in_obj = GaObj_INC_REF(ga_tuple_iter_cur(iter_obj, vm));

        assert(in_obj != NULL);

        struct ga_obj *elem_str = GaObj_INC_REF(GaObj_Super(GaObj_STR(in_obj, vm), GA_STR_TYPE));

        assert(elem_str != NULL);

        GaStringBuilder_Append(sb, ga_str_to_cstring(elem_str));

        GaObj_DEC_REF(in_obj);
        GaObj_DEC_REF(elem_str);
    }

    GaStringBuilder_Append(sb, ")");

    GaObj_DEC_REF(iter_obj);

    return ga_str_from_stringbuf(sb);
}

struct ga_obj *
ga_tuple_new(int nelems)
{
    struct ga_obj *obj = GaObj_New(&tuple_typedef_inst, &tuple_obj_ops);
    struct tuple_state *statep = calloc(sizeof(struct tuple_state) + nelems*sizeof(struct ga_obj*), 1);

    statep->size = nelems;

    obj->un.statep = statep;

    return obj;
}

struct ga_obj *
ga_tuple_get_elem(struct ga_obj *self, int elem)
{
    struct tuple_state *statep = self->un.statep;

    if (elem < statep->size) {
        return statep->elems[elem];
    }

    return NULL;
}

int
ga_tuple_get_size(struct ga_obj *self)
{
    struct tuple_state *statep = self->un.statep;

    return statep->size;
}

void
ga_tuple_init_elem(struct ga_obj *self, int elem, struct ga_obj *obj)
{
    struct tuple_state *statep = self->un.statep;

    statep->elems[elem] = GaObj_INC_REF(obj);
}
