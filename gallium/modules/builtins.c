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
        GaEval_RaiseException(vm, GaErr_NewArgumentError("super() requires at least one argument"));
        return NULL;
    }

    struct ga_obj *obj = args[0];
    struct ga_obj *clazz = obj->type;

    if (clazz->type != &ga_class_type_inst) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Object"));
        return NULL;
    }
   
    struct ga_obj *base = GaClass_Base(clazz);
    struct ga_obj *super_inst = GaObj_INVOKE(base, vm, argc - 1, &args[1]);

    obj->super = GaObj_INC_REF(super_inst);

    GaObj_SETATTR(obj, vm, "super", super_inst);

    return &ga_null_inst;
}

static struct ga_obj *
chr_builtin(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("chr() requires one argument"));
        return NULL;
    }

    struct ga_obj *int_obj = GaObj_Super(args[0], &ga_int_type_inst);

    if (!int_obj) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Int"));
        return NULL;
    }

    char str[] = {
        (uint8_t)(GaInt_TO_I64(int_obj)),
        0
    };

    return GaStr_FromCString(str);
}


static struct ga_obj *
compile_builtin(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("compile() requires one argument"));
        return NULL;
    }

    struct ga_obj *ast = GaObj_Super(args[0], &ga_astnode_type_inst);

    if (!ast) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("ast.AstNode"));
        return NULL;
    }

    struct compiler_state compiler;
   
    memset(&compiler, 0, sizeof(compiler));

    struct ga_obj *ret = GaAst_Compile(&compiler, ast->un.statep);

    return ret;
}

static struct ga_obj *
filter_builtin(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 2) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("map() requires two arguments"));
        return NULL;
    }

    struct ga_obj *func = args[0];
    struct ga_obj *collection = args[1];
    struct ga_obj *iter_obj = GaObj_ITER(collection, vm);

    if (!iter_obj) {
        return NULL;
    }

    GaObj_INC_REF(iter_obj);

    struct ga_obj *ret = NULL;
    struct list *listp = GaLinkedList_New();
    struct ga_obj *in_obj = NULL;

    while (GaObj_ITER_NEXT(iter_obj, vm)) {
        in_obj = GaObj_INC_REF(GaObj_ITER_CUR(iter_obj, vm));

        if (!in_obj) {
            goto cleanup;
        }

        if (GaObj_IS_TRUE(GaObj_INVOKE(func, vm, 1, &in_obj), vm)) {
            GaLinkedList_Push(listp, GaObj_MOVE_REF(in_obj));
        } else {
            GaObj_DEC_REF(in_obj);
        }
    }

    ret = GaTuple_New(LIST_COUNT(listp));

    int i = 0;
    list_iter_t iter;
    GaLinkedList_GetIter(listp, &iter);

    while (GaIter_Next(&iter, (void**)&in_obj)) {
        GaTuple_InitElem(ret, i++, in_obj);
    }

cleanup:
    GaObj_DEC_REF(iter_obj);
    GaLinkedList_Destroy(listp, NULL, NULL);
    return ret;
}

static struct ga_obj *
input_builtin(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc > 1) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("input() accepts one optional argument"));
        return NULL;
    }

    if (argc == 1) {
        struct ga_obj *prompt_str = GaObj_INC_REF(GaObj_STR(args[0], vm));
        fputs(GaStr_ToCString(prompt_str), stdout);
        fflush(stdout);
        GaObj_DEC_REF(prompt_str);
    }
    
    struct ga_obj *ret = NULL;
    char *lineptr = NULL;
    size_t nchars;

    if (getline(&lineptr, &nchars, stdin) < 0) {
        GaEval_RaiseException(vm, GaErr_NewInternalError("input(): getline() failed!"));
    } else {

        for (int i = 0; lineptr[i]; i++)
            if (lineptr[i] == '\n') lineptr[i] = '\0';

        ret = GaStr_FromCString(lineptr);
    }

    if (lineptr) free(lineptr);

    return ret;
}

static struct ga_obj *
len_builtin(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("len() requires one argument"));
        return NULL;
    }

    struct ga_obj *collection = args[0];
    struct ga_obj *res = GaObj_LEN(collection, vm);
    
    if (!res) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("len()"));
        return NULL;
    }

    return res;
}

static struct ga_obj *
map_builtin(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 2) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("map() requires two arguments"));
        return NULL;
    }
    
    struct ga_obj *func = args[0];
    struct ga_obj *collection = args[1];
    struct ga_obj *iter_obj = GaObj_ITER(collection, vm);

    if (!iter_obj) {
        return NULL;
    }

    GaObj_INC_REF(iter_obj);

    struct ga_obj *ret = NULL;
    struct list *listp = GaLinkedList_New();
    struct ga_obj *in_obj = NULL;
    struct ga_obj *out_obj = NULL;

    while (GaObj_ITER_NEXT(iter_obj, vm)) {
        in_obj = GaObj_INC_REF(GaObj_ITER_CUR(iter_obj, vm));

        if (!in_obj) {
            goto cleanup;
        }
        
        out_obj = GaObj_INVOKE(func, vm, 1, &in_obj);

        if (!out_obj) {
            goto cleanup;
        }

        GaObj_INC_REF(out_obj);
        GaObj_DEC_REF(in_obj);
        
        GaLinkedList_Push(listp, out_obj);
    }

    ret = GaTuple_New(LIST_COUNT(listp));

    int i = 0;
    list_iter_t iter;
    GaLinkedList_GetIter(listp, &iter);

    while (GaIter_Next(&iter, (void**)&out_obj)) {
        GaTuple_InitElem(ret, i++, GaObj_MOVE_REF(out_obj));
    }

cleanup:
    GaObj_DEC_REF(iter_obj);
    GaLinkedList_Destroy(listp, NULL, NULL);
    return ret;
}

static struct ga_obj *
open_builtin(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 2) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("open() requires two arguments"));
        return NULL;
    }

    struct ga_obj *file_str = GaObj_Super(args[0], &ga_str_type_inst);
    struct ga_obj *mode_str = GaObj_Super(args[1], &ga_str_type_inst);

    if (!file_str || !mode_str) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Str"));
        return NULL;
    }

    mode_t mode = 0;
    const char *file_cstring = GaStr_ToCString(file_str);
    const char *mode_cstring = GaStr_ToCString(mode_str);

    for (int i = 0; i < GaStr_Len(mode_str); i++) {
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

        GaEval_RaiseException(vm, GaErr_NewIOError(msg));

        return NULL;
    }

    return GaFile_New(fd, mode);
}

static struct ga_obj *
print_builtin(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    for (int i = 0; i < argc; i++) {
        struct ga_obj *obj = args[i];
        struct ga_obj *str = GaObj_INC_REF(GaObj_STR(obj, vm));

        fputs(GaStr_ToCString(str), stdout);
    
        GaObj_DEC_REF(str);
    }
    
    return &ga_null_inst;
}

static struct ga_obj *
puts_builtin(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    for (int i = 0; i < argc; i++) {
        struct ga_obj *obj = args[i];
        struct ga_obj *str = GaObj_INC_REF(GaObj_STR(obj, vm));

        fputs(GaStr_ToCString(str), stdout);

        GaObj_DEC_REF(str);
    }

    puts("");
    return &ga_null_inst;
}

static struct ga_obj *builtins_singleton = NULL;

__attribute__((constructor))
static void
init_builtin_singleton()
{
    builtins_singleton = GaObj_INC_REF(GaMod_OpenBuiltins());
}

__attribute__((destructor))
static void
fini_builtin_singleton()
{
    GaObj_DEC_REF(builtins_singleton);
}

struct ga_obj *
GaMod_OpenBuiltins()
{
    if (builtins_singleton) return builtins_singleton;
    
    builtins_singleton = GaModule_New("__builtins__", NULL, NULL);

    GaObj_SETATTR(builtins_singleton, NULL, "chr", GaBuiltin_New(chr_builtin, NULL));
    GaObj_SETATTR(builtins_singleton, NULL, "compile", GaBuiltin_New(compile_builtin, NULL));
    GaObj_SETATTR(builtins_singleton, NULL, "filter", GaBuiltin_New(filter_builtin, NULL));
    GaObj_SETATTR(builtins_singleton, NULL, "input", GaBuiltin_New(input_builtin, NULL));
    GaObj_SETATTR(builtins_singleton, NULL, "len", GaBuiltin_New(len_builtin, NULL));
    GaObj_SETATTR(builtins_singleton, NULL, "map", GaBuiltin_New(map_builtin, NULL));
    GaObj_SETATTR(builtins_singleton, NULL, "null", GA_NULL);
    GaObj_SETATTR(builtins_singleton, NULL, "open", GaBuiltin_New(open_builtin, NULL));
    GaObj_SETATTR(builtins_singleton, NULL, "print", GaBuiltin_New(print_builtin, NULL));
    GaObj_SETATTR(builtins_singleton, NULL, "puts", GaBuiltin_New(puts_builtin, NULL));
    GaObj_SETATTR(builtins_singleton, NULL, "super", GaBuiltin_New(super_builtin, NULL));
    GaObj_SETATTR(builtins_singleton, NULL, "Dict", &ga_dict_type_inst);
    GaObj_SETATTR(builtins_singleton, NULL, "Int", &ga_int_type_inst);
    GaObj_SETATTR(builtins_singleton, NULL, "List", &ga_list_type_inst);
    GaObj_SETATTR(builtins_singleton, NULL, "MutStr", GA_MUTSTR_TYPE);
    GaObj_SETATTR(builtins_singleton, NULL, "Object", &ga_obj_type_inst);
    GaObj_SETATTR(builtins_singleton, NULL, "Range", &ga_range_type_inst);
    GaObj_SETATTR(builtins_singleton, NULL, "Str", &ga_str_type_inst);
    GaObj_SETATTR(builtins_singleton, NULL, "Type", &ga_type_type_inst);
    GaObj_SETATTR(builtins_singleton, NULL, "WeakRef", &ga_weakref_type_inst);

    GaObj_SETATTR(builtins_singleton, NULL, "stdout", GaFile_New(1, O_WRONLY));
    GaObj_SETATTR(builtins_singleton, NULL, "Enumerable", GaEnumerable_New());

    /* Note: This is a HACK until I implement the use statement to import modules... */
    //GAOBJ_SETATTR(builtins_singleton, NULL, "ast", ga_ast_mod_open());
    //GAOBJ_SETATTR(builtins_singleton, NULL, "parser", ga_parser_mod_open());

    return builtins_singleton;
}