/*
 * range.c - Gallium's builtin range iterator.
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

static GaObject *   range_type_invoke(GaObject *, struct vm *, int, GaObject **);

GA_BUILTIN_TYPE_DECL(_GaRange_Type, "Range", range_type_invoke);

static void         range_destroy(GaObject *);
static GaObject *   range_iter(GaObject *, struct vm *);

static struct ga_obj_ops range_ops = {
    .destroy    =   range_destroy,
    .iter       =   range_iter
};

struct range_state {
    int64_t start;
    int64_t end;
    int64_t stride;
};

struct range_iter_state {
    struct range_state  *   range_state;
    GaObject            *   range;
    int64_t                 pos;
};

GA_BUILTIN_TYPE_DECL(ga_range_iter_type_inst, "RangeIter", NULL);

static void         range_iter_destroy(GaObject *);
static bool         range_iter_next(GaObject *, struct vm *);
static GaObject *   range_iter_cur(GaObject *, struct vm *);

static struct ga_obj_ops range_iter_ops = {
    .destroy    =   range_iter_destroy,
    .iter_next  =   range_iter_next,
    .iter_cur   =   range_iter_cur
};

static GaObject *
range_type_invoke(GaObject *self, struct vm *vm, int argc, GaObject **args)
{
    if (argc != 2 && argc != 3) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("Range() requires 2-3 arguments"));
        return NULL;
    }

    for (int i = 0; i < argc; i++) {
        if (!GaObj_IsInstanceOf(args[i], &_GaInt_Type)) {
            GaEval_RaiseException(vm, GaErr_NewTypeError("Int"));
        }
    }

    if (argc == 2) {
        return GaRange_New(GaInt_TO_I64(args[0]), GaInt_TO_I64(args[1]), 1);
    } else {
        return GaRange_New(GaInt_TO_I64(args[0]), GaInt_TO_I64(args[1]), GaInt_TO_I64(args[2]));
    }
}

static void
range_destroy(GaObject *self)
{
    free(self->un.statep);
}

static void
range_iter_destroy(GaObject *self)
{
    struct range_iter_state *statep = self->un.statep;
    GaObj_DEC_REF(statep->range);
    free(statep);
}

static bool
range_iter_next(GaObject *self, struct vm *vm)
{
    struct range_iter_state *statep = self->un.statep;

    if (statep->pos == -1) {
        statep->pos = statep->range_state->start;
    } else {
        statep->pos += statep->range_state->stride;
    }

    return statep->pos < statep->range_state->end;
}

static GaObject *
range_iter_cur(GaObject *self, struct vm *vm)
{
    struct range_iter_state *statep = self->un.statep;

    return GaInt_FROM_I64(statep->pos);
}

static GaObject *
range_iter_new(GaObject *range)
{
    GaObject *obj = GaObj_New(&ga_range_iter_type_inst, &range_iter_ops);
    struct range_iter_state *statep = calloc(sizeof(struct range_iter_state), 1);
    statep->range_state = range->un.statep;
    statep->range = GaObj_INC_REF(range);
    statep->pos = -1;
    obj->un.statep = statep;
    return obj;
}

static GaObject *
range_iter(GaObject *self, struct vm *vm)
{
    return range_iter_new(self);
}

GaObject *
GaRange_New(int64_t start, int64_t end, int64_t stride)
{
    GaObject *obj = GaObj_New(&_GaRange_Type, &range_ops);
    struct range_state *statep = calloc(sizeof(struct range_state), 1);

    statep->start = start;
    statep->end = end;
    statep->stride = stride;
    obj->un.statep = statep;

    return obj;
}
