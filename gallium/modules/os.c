#include <stdio.h>
#include <stdlib.h>
#include <gallium/builtins.h>
#include <gallium/object.h>
#include <gallium/vm.h>

static struct ga_obj *
getenv_builtin(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        vm_raise_exception(vm, ga_argument_error_new("compile() requires one argument"));
        return NULL;
    }

    struct ga_obj *name = ga_obj_super(args[0], GA_STR_TYPE);

    if (!name) {
        vm_raise_exception(vm, ga_type_error_new("Str"));
        return NULL;
    }

    char *val = getenv(ga_str_to_cstring(name));

    if (val) {
        return ga_str_from_cstring(val);
    }

    
    return GA_NULL;

}

struct ga_obj *
ga_os_mod_open()
{
    static struct ga_obj *mod = NULL;

    if (mod) {
        return mod;
    }

    mod = ga_mod_new("os", NULL);

    GAOBJ_SETATTR(mod, NULL, "getenv", ga_builtin_new(getenv_builtin, NULL));

    return mod;
}

