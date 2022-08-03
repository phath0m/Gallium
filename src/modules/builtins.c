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
#include <gallium/list.h>
#include <gallium/object.h>
#include <gallium/vm.h>
#include "../compiler.h"

ssize_t getline(char **, size_t *, FILE *);

static GaObject *
super_builtin(GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARG_COUNT_MIN(vm, 1, argc)) {
        return NULL;
    }

    GaObject *obj = args[0];
    GaObject *clazz = obj->type;

    if (!Ga_ENSURE_TYPE(vm, clazz, GA_CLASS_TYPE)) {
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
    if (!Ga_CHECK_ARGS_EXACT(vm, 1, (GaObject *[]){ GA_INT_TYPE }, argc,
         args))
    {
        return NULL;
    }

    GaObject *int_obj = GaObj_Super(args[0], &_GaInt_Type);

    char str[] = {
        (uint8_t)(GaInt_TO_I64(int_obj)),
        0
    };

    return GaStr_FromCString(str);
}

static GaObject *
input_builtin(GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_OPTIONAL(vm, 0, (GaObject *[]){ GA_STR_TYPE, NULL },
         argc, args))
    {
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
    if (!Ga_CHECK_ARG_COUNT_EXACT(vm, 1, argc)) {
        return NULL;
    }

    GaObject *collection = args[0];
    GaObject *res = GaObj_LEN(collection, vm);
    
    if (!res) {
        return NULL;
    }

    return res;
}

static GaObject *
open_builtin(GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_EXACT(vm, 2, (GaObject *[]){ GA_STR_TYPE, GA_STR_TYPE },
         argc, args))
    {
        return NULL;
    }

    GaObject *file_str = GaObj_Super(args[0], GA_STR_TYPE);
    GaObject *mode_str = GaObj_Super(args[1], GA_STR_TYPE);

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

    int fd = open(file_cstring, mode, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP |
                                      S_IROTH | S_IWOTH);

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
    if (!Ga_CHECK_ARG_COUNT_EXACT(vm, 1, argc)) {
        return NULL;
    }

    return GaInt_FROM_I64((int64_t)args[0]);
}

GaObject *
GaMod_OpenBuiltins()
{
    GaObject *mod = GaModule_New("__builtins__", NULL, NULL);

    GaObj_SETATTR(mod, NULL, "chr", GaBuiltin_New(chr_builtin, NULL));
    GaObj_SETATTR(mod, NULL, "input", GaBuiltin_New(input_builtin, NULL));
    GaObj_SETATTR(mod, NULL, "len", GaBuiltin_New(len_builtin, NULL));
    GaObj_SETATTR(mod, NULL, "null", Ga_NULL);
    GaObj_SETATTR(mod, NULL, "open", GaBuiltin_New(open_builtin, NULL));
    GaObj_SETATTR(mod, NULL, "print", GaBuiltin_New(print_builtin, NULL));
    GaObj_SETATTR(mod, NULL, "puts", GaBuiltin_New(puts_builtin, NULL));
    GaObj_SETATTR(mod, NULL, "super", GaBuiltin_New(super_builtin, NULL));
    GaObj_SETATTR(mod, NULL, "id", GaBuiltin_New(id_builtin, NULL));
    GaObj_SETATTR(mod, NULL, "Dict", &_GaDict_Type);
    GaObj_SETATTR(mod, NULL, "Int", &_GaInt_Type);
    GaObj_SETATTR(mod, NULL, "List", GA_LIST_TYPE);
    GaObj_SETATTR(mod, NULL, "MutStr", GA_MUTSTR_TYPE);
    GaObj_SETATTR(mod, NULL, "Object", &_GaObj_Type);
    GaObj_SETATTR(mod, NULL, "Range", &_GaRange_Type);
    GaObj_SETATTR(mod, NULL, "Str", GA_STR_TYPE);
    GaObj_SETATTR(mod, NULL, "Type", &ga_type_type_inst);
    GaObj_SETATTR(mod, NULL, "WeakRef", &_GaWeakRef_Type);

    GaObj_SETATTR(mod, NULL, "stdout", GaFile_New(1, O_WRONLY));
    GaObj_SETATTR(mod, NULL, "Enumerable", GaEnumerable_New());

    return mod;
}