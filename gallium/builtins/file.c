/*
 * file.c - Builtin file type
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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gallium/builtins.h>
#include <gallium/object.h>
#include <gallium/stringbuf.h>
#include <gallium/vm.h>

GA_BUILTIN_TYPE_DECL(ga_file_type_inst, "File", NULL);

static void file_destroy(GaObject *);

static struct ga_obj_ops file_ops = {
    .destroy    =   file_destroy
};

struct file_state {
    bool        closed;
    int         fd;
    mode_t      mode;
};

static GaObject *
file_close(GaObject *self, struct vm *vm, int argc, GaObject **args)
{
    struct file_state *statep = self->un.statep;

    if (!statep->closed) {
        close(statep->fd);
        statep->closed = true;
    }

    return &ga_null_inst;
}

static GaObject *
file_read(GaObject *self, struct vm *vm, int argc, GaObject **args)
{
    if (argc > 1) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("read() accepts one optional argument"));
        return NULL;
    }
   
    ssize_t nbyte = -1;
    ssize_t nread = 0;

    if (argc == 1) {
        GaObject *nbyte_obj = GaObj_Super(args[0], &ga_int_type_inst);

        if (!nbyte_obj) {
            GaEval_RaiseException(vm, GaErr_NewTypeError("Int"));
            return NULL;
        }
        
        nbyte = (ssize_t)GaInt_TO_I64(nbyte_obj);
    }

    struct file_state *statep = self->un.statep;

    if (statep->closed) {
        GaEval_RaiseException(vm, GaErr_NewIOError("The file has been closed"));
        return NULL;
    }

    if (nbyte != -1) {
        char *buf = malloc(nbyte);

        nread = read(statep->fd, buf, nbyte);

        if (nread > 0) {
            struct stringbuf *sb = GaStringBuilder_FromCString(buf, nread);
            return GaStr_FromStringBuilder(sb);
        }

        GaEval_RaiseException(vm, GaErr_NewIOError("Something happened. Check errno you dummy"));
        free(buf);

        return NULL;
    }

    char buf[4096];
    struct stringbuf *contents = GaStringBuilder_New();

    for (;;) {
        ssize_t res = read(statep->fd, buf, 4096);
        
        if (res == 0) {
            break;
        } else if (res < 0) {
            GaStringBuilder_Destroy(contents);
            GaEval_RaiseException(vm, GaErr_NewIOError("I/O error"));
            return NULL;
        }

        GaStringBuilder_AppendEx(contents, buf, res);

        nread += res;
    }

    return GaStr_FromStringBuilder(contents);
}

static GaObject *
file_seek(GaObject *self, struct vm *vm, int argc, GaObject **args)
{
    if (argc != 2) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("seek() requires two arguments"));
        return NULL;
    }

    GaObject *offset_obj = GaObj_Super(args[0], &ga_int_type_inst);
    GaObject *whence_obj = GaObj_Super(args[1], &ga_int_type_inst);

    if (!offset_obj || !whence_obj) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Int"));
        return NULL;
    }

    struct file_state *statep = self->un.statep;

    if (statep->closed) {
        GaEval_RaiseException(vm, GaErr_NewIOError("The file has been closed"));
        return NULL;
    }

    off_t res = 0;
    off_t offset = (off_t)GaInt_TO_I64(offset_obj);
    int whence = (int)GaInt_TO_I64(whence_obj);

    switch (whence) {
        case 0:
            res = lseek(statep->fd, offset, SEEK_SET);
            break;
        case 1:
            res = lseek(statep->fd, offset, SEEK_CUR);
            break;
        case 2:
            res = lseek(statep->fd, offset, SEEK_END);
            break;
        default:
            break;
    }

    return GaInt_FROM_I64((int64_t)res);
}

static GaObject *
file_tell(GaObject *self, struct vm *vm, int argc, GaObject **args)
{
    if (argc != 0) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("tell() requires 0 arguments"));
        return NULL;
    }

    struct file_state *statep = self->un.statep;

    if (statep->closed) {
        GaEval_RaiseException(vm, GaErr_NewIOError("The file has been closed"));
        return NULL;
    }

    off_t pos = lseek(statep->fd, 0, SEEK_CUR);

    return GaInt_FROM_I64(pos);
}

static GaObject *
file_write(GaObject *self, struct vm *vm, int argc, GaObject **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("write() expects one argument"));
        return NULL;
    }

    GaObject *arg_str = GaObj_Super(args[0], &ga_str_type_inst);

    if (!arg_str) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Str"));
        return NULL;
    }

    struct file_state *statep = self->un.statep;
    
    if (statep->closed) {
        GaEval_RaiseException(vm, GaErr_NewIOError("The file has been closed"));
        return NULL;
    }

    const char *buf = GaStr_ToCString(arg_str);
    size_t len = GaStr_Len(arg_str); 
    size_t written = 0;

    while (written < len) {
        ssize_t res = write(statep->fd, buf, len - written);
        
        if (res > 0) {
            written += res;
        } else {
            // TODO: check errno
            GaEval_RaiseException(vm, GaErr_NewIOError("an I/O error occured")); 
            return NULL;
            /* I/O error or something... */
        }
    }

    return &ga_null_inst;
}

static void
file_destroy(GaObject *obj)
{
    GaObject *file_obj = GaObj_Super(obj, &ga_file_type_inst);

    if (!file_obj) return;

    struct file_state *statep = obj->un.statep;

    /* HACK: Do not close standard I/O file descriptors. */
    if (!statep->closed && statep->fd > 2) {
        close(statep->fd);
    }

    free(statep);
}

GaObject *
GaFile_New(int fd, mode_t mode)
{
    GaObject *obj = GaObj_New(&ga_file_type_inst, &file_ops);
    struct file_state *statep = calloc(sizeof(struct file_state), 1);

    statep->fd = fd;
    statep->mode = mode;
    obj->un.statep = statep;

    GaObj_SETATTR(obj, NULL, "close", GaBuiltin_New(file_close, obj));
    GaObj_SETATTR(obj, NULL, "read", GaBuiltin_New(file_read, obj));
    GaObj_SETATTR(obj, NULL, "seek", GaBuiltin_New(file_seek, obj));
    GaObj_SETATTR(obj, NULL, "tell", GaBuiltin_New(file_tell, obj));
    GaObj_SETATTR(obj, NULL, "write", GaBuiltin_New(file_write, obj));

    return obj;
}