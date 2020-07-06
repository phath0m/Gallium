#include <stdio.h>
#include <gallium/builtins.h>
#include <gallium/list.h>
#include <gallium/object.h>
#include <gallium/vm.h>

static struct ga_obj *
super_builtin(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc < 1) {
        return NULL;
    }

    struct ga_obj *obj = args[0];
    struct ga_obj *clazz = obj->type;

    if (clazz->type != &ga_class_type_inst) {
        vm_raise_exception(vm, ga_type_error_new("Object"));
        return NULL;
    }

   
    struct ga_obj *base = ga_class_base(clazz);
    struct ga_obj *super_inst = GAOBJ_INVOKE(base, vm, argc - 1, &args[1]);

    obj->super = GAOBJ_INC_REF(super_inst);

    GAOBJ_SETATTR(obj, vm, "super", super_inst);

    return &ga_null_inst;
}

static struct ga_obj *
filter_builtin(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 2) {
        vm_raise_exception(vm, ga_argument_error_new("map() requires two arguments"));
        return NULL;
    }

    struct ga_obj *func = args[0];
    struct ga_obj *collection = args[1];
    struct ga_obj *iter_obj = ga_obj_iter(collection, vm);

    if (!iter_obj) {
        return NULL;
    }

    GAOBJ_INC_REF(iter_obj);

    struct ga_obj *ret = NULL;
    struct list *listp = list_new();
    struct ga_obj *in_obj = NULL;

    while (GAOBJ_ITER_NEXT(iter_obj, vm)) {
        in_obj = GAOBJ_INC_REF(GAOBJ_ITER_CUR(iter_obj, vm));

        if (!in_obj) {
            goto cleanup;
        }

        if (GAOBJ_IS_TRUE(GAOBJ_INVOKE(func, vm, 1, &in_obj), vm)) {
            list_append(listp, GAOBJ_MOVE_REF(in_obj));
        } else {
            GAOBJ_DEC_REF(in_obj);
        }
    }

    ret = ga_tuple_new(LIST_COUNT(listp));

    int i = 0;
    list_iter_t iter;
    list_get_iter(listp, &iter);

    while (iter_next_elem(&iter, (void**)&in_obj)) {
        ga_tuple_init_elem(ret, i++, in_obj);
    }

cleanup:
    GAOBJ_DEC_REF(iter_obj);
    list_destroy(listp, NULL, NULL);
    return ret;
}

static struct ga_obj *
len_builtin(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        vm_raise_exception(vm, ga_argument_error_new("len() requires one argument"));
        return NULL;
    }

    struct ga_obj *collection = args[0];
    struct ga_obj *res = ga_obj_len(collection, vm);
    
    if (!res) {
        vm_raise_exception(vm, ga_type_error_new("len()"));
        return NULL;
    }

    return res;
}

static struct ga_obj *
map_builtin(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 2) {
        vm_raise_exception(vm, ga_argument_error_new("map() requires two arguments"));
        return NULL;
    }
    
    struct ga_obj *func = args[0];
    struct ga_obj *collection = args[1];
    struct ga_obj *iter_obj = ga_obj_iter(collection, vm);

    if (!iter_obj) {
        return NULL;
    }

    GAOBJ_INC_REF(iter_obj);

    struct ga_obj *ret = NULL;
    struct list *listp = list_new();
    struct ga_obj *in_obj = NULL;
    struct ga_obj *out_obj = NULL;

    while (GAOBJ_ITER_NEXT(iter_obj, vm)) {
        in_obj = GAOBJ_INC_REF(GAOBJ_ITER_CUR(iter_obj, vm));

        if (!in_obj) {
            goto cleanup;
        }
        
        out_obj = GAOBJ_INVOKE(func, vm, 1, &in_obj);

        GAOBJ_DEC_REF(in_obj);

        if (!out_obj) {
            goto cleanup;
        }

        list_append(listp, out_obj);
    }

    ret = ga_tuple_new(LIST_COUNT(listp));

    int i = 0;
    list_iter_t iter;
    list_get_iter(listp, &iter);

    while (iter_next_elem(&iter, (void**)&out_obj)) {
        ga_tuple_init_elem(ret, i++, out_obj);
    }

cleanup:
    GAOBJ_DEC_REF(iter_obj);
    list_destroy(listp, NULL, NULL);
    return ret;
}

static struct ga_obj *
print_builtin(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    for (int i = 0; i < argc; i++) {
        struct ga_obj *obj = args[i];
        struct ga_obj *str = GAOBJ_INC_REF(GAOBJ_STR(obj, vm));

        fputs(ga_str_to_cstring(str), stdout);
    
        GAOBJ_DEC_REF(str);
    }
    
    puts("");
    return &ga_null_inst;
}

struct ga_obj *
ga_builtin_mod()
{
    static struct ga_obj *mod = NULL;

    if (mod) {
        return mod;
    }
    
    mod = ga_mod_new("__builtins__", NULL);

    GAOBJ_SETATTR(mod, NULL, "filter", ga_builtin_new(filter_builtin, NULL));
    GAOBJ_SETATTR(mod, NULL, "len", ga_builtin_new(len_builtin, NULL));
    GAOBJ_SETATTR(mod, NULL, "map", ga_builtin_new(map_builtin, NULL));
    GAOBJ_SETATTR(mod, NULL, "print", ga_builtin_new(print_builtin, NULL));
    GAOBJ_SETATTR(mod, NULL, "super", ga_builtin_new(super_builtin, NULL));
    GAOBJ_SETATTR(mod, NULL, "Dict", &ga_dict_type_inst);
    GAOBJ_SETATTR(mod, NULL, "Int", &ga_int_type_inst);
    GAOBJ_SETATTR(mod, NULL, "List", &ga_list_type_inst);
    GAOBJ_SETATTR(mod, NULL, "Object", &ga_obj_type_inst);
    GAOBJ_SETATTR(mod, NULL, "Range", &ga_range_type_inst);
    GAOBJ_SETATTR(mod, NULL, "Str", &ga_str_type_inst);
    GAOBJ_SETATTR(mod, NULL, "Type", &ga_type_type_inst);
    GAOBJ_SETATTR(mod, NULL, "WeakRef", &ga_weakref_type_inst);
    
    return mod;
}
