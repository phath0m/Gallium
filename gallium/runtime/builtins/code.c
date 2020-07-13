#include <stdlib.h>
#include <gallium/builtins.h>
#include <runtime/bytecode.h>
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
};

static struct ga_obj *
ga_code_invoke_inline_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    struct code_state *statep = self->un.statep;

    return vm_exec_code(vm, vm->top->mod, statep->proc, STACKFRAME_NEW(vm, statep->proc, vm->top), argc, args);
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

    list_destroy(statep->data->constants, constant_destroy_cb, NULL);
    list_destroy(statep->data->procs, proc_destroy_cb, NULL);
    list_destroy(statep->data->strings, string_destroy_cb, NULL);

    free(statep->data);
    free(statep);
}

static struct ga_obj *
ga_code_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    struct code_state *statep = self->un.statep;

    return vm_exec_code(vm, vm->top->mod, statep->proc, NULL, argc, args); 
}

void
ga_code_add_constant(struct ga_obj *self, struct ga_obj *constant)
{
    struct code_state *statep = self->un.statep;

    list_append(statep->data->constants, GAOBJ_INC_REF(constant));
}

struct ga_obj *
ga_code_invoke_inline(struct vm *vm, struct ga_obj *self, struct stackframe *frame)
{
    struct code_state *statep = self->un.statep;
    struct ga_obj *ret = vm_exec_code(vm, frame->mod, statep->proc, STACKFRAME_NEW(vm, statep->proc, frame), 0, NULL);
    
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
