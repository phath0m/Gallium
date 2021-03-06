// delete me
#include <stdio.h>

#include <stdlib.h>
#include <string.h>
#include <gallium/builtins.h>
#include <gallium/dict.h>
#include <gallium/list.h>
#include <gallium/object.h>
#include <gallium/vm.h>

static void             ga_func_destroy(struct ga_obj *);
static struct ga_obj *  ga_func_invoke(struct ga_obj *, struct vm *, int, struct ga_obj **);

GA_BUILTIN_TYPE_DECL(ga_func_type_inst, "Func", NULL);

struct ga_obj_ops ga_func_ops = {
    .destroy        =   ga_func_destroy,
    .invoke         =   ga_func_invoke
};

struct ga_func_param {
    int                 flags;
    char                name[];
};

struct ga_func_state {
    struct ga_proc      *   code;
    struct ga_obj       *   mod;
    struct list         *   params;
    struct stackframe   *   captive;
};

static void
ga_func_destroy(struct ga_obj *self)
{
    struct ga_func_state *statep = self->un.statep;

    if (statep->captive) {
        STACKFRAME_DESTROY(statep->captive);
    }
}

static struct ga_obj *
ga_func_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    struct ga_func_state *statep = self->un.statep;

    if (argc != LIST_COUNT(statep->params)) {
        vm_raise_exception(vm, ga_argument_error_new("argument mismatch"));
        return NULL;
    }

    struct stackframe *frame = STACKFRAME_NEW(statep->mod, statep->code, statep->captive);

    return vm_eval_frame(vm, frame, argc, args);
}

struct ga_obj *
ga_closure_new(struct stackframe *captive, struct ga_obj *mod, struct ga_proc *code)
{
    struct ga_func_state *statep = calloc(sizeof(struct ga_func_state), 1);
    struct ga_obj *obj = ga_obj_new(&ga_func_type_inst, &ga_func_ops);
    statep->code = code;
    statep->mod = mod;
    statep->params = list_new();
    statep->captive = captive;
    obj->un.statep = statep;
    
    captive->ref_count++;

    return obj;
}

struct ga_obj *
ga_func_new(struct ga_obj *mod, struct ga_proc *code)
{
    struct ga_func_state *statep = calloc(sizeof(struct ga_func_state), 1);
    struct ga_obj *obj = ga_obj_new(&ga_func_type_inst, &ga_func_ops);
    statep->code = code;
    statep->mod = mod;
    statep->params = list_new();
    obj->un.statep = statep;
    return obj;
}

void
ga_func_add_param(struct ga_obj *self, const char *name, int flags)
{
    struct ga_func_state *statep = self->un.statep;
    size_t name_len = strlen(name);
    struct ga_func_param *param = calloc(sizeof(struct ga_func_param) + name_len + 1, 1);
    strcpy(param->name, name);
    param->flags = flags;
    list_append(statep->params, param);
}
