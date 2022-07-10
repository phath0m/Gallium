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

static void ga_file_destroy(struct ga_obj *);

struct ga_obj_ops file_ops = {
    .destroy    =   ga_file_destroy
};

struct file_state {
    bool        closed;
    int         fd;
    mode_t      mode;
};

static struct ga_obj *
ga_file_close_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    struct file_state *statep = self->un.statep;

    if (!statep->closed) {
        close(statep->fd);
        statep->closed = true;
    }

    return &ga_null_inst;
}

static struct ga_obj *
ga_file_read_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc > 1) {
        vm_raise_exception(vm, ga_argument_error_new("read() accepts one optional argument"));
        return NULL;
    }
   
    ssize_t nbyte = -1;
    ssize_t nread = 0;

    if (argc == 1) {
        struct ga_obj *nbyte_obj = ga_obj_super(args[0], &ga_int_type_inst);

        if (!nbyte_obj) {
            vm_raise_exception(vm, ga_type_error_new("Int"));
            return NULL;
        }
        
        nbyte = (ssize_t)ga_int_to_i64(nbyte_obj);
    }

    struct file_state *statep = self->un.statep;

    if (statep->closed) {
        vm_raise_exception(vm, ga_io_error_new("The file has been closed"));
        return NULL;
    }

    if (nbyte != -1) {
        char *buf = malloc(nbyte);

        nread = read(statep->fd, buf, nbyte);

        if (nread > 0) {
            struct stringbuf *sb = stringbuf_wrap_buf(buf, nread);
            return ga_str_from_stringbuf(sb);
        }

        vm_raise_exception(vm, ga_io_error_new("Something happened. Check errno you dummy"));
        free(buf);

        return NULL;
    }

    char buf[4096];
    struct stringbuf *contents = stringbuf_new();

    for (;;) {
        ssize_t res = read(statep->fd, buf, 4096);
        
        if (res == 0) {
            break;
        } else if (res < 0) {
            stringbuf_destroy(contents);
            vm_raise_exception(vm, ga_io_error_new("I/O error"));
            return NULL;
        }

        stringbuf_append_range(contents, buf, res);

        nread += res;
    }

    return ga_str_from_stringbuf(contents);
}

static struct ga_obj *
ga_file_seek_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 2) {
        vm_raise_exception(vm, ga_argument_error_new("seek() requires two arguments"));
        return NULL;
    }

    struct ga_obj *offset_obj = ga_obj_super(args[0], &ga_int_type_inst);
    struct ga_obj *whence_obj = ga_obj_super(args[1], &ga_int_type_inst);

    if (!offset_obj || !whence_obj) {
        vm_raise_exception(vm, ga_type_error_new("Int"));
        return NULL;
    }

    struct file_state *statep = self->un.statep;

    if (statep->closed) {
        vm_raise_exception(vm, ga_io_error_new("The file has been closed"));
        return NULL;
    }

    off_t res = 0;
    off_t offset = (off_t)ga_int_to_i64(offset_obj);
    int whence = (int)ga_int_to_i64(whence_obj);

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

    return ga_int_from_i64((int64_t)res);
}

static struct ga_obj *
ga_file_tell_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 0) {
        vm_raise_exception(vm, ga_argument_error_new("tell() requires 0 arguments"));
        return NULL;
    }

    struct file_state *statep = self->un.statep;

    if (statep->closed) {
        vm_raise_exception(vm, ga_io_error_new("The file has been closed"));
        return NULL;
    }

    off_t pos = lseek(statep->fd, 0, SEEK_CUR);

    return ga_int_from_i64(pos);
}

static struct ga_obj *
ga_file_write_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        vm_raise_exception(vm, ga_argument_error_new("write() expects one argument"));
        return NULL;
    }

    struct ga_obj *arg_str = ga_obj_super(args[0], &ga_str_type_inst);

    if (!arg_str) {
        vm_raise_exception(vm, ga_type_error_new("Str"));
        return NULL;
    }

    struct file_state *statep = self->un.statep;
    
    if (statep->closed) {
        vm_raise_exception(vm, ga_io_error_new("The file has been closed"));
        return NULL;
    }

    const char *buf = ga_str_to_cstring(arg_str);
    size_t len = ga_str_len(arg_str); 
    size_t written = 0;

    while (written < len) {
        ssize_t res = write(statep->fd, buf, len - written);
        
        if (res > 0) {
            written += res;
        } else {
            // TODO: check errno
            vm_raise_exception(vm, ga_io_error_new("an I/O error occured")); 
            return NULL;
            /* I/O error or something... */
        }
    }

    return &ga_null_inst;
}

static void
ga_file_destroy(struct ga_obj *obj)
{
    struct ga_obj *file_obj = ga_obj_super(obj, &ga_file_type_inst);

    if (!file_obj) return;

    struct file_state *statep = obj->un.statep;

    if (!statep->closed) {
        close(statep->fd);
    }

    free(statep);
}

struct ga_obj *
ga_file_new(int fd, mode_t mode)
{
    struct ga_obj *obj = ga_obj_new(&ga_file_type_inst, &file_ops);
    struct file_state *statep = calloc(sizeof(struct file_state), 1);

    statep->fd = fd;
    statep->mode = mode;
    obj->un.statep = statep;

    GAOBJ_SETATTR(obj, NULL, "close", ga_builtin_new(ga_file_close_method, obj));
    GAOBJ_SETATTR(obj, NULL, "read", ga_builtin_new(ga_file_read_method, obj));
    GAOBJ_SETATTR(obj, NULL, "seek", ga_builtin_new(ga_file_seek_method, obj));
    GAOBJ_SETATTR(obj, NULL, "tell", ga_builtin_new(ga_file_tell_method, obj));
    GAOBJ_SETATTR(obj, NULL, "write", ga_builtin_new(ga_file_write_method, obj));

    return obj;
}