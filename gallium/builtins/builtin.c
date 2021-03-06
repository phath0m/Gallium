#include <stdio.h>
#include <stdlib.h>
#include <gallium/builtins.h>
#include <gallium/object.h>
#include <gallium/vm.h>

GA_BUILTIN_TYPE_DECL(ga_cfunc_type_inst, "Builtin", NULL);

static void                 ga_builtin_destroy(struct ga_obj *);
static struct ga_obj    *   ga_builtin_invoke(struct ga_obj *, struct vm *, int, struct ga_obj **);

struct ga_obj_ops builtin_obj_ops = {
    .destroy    =   ga_builtin_destroy,
    .invoke     =   ga_builtin_invoke
};

struct ga_builtin_state {
    ga_cfunc_t        func;
    struct ga_obj   *   self;
};

static void
ga_builtin_destroy(struct ga_obj *self)
{
    struct ga_builtin_state *statep = self->un.statep;

    if (statep->self) {
        GAOBJ_DEC_REF(statep->self);
    }
}

struct ga_obj *
ga_builtin_new(ga_cfunc_t func, struct ga_obj *self)
{
    struct ga_obj *ret = ga_obj_new(&ga_cfunc_type_inst, &builtin_obj_ops);
    struct ga_builtin_state *statep = calloc(sizeof(struct ga_builtin_state), 1);

    if (self) {
        statep->self = GAOBJ_INC_REF(ga_weakref_new(self));
    }

    statep->func = func;
    ret->un.statep = statep;

    return ret;
}

static struct ga_obj *
ga_builtin_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    struct ga_builtin_state *statep = self->un.statep;

    if (statep->self) {
        self = ga_weakref_val(statep->self);
        
        if (self == &ga_null_inst) {
            printf("BUG: Referenced 'self' object no longer exists!\n");
            return self;
        }
    }

    return statep->func(self, vm, argc, args);
}
