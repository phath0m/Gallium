/*
 * code.c - Builtin code type. Code refers to executable Gallium code. This may
 * or may not be bound to a function.
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
#include <stdio.h>
#include <stdlib.h>
#include <gallium/builtins.h>
#include <gallium/list.h>
#include <gallium/object.h>
#include <gallium/vm.h>

GaObject * _GaCode_type = NULL;

static void         code_destroy(GaObject *);
static GaObject *   code_invoke(GaObject *, GaContext *, int, GaObject **);

static struct ga_obj_ops code_ops = {
    .destroy    =   code_destroy,
    .invoke     =   code_invoke
};

struct code_state {
    struct ga_proc      *   proc;
    struct ga_mod_data  *   data;
    char                    name[];
};

static GaObject *
ga_code_eval(GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_OPTIONAL(vm, 1, (GaObject*[]){ GA_CODE_TYPE,
                                GA_DICT_TYPE, NULL }, argc, args))
    {
        return NULL;
    }

    GaObject *self = GaObj_Super(args[0], GA_CODE_TYPE);
    struct code_state *statep = self->un.statep;
    GaObject *mod = vm->top->mod;

    if (argc == 2) {
        GaObject *dict_obj = GaObj_Super(args[1], GA_DICT_TYPE);

        mod = GaModule_New("__anon__", NULL, NULL);

        struct ga_dict_kvp *kvp;
        _Ga_iter_t iter;

        GaDict_GetITer(dict_obj, &iter);

        while (_Ga_iter_next(&iter, (void**)&kvp)) {
            GaObject *str = GaObj_Super(kvp->key, GA_STR_TYPE);

            if (!str) continue;

            GaObj_SETATTR(mod, vm, GaStr_ToCString(str), kvp->val);
        }
    }

    return GaEval_ExecFrame(vm, GaFrame_NEW(mod, statep->proc, vm->top), 0, args);
}

static void
constant_destroy_cb(void *v, void *s)
{
    GaObj_DEC_REF(v);
}

static void
proc_destroy_cb(void *v, void *s)
{
    ga_proc_destroy(v);
}

static void
string_destroy_cb(void *v, void *s)
{
    free(v);
}

static void
code_destroy(GaObject *self)
{
    struct code_state *statep = self->un.statep;

    GaVec_Fini(&statep->data->object_pool, constant_destroy_cb, NULL);
    GaVec_Fini(&statep->data->proc_pool, proc_destroy_cb, NULL);
    GaVec_Fini(&statep->data->string_pool, string_destroy_cb, NULL);

    free(statep->data);
    free(statep);
}

static GaObject *
code_invoke(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    struct code_state *statep = self->un.statep;
    GaObject *mod = vm->top->mod;

    if (argc == 1) {
        GaObject *dict_obj = Ga_ENSURE_TYPE(vm, args[0], GA_DICT_TYPE);

        if (!dict_obj) {
            return NULL;
        }

        mod = GaModule_New("__anon__", NULL, NULL);

        struct ga_dict_kvp *kvp;
        _Ga_iter_t iter;

        GaDict_GetITer(dict_obj, &iter);

        while (_Ga_iter_next(&iter, (void**)&kvp)) {
            GaObject *str = GaObj_Super(kvp->key, GA_STR_TYPE);

            if (!str) continue;

            GaObj_SETATTR(mod, vm, GaStr_ToCString(str), kvp->val);
        }
    }

    struct stackframe *frame = GaFrame_NEW(mod, statep->proc, NULL);
    return GaEval_ExecFrame(vm, frame, 0, NULL);
}

GaObject *
GaCode_Eval(GaContext *vm, GaObject *self, struct stackframe *frame)
{
    struct code_state *statep = self->un.statep;
    struct stackframe *new_frame = GaFrame_NEW(frame->mod, statep->proc, vm->top);
    return GaEval_ExecFrame(vm, new_frame, 0, NULL);
}

static void
assign_methods(GaObject *obj, GaObject *self)
{
    GaObj_SETATTR(obj, NULL, "eval", GaBuiltin_New(ga_code_eval, self));
}

GaObject *
_GaCode_init()
{
    _GaCode_type = GaObj_NewType("Code", NULL);
    assign_methods(_GaCode_type, NULL);
    return GaObj_INC_REF(_GaCode_type);
}

void
_GaCode_fini()
{
    GaObj_XDEC_REF(_GaCode_type);
}

GaObject *
GaCode_New(struct ga_proc *proc, struct ga_mod_data *data)
{
    GaObject *obj = GaObj_New(GA_CODE_TYPE, &code_ops);
    struct code_state *statep = calloc(sizeof(struct code_state), 1);

    assert(proc->obj == NULL);

    statep->data = data;
    statep->proc = proc;
    proc->obj = obj; 
    obj->un.statep = statep;

    assign_methods(obj, obj);

    return obj;
}

struct ga_proc *
GaCode_GetProc(GaObject *self)
{
    GaObject *self_code = GaObj_Super(self, GA_CODE_TYPE);
    struct code_state *statep = self_code->un.statep;
    return statep->proc;
}