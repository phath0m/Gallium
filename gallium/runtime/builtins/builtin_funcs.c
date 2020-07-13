#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <gallium/builtins.h>
#include <gallium/compiler.h>
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
compile_builtin(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        vm_raise_exception(vm, ga_argument_error_new("compile() requires one argument"));
        return NULL;
    }

    struct ga_obj *ast = ga_obj_super(args[0], &ga_astnode_type_inst);

    if (!ast) {
        vm_raise_exception(vm, ga_type_error_new("ast.AstNode"));
        return NULL;
    }

    struct compiler_state compiler;
   
    memset(&compiler, 0, sizeof(compiler));

    struct ga_obj *ret = compiler_compile_ast(&compiler, ast->un.statep);

    return ret;
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
input_builtin(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc > 1) {
        vm_raise_exception(vm, ga_argument_error_new("input() accepts one optional argument"));
        return NULL;
    }

    if (argc == 1) {
        struct ga_obj *prompt_str = GAOBJ_INC_REF(GAOBJ_STR(args[0], vm));
        fputs(ga_str_to_cstring(prompt_str), stdout);
        fflush(stdout);
        GAOBJ_DEC_REF(prompt_str);
    }
    
    struct ga_obj *ret = NULL;
    char *lineptr = NULL;
    size_t nchars;

    if (getline(&lineptr, &nchars, stdin) < 0) {
        vm_raise_exception(vm, ga_internal_error_new("input(): getline() failed!"));
    } else {
        ret = ga_str_from_cstring(lineptr);
    }

    if (lineptr) free(lineptr);

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
open_builtin(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 2) {
        vm_raise_exception(vm, ga_argument_error_new("open() requires two arguments"));
        return NULL;
    }

    struct ga_obj *file_str = ga_obj_super(args[0], &ga_str_type_inst);
    struct ga_obj *mode_str = ga_obj_super(args[1], &ga_str_type_inst);

    if (!file_str || !mode_str) {
        vm_raise_exception(vm, ga_type_error_new("Str"));
        return NULL;
    }

    mode_t mode = 0;
    const char *file_cstring = ga_str_to_cstring(file_str);
    const char *mode_cstring = ga_str_to_cstring(mode_str);

    for (int i = 0; i < ga_str_len(mode_str); i++) {
        switch (mode_cstring[i]) {
            case 'a':
                mode |= O_APPEND;
                break;
            case 'r':
                mode |= O_RDONLY;
                break;
            case 'w':
                mode |= O_WRONLY;
                break;
            default:
                /* raise format error */
                break;
        }
    }

    if ((mode & O_WRONLY) && (mode & O_APPEND) == 0) {
        if (access(file_cstring, F_OK) != 0)
            mode |= O_CREAT;
        else 
            mode |= O_TRUNC;
    }

    int fd = open(file_cstring, mode, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);


    if (fd < 0) {
        const char *msg = "An unknown error occured";

        switch (errno) {
            case EACCES:
                msg = "Access denied";
                break;
            case EISDIR:
                msg = "Is a directory";
                break;
            case ENOENT:
                msg = "File or directory does not exist";
                break;
            default:
                break;
        }

        vm_raise_exception(vm, ga_io_error_new(msg));

        return NULL;
    }

    return ga_file_new(fd, mode);
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

    GAOBJ_SETATTR(mod, NULL, "compile", ga_builtin_new(compile_builtin, NULL));
    GAOBJ_SETATTR(mod, NULL, "filter", ga_builtin_new(filter_builtin, NULL));
    GAOBJ_SETATTR(mod, NULL, "input", ga_builtin_new(input_builtin, NULL));
    GAOBJ_SETATTR(mod, NULL, "len", ga_builtin_new(len_builtin, NULL));
    GAOBJ_SETATTR(mod, NULL, "map", ga_builtin_new(map_builtin, NULL));
    GAOBJ_SETATTR(mod, NULL, "open", ga_builtin_new(open_builtin, NULL));
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

    /* Note: This is a HACK until I implement the use statement to import modules... */
    GAOBJ_SETATTR(mod, NULL, "ast", ga_ast_mod_open());

    return mod;
}
