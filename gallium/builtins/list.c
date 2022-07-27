/*
 * list.c - Builtin list type
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
#include <gallium/stringbuf.h>
#include <gallium/builtins.h>
#include <gallium/object.h>
#include <gallium/vm.h>

#define GA_LIST_INITIAL_SIZE    128

static struct ga_obj    *   ga_list_type_invoke(struct ga_obj *, struct vm *, int, struct ga_obj**);

GA_BUILTIN_TYPE_DECL(ga_list_type_inst, "List", ga_list_type_invoke);
GA_BUILTIN_TYPE_DECL(ga_list_iter_type_inst, "ListIter", NULL);

static void                 ga_list_destroy(struct ga_obj *);
static struct ga_obj    *   ga_list_iter(struct ga_obj *, struct vm *);
static struct ga_obj    *   ga_list_getindex(struct ga_obj *, struct vm *, struct ga_obj *);
static void                 ga_list_setindex(struct ga_obj *, struct vm *, struct ga_obj *, struct ga_obj *);
static struct ga_obj    *   ga_list_len(struct ga_obj *, struct vm *);
static struct ga_obj    *   ga_list_str(struct ga_obj *, struct vm *);

struct ga_obj_ops   list_obj_ops = {
    .destroy    =   ga_list_destroy,
    .iter       =   ga_list_iter,
    .getindex   =   ga_list_getindex,
    .setindex   =   ga_list_setindex,
    .len        =   ga_list_len,
    .str        =   ga_list_str
};

struct list_state {
    int                 used_cells;
    int                 avail_cells;
    struct ga_obj   **  cells;
};

static void                 ga_list_iter_destroy(struct ga_obj *);
static bool                 ga_list_iter_next(struct ga_obj *, struct vm *);
static struct ga_obj    *   ga_list_iter_cur(struct ga_obj *, struct vm *);

struct ga_obj_ops   list_iter_obj_ops = {
    .destroy    =   ga_list_iter_destroy,
    .iter_next  =   ga_list_iter_next,
    .iter_cur   =   ga_list_iter_cur,
};

struct list_iter_state {
    struct ga_obj   *   listp;
    int                 index; 
};

static struct ga_obj *
ga_list_type_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    return GaList_New();
}

static struct ga_obj *
ga_list_append_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    for (int i = 0; i < argc; i++) {
        GaList_Append(self, args[i]);
    }

    return &ga_null_inst;
}

static struct ga_obj *
ga_list_remove_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    for (int i = 0; i < argc; i++) {
        GaList_Remove(self, vm, args[i]);
    }

    return &ga_null_inst;
}

static void
ga_list_destroy(struct ga_obj *self)
{
    struct list_state *statep = self->un.statep;

    for (int i = 0; i < statep->used_cells; i++) {
        GaObj_DEC_REF(statep->cells[i]);
    }

    free(statep->cells);
    free(statep);
}

static struct ga_obj *
ga_list_getindex(struct ga_obj *self, struct vm *vm, struct ga_obj *key)
{
    struct list_state *statep = self->un.statep;
    struct ga_obj *key_int = GaObj_Super(key, &ga_int_type_inst);

    if (!key_int) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Int"));
        return NULL;
    }

    uint32_t index = (uint32_t)GaInt_TO_I64(key_int);

    if (index < statep->used_cells) {
        return statep->cells[index];
    }

    GaEval_RaiseException(vm, GaErr_NewIndexError("Index out of range"));
    return NULL;
}

static void
ga_list_setindex(struct ga_obj *self, struct vm *vm, struct ga_obj *key, struct ga_obj *val)
{
    struct list_state *statep = self->un.statep;
    struct ga_obj *key_int = GaObj_Super(key, &ga_int_type_inst);

    if (!key_int) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Int"));
        return;
    }

    uint32_t index = (uint32_t)GaInt_TO_I64(key_int);

    if (index < statep->used_cells) {
        GaObj_INC_REF(val);
        GaObj_DEC_REF(statep->cells[index]);
        statep->cells[index] = val;
        
        return;
    }

    GaEval_RaiseException(vm, GaErr_NewIndexError("Index out of range"));
}

static struct ga_obj *
ga_list_len(struct ga_obj *self, struct vm *vm)
{
    return GaInt_FROM_I64(GaList_Size(self));
}

static struct ga_obj *
ga_list_iter_new(struct ga_obj *listp)
{
    struct ga_obj *obj = GaObj_New(&ga_list_iter_type_inst, &list_iter_obj_ops);
    struct list_iter_state *statep = calloc(sizeof(struct list_iter_state), 1);

    statep->listp = GaObj_INC_REF(listp);
    statep->index = -1;
    obj->un.statep = statep;

    return obj;
}

static void
ga_list_iter_destroy(struct ga_obj *self)
{
    struct list_iter_state *statep = self->un.statep;
    GaObj_DEC_REF(statep->listp);
    free(statep);
}

static struct ga_obj *
ga_list_iter(struct ga_obj *self, struct vm *vm)
{
    return ga_list_iter_new(self);
}

static bool
ga_list_iter_next(struct ga_obj *self, struct vm *vm)
{
    struct list_iter_state *statep = self->un.statep;

    statep->index++;

    return statep->index < GaList_Size(statep->listp);
}

static struct ga_obj *
ga_list_iter_cur(struct ga_obj *self, struct vm *vm)
{
    struct list_iter_state *statep = self->un.statep;
    struct ga_obj *list_obj = GaObj_Super(statep->listp, &ga_list_type_inst);
    struct list_state *list_statep = list_obj->un.statep;

    return list_statep->cells[statep->index];
}

static struct ga_obj *
ga_list_str(struct ga_obj *self, struct vm *vm)
{
    struct ga_obj *iter_obj = ga_list_iter(self, vm);
    assert(iter_obj != NULL);
    GaObj_INC_REF(iter_obj);
    struct stringbuf *sb = GaStringBuilder_New();
    GaStringBuilder_Append(sb, "[");
    for (int i = 0; ga_list_iter_next(iter_obj, vm); i++) {
        if (i != 0) GaStringBuilder_Append(sb, ", ");
        struct ga_obj *in_obj = GaObj_INC_REF(ga_list_iter_cur(iter_obj, vm));
        assert(in_obj != NULL);
        struct ga_obj *elem_str = GaObj_Super(GaObj_STR(in_obj, vm), GA_STR_TYPE);
        assert(elem_str != NULL);
        GaStringBuilder_Append(sb, GaStr_ToCString(elem_str));
        GaObj_DEC_REF(in_obj);
    }
    GaStringBuilder_Append(sb, "]");
    GaObj_DEC_REF(iter_obj);
    return GaStr_FromStringBuilder(sb);
}

struct ga_obj *
GaList_New()
{
    struct ga_obj *obj = GaObj_New(&ga_list_type_inst, &list_obj_ops);
    struct list_state *statep = calloc(sizeof(struct list_state), 1);

    statep->cells = calloc(sizeof(struct ga_obj*)*GA_LIST_INITIAL_SIZE, 1);
    statep->avail_cells = GA_LIST_INITIAL_SIZE;

    obj->un.statep = statep;

    GaObj_SETATTR(obj, NULL, "append", GaBuiltin_New(ga_list_append_method, obj));
    GaObj_SETATTR(obj, NULL, "remove", GaBuiltin_New(ga_list_remove_method, obj));

    return obj;
}

void
GaList_Append(struct ga_obj *self, struct ga_obj *val)
{
    struct list_state *statep = self->un.statep;
    int index = statep->used_cells;

    if (index >= statep->avail_cells) {
        statep->avail_cells *= 2;
        statep->cells = realloc(statep->cells, sizeof(struct ga_obj *) * statep->avail_cells);
    }

    statep->cells[index] = GaObj_INC_REF(val);
    statep->used_cells++;
}

void
GaList_Remove(struct ga_obj *self, struct vm *vm, struct ga_obj *val)
{
    struct list_state *statep = self->un.statep;
    int needle_index = -1;

    for (int i = 0; i < statep->used_cells; i++) {
        if (GaObj_EQUALS(statep->cells[i], vm, val)) {
            needle_index = i;
            break;
        }
    }

    if (needle_index == -1) {
        return;
    }

    GaObj_DEC_REF(statep->cells[needle_index]);
    
    statep->used_cells--;

    for (int i = needle_index; i < statep->used_cells; i++) {
        statep->cells[i] = statep->cells[i + 1];
    }
}

int
GaList_Size(struct ga_obj *self)
{
    struct list_state *statep = self->un.statep;
    return statep->used_cells;
}

int
ga_list_get_cells(struct ga_obj *self, struct ga_obj ***cells)
{
    struct list_state *statep = self->un.statep;

    *cells = statep->cells;

    return statep->used_cells;
}