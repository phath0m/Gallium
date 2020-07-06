#include <stdlib.h>
#include <string.h>
#include <gallium/builtins.h>
#include <gallium/object.h>
#include <gallium/vm.h>
#include <runtime/bytecode.h>

static struct ga_obj *  ga_mod_invoke(struct ga_obj *, struct vm *, int, struct ga_obj **);

struct ga_obj ga_mod_typedef_inst = {
    .ref_count      =   1,
    .un.statep      =   "Module"
};

struct ga_obj_ops ga_mod_ops = {
    .invoke         =   ga_mod_invoke
};

struct ga_mod_state {
    struct ga_code  *   constructor;
    char                name[];
};

static struct ga_obj *
ga_mod_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    struct ga_mod_state *statep = self->un.statep;

    return vm_exec_code(vm, statep->constructor, NULL, 0, NULL);
}

struct ga_obj *
ga_mod_new(const char *name, struct ga_code *constructor)
{
    size_t name_len = strlen(name);
    struct ga_mod_state *statep = calloc(sizeof(struct ga_mod_state) + name_len, 1);
    struct ga_obj *mod = ga_obj_new(&ga_mod_typedef_inst, &ga_mod_ops);
    strncpy(statep->name, name, name_len);
    statep->constructor = constructor;
    
    if (constructor) {
        constructor->mod = mod;
    }

    mod->un.statep = statep;
    return mod;
}

void
ga_mod_import(struct ga_obj *self, struct vm *vm, struct ga_obj *mod)
{
    list_iter_t iter;
    dict_get_iter(mod->dict, &iter);

    struct dict_kvp *kvp;

    while (iter_next_elem(&iter, (void**)&kvp)) {
        GAOBJ_SETATTR(self, vm, kvp->key, (struct ga_obj*)kvp->val);
    }
}
