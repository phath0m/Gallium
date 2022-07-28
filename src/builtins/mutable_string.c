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

static GaObject *   mutstr_type_invoke(GaObject *, GaContext *, int, GaObject **);

GA_BUILTIN_TYPE_DECL(_GaMutStr_Type, "MutStr", mutstr_type_invoke);

static bool         mutstr_equals(GaObject *, GaContext *, GaObject *);
static int64_t      mutstr_hash(GaObject *, GaContext *);
static GaObject *   mutstr_str(GaObject *, GaContext *);

static struct ga_obj_ops mutstr_ops = {
    .equals = mutstr_equals,
    .hash   = mutstr_hash,
    .str    = mutstr_str
};

static GaObject *
mutstr_append(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("append() requires one argument"));
        return NULL;
    }

    GaObject *str_obj = GaObj_Super(args[0], GA_STR_TYPE);

    if (!str_obj) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Str"));
        return NULL;
    }

    const char *str = GaStr_ToCString(str_obj);
    size_t len = GaStr_Len(str_obj);

    struct stringbuf *sb = self->un.statep;

    GaStringBuilder_AppendEx(sb, str, len);

    return Ga_NULL;
}

static GaObject *
mutstr_type_invoke(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    if (argc > 1) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("MutStr() accepts one optional argument"));
        return NULL;
    }

    GaObject *obj = GaObj_New(&_GaMutStr_Type, &mutstr_ops);

    obj->un.statep = GaStringBuilder_New();

    GaObj_SETATTR(obj, NULL, "append", GaBuiltin_New(mutstr_append, obj));

    return obj;
}

static bool
mutstr_equals(GaObject *self, GaContext *vm, GaObject *right)
{
    GaObject *right_str = GaObj_Super(right, &_GaMutStr_Type);

    if (!right_str) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("MutStr"));
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
mutstr_hash(GaObject *self, GaContext *vm)
{
    struct stringbuf *sb = self->un.statep;
    const char *str = STRINGBUF_VALUE(sb);

    uint32_t res = 0;

    for (int i = 0; i < STRINGBUF_LEN(sb); i++) {
        res = str[i] + 31 * res;
    }

    return res;
}

static GaObject *
mutstr_str(GaObject *self, GaContext *vm)
{
    struct stringbuf *sb = GaStringBuilder_Dup(self->un.statep);
    return GaStr_FromStringBuilder(sb);
}