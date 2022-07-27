/*
 * mutable_string.c - Gallium's mutable string type ('MutStr')
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
#include <gallium/builtins.h>
#include <gallium/object.h>
#include <gallium/stringbuf.h>
#include <gallium/vm.h>

static struct ga_obj *ga_mutstr_type_invoke(struct ga_obj *, struct vm *, int, struct ga_obj **);

GA_BUILTIN_TYPE_DECL(ga_mutstr_type_inst, "MutStr", ga_mutstr_type_invoke);

static bool                 ga_mutstr_equals(struct ga_obj *, struct vm *, struct ga_obj *);
static int64_t              ga_mutstr_hash(struct ga_obj *, struct vm *);
static struct ga_obj    *   ga_mutstr_str(struct ga_obj *, struct vm *);

struct ga_obj_ops   mutstr_obj_ops = {
    .equals = ga_mutstr_equals,
    .hash   = ga_mutstr_hash,
    .str    = ga_mutstr_str
};

static struct ga_obj *
ga_mutstr_append_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, ga_argument_error_new("append() requires one argument"));
        return NULL;
    }

    struct ga_obj *str_obj = GaObj_Super(args[0], GA_STR_TYPE);

    if (!str_obj) {
        GaEval_RaiseException(vm, ga_type_error_new("Str"));
        return NULL;
    }

    const char *str = ga_str_to_cstring(str_obj);
    size_t len = ga_str_len(str_obj);

    struct stringbuf *sb = self->un.statep;

    GaStringBuilder_AppendEx(sb, str, len);

    return GA_NULL;
}

static struct ga_obj *
ga_mutstr_type_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc > 1) {
        GaEval_RaiseException(vm, ga_argument_error_new("MutStr() accepts one optional argument"));
        return NULL;
    }

    struct ga_obj *obj = GaObj_New(&ga_mutstr_type_inst, &mutstr_obj_ops);

    obj->un.statep = GaStringBuilder_New();

    GaObj_SETATTR(obj, NULL, "append", ga_builtin_new(ga_mutstr_append_method, obj));

    return obj;
}

static bool
ga_mutstr_equals(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    struct ga_obj *right_str = GaObj_Super(right, &ga_mutstr_type_inst);

    if (!right_str) {
        GaEval_RaiseException(vm, ga_type_error_new("MutStr"));
        return NULL;
    }

    struct stringbuf *left_sb = self->un.statep;
    struct stringbuf *right_sb = right_str->un.statep;

    if (STRINGBUF_LEN(left_sb) != STRINGBUF_LEN(right_sb)) {
        return false;
    }

    return memcmp(STRINGBUF_VALUE(left_sb), STRINGBUF_VALUE(right_sb), STRINGBUF_LEN(left_sb)) == 0;
}

static int64_t
ga_mutstr_hash(struct ga_obj *self, struct vm *vm)
{
    struct stringbuf *sb = self->un.statep;
    const char *str = STRINGBUF_VALUE(sb);

    uint32_t res = 0;

    for (int i = 0; i < STRINGBUF_LEN(sb); i++) {
        res = str[i] + 31 * res;
    }

    return res;
}

static struct ga_obj *
ga_mutstr_str(struct ga_obj *self, struct vm *vm)
{
    struct stringbuf *sb = GaStringBuilder_Dup(self->un.statep);
    return ga_str_from_stringbuf(sb);
}
