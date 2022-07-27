/*
 * function.c - Function type
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
#include <string.h>
#include <gallium/builtins.h>
#include <gallium/dict.h>
#include <gallium/list.h>
#include <gallium/object.h>
#include <gallium/vm.h>

static void             ga_func_destroy(struct ga_obj *);
static struct ga_obj *  ga_func_invoke(struct ga_obj *, struct vm *, int, struct ga_obj **);
static struct ga_obj *  ga_func_str(struct ga_obj *, struct vm *);

GA_BUILTIN_TYPE_DECL(ga_func_type_inst, "Func", NULL);

struct ga_obj_ops ga_func_ops = {
    .destroy        =   ga_func_destroy,
    .invoke         =   ga_func_invoke,
    .str            =   ga_func_str
};

struct ga_func_param {
    int                 flags;
    char                name[];
};

struct ga_func_state {
    struct ga_proc      *   code;
    struct ga_proc      *   parent;
    struct ga_obj       *   mod;
    struct list         *   params;
    struct stackframe   *   captive;
};

static void
ga_func_destroy(struct ga_obj *self)
{
    struct ga_func_state *statep = self->un.statep;
    if (statep->captive) {
        GaFrame_DESTROY(statep->captive);
    }
    if (statep->parent->obj) GaObj_DEC_REF(statep->parent->obj);
}

static struct ga_obj *
ga_func_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    struct ga_func_state *statep = self->un.statep;

    if (argc != LIST_COUNT(statep->params)) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("argument mismatch"));
        return NULL;
    }

    struct stackframe *frame = GaFrame_NEW(statep->mod, statep->code, statep->captive);

    return GaEval_ExecFrame(vm, frame, argc, args);
}

static struct ga_obj *
ga_func_str(struct ga_obj *self, struct vm *vm)
{
    return GaStr_FromCString("Func");
}

struct ga_obj *
GaClosure_New(struct stackframe *captive, struct ga_obj *mod, struct ga_proc *code, struct ga_proc *parent)
{
    struct ga_func_state *statep = calloc(sizeof(struct ga_func_state), 1);
    struct ga_obj *obj = GaObj_New(&ga_func_type_inst, &ga_func_ops);
    statep->code = code;
    statep->parent = parent;
    statep->mod = mod;
    statep->params = GaLinkedList_New();
    statep->captive = captive;
    obj->un.statep = statep;

    captive->ref_count++;

    GaObj_XINC_REF(parent->obj);

    return obj;
}

struct ga_obj *
GaFunc_New(struct ga_obj *mod, struct ga_proc *code, struct ga_proc *parent)
{
    struct ga_func_state *statep = calloc(sizeof(struct ga_func_state), 1);
    struct ga_obj *obj = GaObj_New(&ga_func_type_inst, &ga_func_ops);
    statep->code = code;
    statep->parent = parent;
    statep->mod = mod;
    statep->params = GaLinkedList_New();
    obj->un.statep = statep;

    GaObj_XINC_REF(parent->obj);

    return obj;
}

void
GaFunc_AddParam(struct ga_obj *self, const char *name, int flags)
{
    struct ga_func_state *statep = self->un.statep;
    size_t name_len = strlen(name);
    struct ga_func_param *param = calloc(sizeof(struct ga_func_param) + name_len + 1, 1);
    strcpy(param->name, name);
    param->flags = flags;
    GaLinkedList_Push(statep->params, param);
}