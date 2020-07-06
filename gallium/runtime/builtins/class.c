#include <stdlib.h>
#include <string.h>
#include <gallium/builtins.h>
#include <gallium/dict.h>
#include <gallium/object.h>
#include <gallium/vm.h>

GA_BUILTIN_TYPE_DECL(ga_class_type_inst, "Class", NULL);

static void ga_class_destroy(struct ga_obj *);
static struct ga_obj *ga_class_invoke(struct ga_obj *, struct vm *, int, struct ga_obj **);

struct ga_obj_ops class_ops = {
    .destroy    =   ga_class_destroy,
    .invoke     =   ga_class_invoke
};

struct ga_class_state {
    struct ga_obj   *   ctr;
    struct ga_obj   *   base;
};

static void
ga_class_destroy(struct ga_obj *self)
{
    struct ga_class_state *statep = self->un.statep;

    if (statep->base) {
        GAOBJ_DEC_REF(statep->base);
    }

    if (statep->ctr) {
        GAOBJ_DEC_REF(statep->ctr);
    }
}

static struct ga_obj *
ga_class_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    struct ga_obj *obj_inst = GAOBJ_INC_REF(ga_obj_new(self, NULL));
    struct dict *dictp = self->dict;
    struct dict_kvp *kvp;

    list_iter_t iter;
    dict_get_iter(dictp, &iter);

    while (iter_next_elem(&iter, (void**)&kvp)) {
        struct ga_obj *method = kvp->val;

        if (method->type == &ga_func_type_inst) {
            GAOBJ_SETATTR(obj_inst, vm, kvp->key, ga_method_new(obj_inst, kvp->val));
        } else {
            GAOBJ_SETATTR(obj_inst, vm, kvp->key, kvp->val);
        }
    }

    struct ga_class_state *statep = self->un.statep;

    if (statep->ctr) {
        struct ga_obj *new_args[128];

        for (int i = 0; i < argc; i++) {
            new_args[i + 1] = args[i];
        }

        new_args[0] = obj_inst;
        GAOBJ_INVOKE(statep->ctr, vm, argc + 1, new_args);
    }

    if (!statep->ctr || !obj_inst->super) {
        struct ga_obj *super_inst = GAOBJ_INVOKE(statep->base, vm, 0, NULL);
        obj_inst->super = GAOBJ_INC_REF(super_inst);
    }

    return GAOBJ_MOVE_REF(obj_inst);
}

struct ga_obj *
ga_class_base(struct ga_obj *self)
{
    struct ga_class_state *statep = self->un.statep;

    return statep->base;
}

struct ga_obj *
ga_class_new(const char *name, struct ga_obj *base, struct ga_obj *dict)
{
    struct ga_class_state *statep = calloc(sizeof(struct ga_class_state), 1);
    struct ga_obj *clazz = ga_obj_new(&ga_class_type_inst, &class_ops);
    struct ga_dict_kvp *kvp;

    clazz->super = GAOBJ_INC_REF(ga_type_new(name));

    list_iter_t iter;
    ga_dict_get_iter(dict, &iter);

    while (iter_next_elem(&iter, (void**)&kvp)) {
        if (kvp->key->type != &ga_str_type_inst) {
            /* this shouldn't happen */
            return NULL;
        }

        const char *str = ga_str_to_cstring(kvp->key);

        if (strcmp(str, name) != 0) {
            GAOBJ_SETATTR(clazz, NULL, str, kvp->val);
        } else {
            statep->ctr = GAOBJ_INC_REF(kvp->val);
        }
    }

    statep->base = GAOBJ_INC_REF(base);
    clazz->un.statep = statep;

    return clazz;
}
