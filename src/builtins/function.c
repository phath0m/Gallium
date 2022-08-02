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
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <gallium/builtins.h>
#include <gallium/dict.h>
#include <gallium/list.h>
#include <gallium/object.h>
#include <gallium/vm.h>

static void         func_destroy(GaObject *);
static GaObject *   func_invoke(GaObject *, GaContext *, int, GaObject **);
static GaObject *   func_str(GaObject *, GaContext *);

GA_BUILTIN_TYPE_DECL(_GaFunc_Type, "Func", NULL);

static struct ga_obj_ops func_ops = {
    .destroy        =   func_destroy,
    .invoke         =   func_invoke,
    .str            =   func_str
};

struct func_param {
    int                 flags;
    char                name[];
};

struct func_state {
    struct ga_proc      *   code;
    struct ga_proc      *   parent;
    GaObject            *   mod;
    bool                    variadic;       /* whether or not this is variadic */
    int                     argc;           /* How many positional arguments to expect */
    _Ga_list_t          *   params;
    struct stackframe   *   captive;
};

static void
func_destroy(GaObject *self)
{
    struct func_state *statep = self->un.statep;
    if (statep->captive) {
        GaFrame_DESTROY(statep->captive);
    }
    if (statep->parent->obj) GaObj_DEC_REF(statep->parent->obj);
}

static GaObject *
func_invoke_variadic(struct func_state *statep, struct stackframe *frame,
                     GaContext *vm, int argc, GaObject **args)
{
    /* Resize args and append a tuple containing any remaining args */
    GaObject **new_args = calloc(statep->argc + 1, sizeof(GaObject *));
    GaObject *tuple = GaTuple_New(argc - statep->argc);
    memccpy(new_args, args, statep->argc, sizeof(GaObject *));
    new_args[statep->argc] = tuple;
    for (int i = statep->argc; i < argc; i++) {
        GaTuple_InitElem(tuple, i - statep->argc, args[i]);
    }
    GaObject *ret = GaEval_ExecFrame(vm, frame, statep->argc + 1, new_args);
    free(new_args);
    return ret;
}

static GaObject *
func_invoke(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    struct func_state *statep = self->un.statep;
    bool variable_args = statep->variadic;

    if (!((variable_args && Ga_CHECK_ARG_COUNT_MIN(vm, statep->argc, argc)) ||
                            Ga_CHECK_ARG_COUNT_EXACT(vm, statep->argc, argc)))
    {
        return NULL;
    }

    struct stackframe *frame = GaFrame_NEW(statep->mod, statep->code, statep->captive);

    if (statep->variadic) {
        return func_invoke_variadic(statep, frame, vm, argc, args);
    } else {
        return GaEval_ExecFrame(vm, frame, argc, args);
    }
}

static GaObject *
func_str(GaObject *self, GaContext *vm)
{
    return GaStr_FromCString("Func");
}

GaObject *
GaClosure_New(struct stackframe *captive, GaObject *mod, struct ga_proc *code, struct ga_proc *parent)
{
    struct func_state *statep = calloc(sizeof(struct func_state), 1);
    GaObject *obj = GaObj_New(&_GaFunc_Type, &func_ops);
    statep->code = code;
    statep->parent = parent;
    statep->mod = mod;
    statep->params = _Ga_list_new();
    statep->captive = captive;
    obj->un.statep = statep;

    captive->ref_count++;

    GaObj_XINC_REF(parent->obj);

    return obj;
}

GaObject *
GaFunc_New(GaObject *mod, struct ga_proc *code, struct ga_proc *parent)
{
    struct func_state *statep = calloc(sizeof(struct func_state), 1);
    GaObject *obj = GaObj_New(&_GaFunc_Type, &func_ops);
    statep->code = code;
    statep->parent = parent;
    statep->mod = mod;
    statep->params = _Ga_list_new();
    obj->un.statep = statep;

    GaObj_XINC_REF(parent->obj);

    return obj;
}

void
GaFunc_AddParam(GaObject *self, const char *name, int flags)
{
    struct func_state *statep = self->un.statep;
    size_t name_len = strlen(name);
    struct func_param *param = calloc(sizeof(struct func_param) + name_len + 1, 1);
    strcpy(param->name, name);
    param->flags = flags;

    if ((flags & GaFunc_VARIADIC)) {
        statep->variadic = true;
    } else {
        statep->argc++;
    }

    _Ga_list_push(statep->params, param);
}