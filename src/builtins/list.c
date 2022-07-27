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

static GaObject *   list_type_invoke(GaObject *, struct vm *, int, struct ga_obj**);

GA_BUILTIN_TYPE_DECL(_GaList_Type, "List", list_type_invoke);
GA_BUILTIN_TYPE_DECL(ga_list_iter_type_inst, "ListIter", NULL);

static void             list_destroy(GaObject *);
static GaObject    *    list_iter(GaObject *, struct vm *);
static GaObject    *    list_getindex(GaObject *, struct vm *, GaObject *);
static void             list_setindex(GaObject *, struct vm *, GaObject *, GaObject *);
static GaObject    *    list_len(GaObject *, struct vm *);
static GaObject    *    list_str(GaObject *, struct vm *);

static struct ga_obj_ops list_ops = {
    .destroy    =   list_destroy,
    .iter       =   list_iter,
    .getindex   =   list_getindex,
    .setindex   =   list_setindex,
    .len        =   list_len,
    .str        =   list_str
};

struct list_state {
    int            used_cells;
    int            avail_cells;
    GaObject   **  cells;
};

static void         list_iter_destroy(GaObject *);
static bool         list_iter_next(GaObject *, struct vm *);
static GaObject *   list_iter_cur(GaObject *, struct vm *);

static struct ga_obj_ops list_iter_ops = {
    .destroy    =   list_iter_destroy,
    .iter_next  =   list_iter_next,
    .iter_cur   =   list_iter_cur,
};

struct list_iter_state {
    GaObject    *   listp;
    int             index; 
};

static GaObject *
list_type_invoke(GaObject *self, struct vm *vm, int argc, GaObject **args)
{
    return GaList_New();
}

static GaObject *
list_append(GaObject *self, struct vm *vm, int argc, GaObject **args)
{
    for (int i = 0; i < argc; i++) {
        GaList_Append(self, args[i]);
    }

    return &_GaNull;
}

static GaObject *
list_remove(GaObject *self, struct vm *vm, int argc, GaObject **args)
{
    for (int i = 0; i < argc; i++) {
        GaList_Remove(self, vm, args[i]);
    }

    return &_GaNull;
}

static void
list_destroy(GaObject *self)
{
    struct list_state *statep = self->un.statep;

    for (int i = 0; i < statep->used_cells; i++) {
        GaObj_DEC_REF(statep->cells[i]);
    }

    free(statep->cells);
    free(statep);
}

static GaObject *
list_getindex(GaObject *self, struct vm *vm, GaObject *key)
{
    struct list_state *statep = self->un.statep;
    GaObject *key_int = GaObj_Super(key, &_GaInt_Type);

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
list_setindex(GaObject *self, struct vm *vm, GaObject *key, GaObject *val)
{
    struct list_state *statep = self->un.statep;
    GaObject *key_int = GaObj_Super(key, &_GaInt_Type);

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

static GaObject *
list_len(GaObject *self, struct vm *vm)
{
    return GaInt_FROM_I64(GaList_Size(self));
}

static GaObject *
list_iter_new(GaObject *listp)
{
    GaObject *obj = GaObj_New(&ga_list_iter_type_inst, &list_iter_ops);
    struct list_iter_state *statep = calloc(sizeof(struct list_iter_state), 1);

    statep->listp = GaObj_INC_REF(listp);
    statep->index = -1;
    obj->un.statep = statep;

    return obj;
}

static void
list_iter_destroy(GaObject *self)
{
    struct list_iter_state *statep = self->un.statep;
    GaObj_DEC_REF(statep->listp);
    free(statep);
}

static GaObject *
list_iter(GaObject *self, struct vm *vm)
{
    return list_iter_new(self);
}

static bool
list_iter_next(GaObject *self, struct vm *vm)
{
    struct list_iter_state *statep = self->un.statep;

    statep->index++;

    return statep->index < GaList_Size(statep->listp);
}

static GaObject *
list_iter_cur(GaObject *self, struct vm *vm)
{
    struct list_iter_state *statep = self->un.statep;
    GaObject *list_obj = GaObj_Super(statep->listp, &_GaList_Type);
    struct list_state *list_statep = list_obj->un.statep;

    return list_statep->cells[statep->index];
}

static GaObject *
list_str(GaObject *self, struct vm *vm)
{
    GaObject *iter_obj = list_iter(self, vm);
    assert(iter_obj != NULL);
    GaObj_INC_REF(iter_obj);
    struct stringbuf *sb = GaStringBuilder_New();
    GaStringBuilder_Append(sb, "[");
    for (int i = 0; list_iter_next(iter_obj, vm); i++) {
        if (i != 0) GaStringBuilder_Append(sb, ", ");
        GaObject *in_obj = GaObj_INC_REF(list_iter_cur(iter_obj, vm));
        assert(in_obj != NULL);
        GaObject *elem_str = GaObj_Super(GaObj_STR(in_obj, vm), GA_STR_TYPE);
        assert(elem_str != NULL);
        GaStringBuilder_Append(sb, GaStr_ToCString(elem_str));
        GaObj_DEC_REF(in_obj);
    }
    GaStringBuilder_Append(sb, "]");
    GaObj_DEC_REF(iter_obj);
    return GaStr_FromStringBuilder(sb);
}

GaObject *
GaList_New()
{
    GaObject *obj = GaObj_New(&_GaList_Type, &list_ops);
    struct list_state *statep = calloc(sizeof(struct list_state), 1);

    statep->cells = calloc(sizeof(struct ga_obj*)*GA_LIST_INITIAL_SIZE, 1);
    statep->avail_cells = GA_LIST_INITIAL_SIZE;

    obj->un.statep = statep;

    GaObj_SETATTR(obj, NULL, "append", GaBuiltin_New(list_append, obj));
    GaObj_SETATTR(obj, NULL, "remove", GaBuiltin_New(list_remove, obj));

    return obj;
}

void
GaList_Append(GaObject *self, GaObject *val)
{
    struct list_state *statep = self->un.statep;
    int index = statep->used_cells;

    if (index >= statep->avail_cells) {
        statep->avail_cells *= 2;
        statep->cells = realloc(statep->cells, sizeof(GaObject *) * statep->avail_cells);
    }

    statep->cells[index] = GaObj_INC_REF(val);
    statep->used_cells++;
}

void
GaList_Remove(GaObject *self, struct vm *vm, GaObject *val)
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
GaList_Size(GaObject *self)
{
    struct list_state *statep = self->un.statep;
    return statep->used_cells;
}

int
_GaList_GetCells(GaObject *self, GaObject ***cells)
{
    struct list_state *statep = self->un.statep;

    *cells = statep->cells;

    return statep->used_cells;
}