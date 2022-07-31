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

static GaObject *
super_builtin(GaContext *vm, int argc, GaObject **args)
{
    if (argc < 1) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("super() requires at least one argument"));
        return NULL;
    }

    GaObject *obj = args[0];
    GaObject *clazz = obj->type;

    if (clazz->type != &_GaClass_Type) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Object"));
        return NULL;
    }
   
    GaObject *base = GaClass_Base(clazz);
    GaObject *super_inst = GaObj_INVOKE(base, vm, argc - 1, &args[1]);

    obj->super = GaObj_INC_REF(super_inst);

    GaObj_SETATTR(obj, vm, "super", super_inst);

    return &_GaNull;
}

static GaObject *
chr_builtin(GaContext *vm, int argc, GaObject **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("chr() requires one argument"));
        return NULL;
    }

    GaObject *int_obj = GaObj_Super(args[0], &_GaInt_Type);

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


static GaObject *
compile_builtin(GaContext *vm, int argc, GaObject **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("compile() requires one argument"));
        return NULL;
    }

    GaObject *ast = GaObj_Super(args[0], &_GaAstNode_Type);

    if (!ast) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("ast.AstNode"));
        return NULL;
    }

    struct compiler_state compiler;
   
    memset(&compiler, 0, sizeof(compiler));

    GaObject *ret = GaAst_Compile(&compiler, ast->un.statep);

    return ret;
}

static GaObject *
filter_builtin(GaContext *vm, int argc, GaObject **args)
{
    if (argc != 2) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("map() requires two arguments"));
        return NULL;
    }

    GaObject *func = args[0];
    GaObject *collection = args[1];
    GaObject *iter_obj = GaObj_ITER(collection, vm);

    if (!iter_obj) {
        return NULL;
    }

    GaObj_INC_REF(iter_obj);

    GaObject *ret = NULL;
    _Ga_list_t *listp = _Ga_list_new();
    GaObject *in_obj = NULL;

    while (GaObj_ITER_NEXT(iter_obj, vm)) {
        in_obj = GaObj_INC_REF(GaObj_ITER_CUR(iter_obj, vm));

        if (!in_obj) {
            goto cleanup;
        }

        if (GaObj_IS_TRUE(GaObj_INVOKE(func, vm, 1, &in_obj), vm)) {
            _Ga_list_push(listp, GaObj_MOVE_REF(in_obj));
        } else {
            GaObj_DEC_REF(in_obj);
        }
    }

    ret = GaTuple_New(_Ga_LIST_COUNT(listp));

    int i = 0;
    _Ga_iter_t iter;
    _Ga_list_get_iter(listp, &iter);

    while (_Ga_iter_next(&iter, (void**)&in_obj)) {
        GaTuple_InitElem(ret, i++, in_obj);
    }

cleanup:
    GaObj_DEC_REF(iter_obj);
    _Ga_list_destroy(listp, NULL, NULL);
    return ret;
}

static GaObject *
input_builtin(GaContext *vm, int argc, GaObject **args)
{
    if (argc > 1) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("input() accepts one optional argument"));
        return NULL;
    }

    if (argc == 1) {
        GaObject *prompt_str = GaObj_INC_REF(GaObj_STR(args[0], vm));
        fputs(GaStr_ToCString(prompt_str), stdout);
        fflush(stdout);
        GaObj_DEC_REF(prompt_str);
    }
    
    GaObject *ret = NULL;
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

static GaObject *
len_builtin(GaContext *vm, int argc, GaObject **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("len() requires one argument"));
        return NULL;
    }

    GaObject *collection = args[0];
    GaObject *res = GaObj_LEN(collection, vm);
    
    if (!res) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("len()"));
        return NULL;
    }

    return res;
}

static GaObject *
map_builtin(GaContext *vm, int argc, GaObject **args)
{
    if (argc != 2) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("map() requires two arguments"));
        return NULL;
    }
    
    GaObject *func = args[0];
    GaObject *collection = args[1];
    GaObject *iter_obj = GaObj_ITER(collection, vm);

    if (!iter_obj) {
        return NULL;
    }

    GaObj_INC_REF(iter_obj);

    GaObject *ret = NULL;
    _Ga_list_t *listp = _Ga_list_new();
    GaObject *in_obj = NULL;
    GaObject *out_obj = NULL;

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
        
        _Ga_list_push(listp, out_obj);
    }

    ret = GaTuple_New(_Ga_LIST_COUNT(listp));

    int i = 0;
    _Ga_iter_t iter;
    _Ga_list_get_iter(listp, &iter);

    while (_Ga_iter_next(&iter, (void**)&out_obj)) {
        GaTuple_InitElem(ret, i++, GaObj_MOVE_REF(out_obj));
    }

cleanup:
    GaObj_DEC_REF(iter_obj);
    _Ga_list_destroy(listp, NULL, NULL);
    return ret;
}

static GaObject *
open_builtin(GaContext *vm, int argc, GaObject **args)
{
    if (argc != 2) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("open() requires two arguments"));
        return NULL;
    }

    GaObject *file_str = GaObj_Super(args[0], &_GaStr_Type);
    GaObject *mode_str = GaObj_Super(args[1], &_GaStr_Type);

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

static GaObject *
print_builtin(GaContext *vm, int argc, GaObject **args)
{
    for (int i = 0; i < argc; i++) {
        GaObject *obj = args[i];
        GaObject *str = GaObj_INC_REF(GaObj_STR(obj, vm));

        fputs(GaStr_ToCString(str), stdout);
    
        GaObj_DEC_REF(str);
    }
    
    return &_GaNull;
}

static GaObject *
puts_builtin(GaContext *vm, int argc, GaObject **args)
{
    for (int i = 0; i < argc; i++) {
        GaObject *obj = args[i];
        GaObject *str = GaObj_INC_REF(GaObj_STR(obj, vm));

        fputs(GaStr_ToCString(str), stdout);

        GaObj_DEC_REF(str);
    }

    puts("");
    return &_GaNull;
}

static GaObject *
id_builtin(GaContext *vm, int argc, GaObject **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("id() requires one argument"));
        return NULL;
    }

    return GaInt_FROM_I64((int64_t)args[0]);
}

static GaObject *builtins_singleton = NULL;

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

GaObject *
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
    GaObj_SETATTR(builtins_singleton, NULL, "null", Ga_NULL);
    GaObj_SETATTR(builtins_singleton, NULL, "open", GaBuiltin_New(open_builtin, NULL));
    GaObj_SETATTR(builtins_singleton, NULL, "print", GaBuiltin_New(print_builtin, NULL));
    GaObj_SETATTR(builtins_singleton, NULL, "puts", GaBuiltin_New(puts_builtin, NULL));
    GaObj_SETATTR(builtins_singleton, NULL, "super", GaBuiltin_New(super_builtin, NULL));
    GaObj_SETATTR(builtins_singleton, NULL, "id", GaBuiltin_New(id_builtin, NULL));
    GaObj_SETATTR(builtins_singleton, NULL, "Dict", &_GaDict_Type);
    GaObj_SETATTR(builtins_singleton, NULL, "Int", &_GaInt_Type);
    GaObj_SETATTR(builtins_singleton, NULL, "List", &_GaList_Type);
    GaObj_SETATTR(builtins_singleton, NULL, "MutStr", GA_MUTSTR_TYPE);
    GaObj_SETATTR(builtins_singleton, NULL, "Object", &_GaObj_Type);
    GaObj_SETATTR(builtins_singleton, NULL, "Range", &_GaRange_Type);
    GaObj_SETATTR(builtins_singleton, NULL, "Str", &_GaStr_Type);
    GaObj_SETATTR(builtins_singleton, NULL, "Type", &ga_type_type_inst);
    GaObj_SETATTR(builtins_singleton, NULL, "WeakRef", &_GaWeakRef_Type);

    GaObj_SETATTR(builtins_singleton, NULL, "stdout", GaFile_New(1, O_WRONLY));
    GaObj_SETATTR(builtins_singleton, NULL, "Enumerable", GaEnumerable_New());

    return builtins_singleton;
}