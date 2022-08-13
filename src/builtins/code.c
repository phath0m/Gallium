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
static GaObject *   code_get_attr(GaObject *, GaContext *, const char *);
static GaObject *   code_invoke(GaObject *, GaContext *, int, GaObject **);

static struct Ga_Operators code_ops = {
    .destroy    =   code_destroy,
    .invoke     =   code_invoke,
    .getattr    =   code_get_attr
};

static GaObject *
ga_code_eval(GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_OPTIONAL(vm, 1, (GaObject*[]){ GA_CODE_TYPE,
                                GA_DICT_TYPE, NULL }, argc, args))
    {
        return NULL;
    }
    GaCodeObject *self = (GaCodeObject *)GaObj_Super(args[0], GA_CODE_TYPE);
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
    return GaEval_ExecFrame(vm, GaFrame_NEW(mod, self, vm->top), 0, args);
}

static void
constant_destroy_cb(void *v, void *s)
{
    GaObj_DEC_REF(v);
}

static void
string_destroy_cb(void *v, void *s)
{
    free(v);
}

static void
code_destroy(GaObject *self)
{
    GaCodeObject *co = (GaCodeObject *)self;
    GaVec_Fini(&co->object_pool, constant_destroy_cb, NULL);
    GaVec_Fini(&co->string_pool, string_destroy_cb, NULL);
}

static GaObject *
get_bytecode(GaCodeObject *co)
{
    GaObject *ret = GaTuple_New(co->bytecode_len);
    for (int i = 0; i < co->bytecode_len; i++) {
        GaObject *ins = GaTuple_New(2);
        int opcode = GA_INS_OPCODE(co->bytecode[i]);
        int immediate = GA_INS_IMMEDIATE(co->bytecode[i]);
        GaTuple_InitElem(ins, 0, GaInt_FROM_I64(opcode));
        GaTuple_InitElem(ins, 1, GaInt_FROM_I64(immediate));
        GaTuple_InitElem(ret, i, ins);
    }
    return ret;
}

static GaObject *
code_get_attr(GaObject *self, GaContext *vm, const char *name)
{
    GaCodeObject *co = (GaCodeObject*)self;
    if (strcmp(name, "constants") == 0) {
        return GaTuple_FromVec(&co->object_pool);
    }
    else if (strcmp(name, "names") == 0) {
        GaObject *ret = GaTuple_New(co->string_pool.used_cells);
        for (int i = 0; i < co->string_pool.used_cells; i++) {
            struct GaCodeStringEntry *ent = co->string_pool.cells[i];
            GaTuple_InitElem(ret, i, GaStr_FromCString(ent->value));
        }
        return ret;
    }
    else if (strcmp(name, "bytecode") == 0) {
        return get_bytecode(co);
    }
    /* not found */
    return NULL;
}

static GaObject *
code_invoke(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    GaCodeObject *co = (GaCodeObject *)self;
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
    struct stackframe *frame = GaFrame_NEW(mod, co, NULL);
    return GaEval_ExecFrame(vm, frame, 0, NULL);
}

GaObject *
GaCode_Eval(GaContext *vm, GaObject *self, struct stackframe *frame)
{
    GaCodeObject *co = (GaCodeObject *)self;
    struct stackframe *new_frame = GaFrame_NEW(frame->mod, co, vm->top);
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
GaCode_New(const char *name)
{
    size_t size = sizeof(GaCodeObject)+strlen(name)+1;
    GaCodeObject *obj = (GaCodeObject *)GaObj_NewEx(GA_CODE_TYPE, &code_ops, size);
    assign_methods((GaObject *)obj, (GaObject *)obj);
    return (GaObject *)obj;
}

GaCodeObject *
GaCode_GetProc(GaObject *self)
{
    GaObject *self_code = GaObj_Super(self, GA_CODE_TYPE);
    return (GaCodeObject *)self_code;
}