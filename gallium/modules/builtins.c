/*
 * builtins.c - Gallium global namespace level built-ins 
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
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <gallium/builtins.h>
#include <gallium/compiler.h>
#include <gallium/list.h>
#include <gallium/object.h>
#include <gallium/vm.h>

ssize_t getline(char **, size_t *, FILE *);

static struct ga_obj *
super_builtin(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc < 1) {
        vm_raise_exception(vm, ga_argument_error_new("super() requires at least one argument"));
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
chr_builtin(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        vm_raise_exception(vm, ga_argument_error_new("chr() requires one argument"));
        return NULL;
    }

    struct ga_obj *int_obj = ga_obj_super(args[0], &ga_int_type_inst);

    if (!int_obj) {
        vm_raise_exception(vm, ga_type_error_new("Int"));
        return NULL;
    }

    char str[] = {
        (uint8_t)(GA_INT_TO_I64(int_obj)),
        0
    };

    return ga_str_from_cstring(str);
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

        for (int i = 0; lineptr[i]; i++)
            if (lineptr[i] == '\n') lineptr[i] = '\0';

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

        if (!out_obj) {
            goto cleanup;
        }

        GAOBJ_INC_REF(out_obj);
        GAOBJ_DEC_REF(in_obj);
        
        list_append(listp, out_obj);
    }

    ret = ga_tuple_new(LIST_COUNT(listp));

    int i = 0;
    list_iter_t iter;
    list_get_iter(listp, &iter);

    while (iter_next_elem(&iter, (void**)&out_obj)) {
        ga_tuple_init_elem(ret, i++, GAOBJ_MOVE_REF(out_obj));
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
    
    return &ga_null_inst;
}

static struct ga_obj *
puts_builtin(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
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

static struct ga_obj *builtins_singleton = NULL;

__attribute__((constructor))
static void
init_builtin_singleton()
{
    builtins_singleton = GAOBJ_INC_REF(ga_builtin_mod());
}

__attribute__((destructor))
static void
fini_builtin_singleton()
{
    GAOBJ_DEC_REF(builtins_singleton);
}

struct ga_obj *
ga_builtin_mod()
{
    if (builtins_singleton) return builtins_singleton;
    
    builtins_singleton = ga_mod_new("__builtins__", NULL, NULL);

    GAOBJ_SETATTR(builtins_singleton, NULL, "chr", ga_builtin_new(chr_builtin, NULL));
    GAOBJ_SETATTR(builtins_singleton, NULL, "compile", ga_builtin_new(compile_builtin, NULL));
    GAOBJ_SETATTR(builtins_singleton, NULL, "filter", ga_builtin_new(filter_builtin, NULL));
    GAOBJ_SETATTR(builtins_singleton, NULL, "input", ga_builtin_new(input_builtin, NULL));
    GAOBJ_SETATTR(builtins_singleton, NULL, "len", ga_builtin_new(len_builtin, NULL));
    GAOBJ_SETATTR(builtins_singleton, NULL, "map", ga_builtin_new(map_builtin, NULL));
    GAOBJ_SETATTR(builtins_singleton, NULL, "null", GA_NULL);
    GAOBJ_SETATTR(builtins_singleton, NULL, "open", ga_builtin_new(open_builtin, NULL));
    GAOBJ_SETATTR(builtins_singleton, NULL, "print", ga_builtin_new(print_builtin, NULL));
    GAOBJ_SETATTR(builtins_singleton, NULL, "puts", ga_builtin_new(puts_builtin, NULL));
    GAOBJ_SETATTR(builtins_singleton, NULL, "super", ga_builtin_new(super_builtin, NULL));
    GAOBJ_SETATTR(builtins_singleton, NULL, "Dict", &ga_dict_type_inst);
    GAOBJ_SETATTR(builtins_singleton, NULL, "Int", &ga_int_type_inst);
    GAOBJ_SETATTR(builtins_singleton, NULL, "List", &ga_list_type_inst);
    GAOBJ_SETATTR(builtins_singleton, NULL, "MutStr", GA_MUTSTR_TYPE);
    GAOBJ_SETATTR(builtins_singleton, NULL, "Object", &ga_obj_type_inst);
    GAOBJ_SETATTR(builtins_singleton, NULL, "Range", &ga_range_type_inst);
    GAOBJ_SETATTR(builtins_singleton, NULL, "Str", &ga_str_type_inst);
    GAOBJ_SETATTR(builtins_singleton, NULL, "Type", &ga_type_type_inst);
    GAOBJ_SETATTR(builtins_singleton, NULL, "WeakRef", &ga_weakref_type_inst);

    GAOBJ_SETATTR(builtins_singleton, NULL, "stdout", ga_file_new(1, O_WRONLY));
    GAOBJ_SETATTR(builtins_singleton, NULL, "Enumerable", ga_enumerable_new());

    /* Note: This is a HACK until I implement the use statement to import modules... */
    //GAOBJ_SETATTR(builtins_singleton, NULL, "ast", ga_ast_mod_open());
    //GAOBJ_SETATTR(builtins_singleton, NULL, "parser", ga_parser_mod_open());

    return builtins_singleton;
}