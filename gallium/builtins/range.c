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

static struct ga_obj    *   ga_range_type_invoke(struct ga_obj *, struct vm *, int, struct ga_obj **);

GA_BUILTIN_TYPE_DECL(ga_range_type_inst, "Range", ga_range_type_invoke);

static void                 ga_range_destroy(struct ga_obj *);
static struct ga_obj    *   ga_range_iter(struct ga_obj *, struct vm *);

struct ga_obj_ops   ga_range_ops = {
    .destroy    =   ga_range_destroy,
    .iter       =   ga_range_iter
};

struct ga_range_state {
    int64_t start;
    int64_t end;
    int64_t stride;
};

struct ga_range_iter_state {
    struct ga_range_state   *   range_state;
    struct ga_obj           *   range;
    int64_t                     pos;
};

GA_BUILTIN_TYPE_DECL(ga_range_iter_type_inst, "RangeIter", NULL);

static void                 ga_range_iter_destroy(struct ga_obj *);
static bool                 ga_range_iter_next(struct ga_obj *, struct vm *);
static struct ga_obj    *   ga_range_iter_cur(struct ga_obj *, struct vm *);

struct ga_obj_ops   ga_range_iter_ops = {
    .destroy    =   ga_range_iter_destroy,
    .iter_next  =   ga_range_iter_next,
    .iter_cur   =   ga_range_iter_cur
};

static struct ga_obj *
ga_range_type_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 2 && argc != 3) {
        vm_raise_exception(vm, ga_argument_error_new("Range() requires 2-3 arguments"));
        return NULL;
    }

    for (int i = 0; i < argc; i++) {
        if (!ga_obj_instanceof(args[i], &ga_int_type_inst)) {
            vm_raise_exception(vm, ga_type_error_new("Int"));
        }
    }

    if (argc == 2) {
        return ga_range_new(ga_int_to_i64(args[0]), ga_int_to_i64(args[1]), 1);
    } else {
        return ga_range_new(ga_int_to_i64(args[0]), ga_int_to_i64(args[1]), ga_int_to_i64(args[2]));
    }
}

static void
ga_range_destroy(struct ga_obj *self)
{
    free(self->un.statep);
}

static void
ga_range_iter_destroy(struct ga_obj *self)
{
    struct ga_range_iter_state *statep = self->un.statep;
    GAOBJ_DEC_REF(statep->range);
    free(statep);
}

static bool
ga_range_iter_next(struct ga_obj *self, struct vm *vm)
{
    struct ga_range_iter_state *statep = self->un.statep;

    if (statep->pos == -1) {
        statep->pos = statep->range_state->start;
    } else {
        statep->pos += statep->range_state->stride;
    }

    return statep->pos < statep->range_state->end;
}

static struct ga_obj *
ga_range_iter_cur(struct ga_obj *self, struct vm *vm)
{
    struct ga_range_iter_state *statep = self->un.statep;

    return ga_int_from_i64(statep->pos);
}

static struct ga_obj *
ga_range_iter_new(struct ga_obj *range)
{
    struct ga_obj *obj = ga_obj_new(&ga_range_iter_type_inst, &ga_range_iter_ops);
    struct ga_range_iter_state *statep = calloc(sizeof(struct ga_range_iter_state), 1);
    statep->range_state = range->un.statep;
    statep->range = GAOBJ_INC_REF(range);
    statep->pos = -1;
    obj->un.statep = statep;
    return obj;
}

static struct ga_obj *
ga_range_iter(struct ga_obj *self, struct vm *vm)
{
    return ga_range_iter_new(self);
}

struct ga_obj *
ga_range_new(int64_t start, int64_t end, int64_t stride)
{
    struct ga_obj *obj = ga_obj_new(&ga_range_type_inst, &ga_range_ops);
    struct ga_range_state *statep = calloc(sizeof(struct ga_range_state), 1);

    statep->start = start;
    statep->end = end;
    statep->stride = stride;
    obj->un.statep = statep;

    return obj;
}
