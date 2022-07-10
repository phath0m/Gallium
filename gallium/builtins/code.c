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
#include <stdio.h>
#include <stdlib.h>
#include <gallium/builtins.h>
#include <gallium/list.h>
#include <gallium/object.h>
#include <gallium/vm.h>

GA_BUILTIN_TYPE_DECL(ga_code_type_inst, "Code", NULL);

static void                 ga_code_destroy(struct ga_obj *);
static struct ga_obj    *   ga_code_invoke(struct ga_obj *, struct vm *, int, struct ga_obj **);

static struct ga_obj_ops code_ops = {
    .destroy    =   ga_code_destroy,
    .invoke     =   ga_code_invoke
};

struct code_state {
    struct ga_proc      *   proc;
    struct ga_mod_data  *   data;
    char                    name[];
};

static struct ga_obj *
ga_code_invoke_inline_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    struct code_state *statep = self->un.statep;

    struct ga_obj *mod = vm->top->mod;

    if (argc == 1) {
        struct ga_obj *dict_obj = ga_obj_super(args[0], GA_DICT_TYPE);

        if (!dict_obj) {
            vm_raise_exception(vm, ga_type_error_new("Dict"));
            return NULL;
        }

        mod = ga_mod_new("__anon__", NULL);

        struct ga_dict_kvp *kvp;
        list_iter_t iter;

        ga_dict_get_iter(dict_obj, &iter);

        while (iter_next_elem(&iter, (void**)&kvp)) {
            struct ga_obj *str = ga_obj_super(kvp->key, GA_STR_TYPE);

            if (!str) continue;

            GAOBJ_SETATTR(mod, vm, ga_str_to_cstring(str), kvp->val);
        }
    }

    return vm_eval_frame(vm, STACKFRAME_NEW(mod, statep->proc, vm->top), 0, args);
}

static void
constant_destroy_cb(void *v, void *s)
{
    GAOBJ_DEC_REF(v);
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
ga_code_destroy(struct ga_obj *self)
{
    struct code_state *statep = self->un.statep;

    vec_fini(&statep->data->object_pool, constant_destroy_cb, NULL);
    vec_fini(&statep->data->proc_pool, proc_destroy_cb, NULL);
    vec_fini(&statep->data->string_pool, string_destroy_cb, NULL);

    free(statep->data);
    free(statep);
}

static struct ga_obj *
ga_code_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    struct code_state *statep = self->un.statep;

    struct ga_obj *mod = vm->top->mod;

    if (argc == 1) {
        struct ga_obj *dict_obj = ga_obj_super(args[0], GA_DICT_TYPE);

        if (!dict_obj) {
            vm_raise_exception(vm, ga_type_error_new("Dict"));
            return NULL;
        }

        mod = ga_mod_new("__anon__", NULL);

        struct ga_dict_kvp *kvp;
        list_iter_t iter;

        ga_dict_get_iter(dict_obj, &iter);

        while (iter_next_elem(&iter, (void**)&kvp)) {
            struct ga_obj *str = ga_obj_super(kvp->key, GA_STR_TYPE);

            if (!str) continue;

            GAOBJ_SETATTR(mod, vm, ga_str_to_cstring(str), kvp->val);
        }
    }

    struct stackframe *frame = STACKFRAME_NEW(mod, statep->proc, NULL);
    
    struct ga_obj *ret = vm_eval_frame(vm, frame, 0, NULL);

    return ret;
}

struct ga_obj *
ga_code_invoke_inline(struct vm *vm, struct ga_obj *self, struct stackframe *frame)
{
    struct code_state *statep = self->un.statep;
    struct stackframe *new_frame = STACKFRAME_NEW(frame->mod, statep->proc, vm->top);

    struct ga_obj *ret = vm_eval_frame(vm, new_frame, 0, NULL);
    
    return ret;
}

struct ga_obj *
ga_code_new(struct ga_proc *proc, struct ga_mod_data *data)
{
    struct ga_obj *obj = ga_obj_new(&ga_code_type_inst, &code_ops);
    struct code_state *statep = calloc(sizeof(struct code_state), 1);

    statep->data = data;
    statep->proc = proc;

    obj->un.statep = statep;

    GAOBJ_SETATTR(obj, NULL, "invoke_inline", ga_builtin_new(ga_code_invoke_inline_method, obj));

    return obj;
}

struct ga_proc *
ga_code_get_proc(struct ga_obj *self)
{
    struct ga_obj *self_code = ga_obj_super(self, &ga_code_type_inst);
    struct code_state *statep = self_code->un.statep;

    return statep->proc;
}