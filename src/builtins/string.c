/*
 * string.c - Gallium's builtin string type
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
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <gallium/builtins.h>
#include <gallium/object.h>
#include <gallium/stringbuf.h>
#include <gallium/vm.h>

static GaObject *   str_type_invoke(GaObject *, GaContext *, int, GaObject **);

GA_BUILTIN_TYPE_DECL(_GaStr_Type, "Str", str_type_invoke);

static GaObject *   str_add(GaObject *, GaContext *, GaObject *);
static bool         str_equals(GaObject *, GaContext *, GaObject *);
static GaObject *   str_getindex(GaObject *, GaContext *, GaObject *);
static int64_t      str_hash(GaObject *, GaContext *);
static GaObject *   str_len(GaObject *, GaContext *);
static GaObject *   str_str(GaObject *, GaContext *);

static struct ga_obj_ops str_ops = {
    .add        = str_add,
    .equals     = str_equals,
    .getindex   = str_getindex,
    .hash       = str_hash,
    .len        = str_len,
    .str        = str_str
};

static GaObject *
str_contains(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("contains() requires one argument"));
        return NULL;
    }

    GaObject *str1_obj = GaObj_Super(args[0], GA_STR_TYPE);

    if (!str1_obj) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Str"));
        return NULL;
    }

    size_t self_len = GaStr_Len(self);
    size_t str1_len = GaStr_Len(str1_obj);

    const char *self_str = GaStr_ToCString(self);
    const char *str1 = GaStr_ToCString(str1_obj);

    int pos = 0;

    while (pos < (self_len - str1_len)) {
        if (memcmp(&self_str[pos], str1, str1_len) == 0) {
            return Ga_TRUE;
        }
    }

    return Ga_FALSE;
}

static GaObject *
str_join(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("join() requires one argument"));
        return NULL;
    }

    GaObject *iter = GaObj_ITER(args[0], vm);

    if (!iter) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Iter"));
        return NULL;
    }

    GaObj_INC_REF(iter);

    int i = 0;
    struct stringbuf *self_sb = self->un.statep;
    struct stringbuf *sb = GaStringBuilder_New();

    while (GaObj_ITER_NEXT(iter, vm)) {
        GaObject *cur = GaObj_ITER_CUR(iter, vm);

        if (!cur) {
            GaEval_RaiseException(vm, GaErr_NewTypeError("Iter"));
            goto error;
        }
      
        GaObj_INC_REF(cur);

        GaObject *cur_str = GaObj_INC_REF(GaObj_STR(cur, vm));
        GaObject *cur_str_obj = GaObj_Super(cur_str, &_GaStr_Type);

        if (!cur_str) {
            GaEval_RaiseException(vm, GaErr_NewTypeError("Str"));
            goto error;
        }
        
        if (i > 0) {
            GaStringBuilder_Concat(sb, self_sb);
        }
        
        GaStringBuilder_Concat(sb, cur_str_obj->un.statep);
        GaObj_DEC_REF(cur_str);
        GaObj_DEC_REF(cur);
        i++;
    }

    GaObj_DEC_REF(iter);

    return GaStr_FromStringBuilder(sb);

error:
    GaObj_DEC_REF(iter);
    GaStringBuilder_Destroy(sb);
    return NULL;
}

static GaObject *
str_lower(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    struct stringbuf *sb = GaStringBuilder_Dup(self->un.statep);
    unsigned char *ptr = (unsigned char *)STRINGBUF_VALUE(sb);

    for (int i = 0; i < STRINGBUF_LEN(sb); i++) {
        ptr[i] = (unsigned char)tolower(ptr[i]);
    }

    return GaStr_FromStringBuilder(sb);
}

static GaObject *
str_replace(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    if (argc != 2) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("replace() requires two arguments"));
        return NULL;
    }

    GaObject *str1_obj = GaObj_Super(args[0], GA_STR_TYPE);
    GaObject *str2_obj = GaObj_Super(args[1], GA_STR_TYPE);

    if (!str1_obj || !str2_obj) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Str"));
        return NULL;
    }

    size_t self_len = GaStr_Len(self);
    size_t str1_len = GaStr_Len(str1_obj);
    size_t str2_len = GaStr_Len(str2_obj);

    const char *self_str = GaStr_ToCString(self);
    const char *str1 = GaStr_ToCString(str1_obj);
    const char *str2 = GaStr_ToCString(str2_obj);

    struct stringbuf *new_sb = GaStringBuilder_New();

    int substr_start = 0;

    int pos = 0;

    while (pos < (self_len - str1_len)) {
        if (memcmp(&self_str[pos], str1, str1_len) == 0) {
            
            if (substr_start != pos) {
                GaStringBuilder_AppendEx(new_sb, &self_str[substr_start], pos - substr_start);
            }

            GaStringBuilder_AppendEx(new_sb, str2, str2_len);
            pos += str1_len;
            substr_start = pos;
        } else {
            pos++;
        }
    }

    pos = self_len;

	if (substr_start != pos) {
		GaStringBuilder_AppendEx(new_sb, &self_str[substr_start], pos - substr_start);
	}

    return GaStr_FromStringBuilder(new_sb);
}

static GaObject *
str_split(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("split() requires one argument"));
        return NULL;
    }

    GaObject *str1_obj = GaObj_Super(args[0], GA_STR_TYPE);

    if (!str1_obj) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Str"));
        return NULL;
    }

    size_t self_len = GaStr_Len(self);
    size_t str1_len = GaStr_Len(str1_obj);

    const char *self_str = GaStr_ToCString(self);
    const char *str1 = GaStr_ToCString(str1_obj);

    GaObject *ret = GaList_New();

    int pos = 0;
    int substr_start = 0;

    while (self_len && pos < (self_len - str1_len)) {
        if (memcmp(&self_str[pos], str1, str1_len) == 0) {
            if (substr_start != pos) {
                GaList_Append(ret, GaStr_FromCStringEx(&self_str[substr_start], pos - substr_start));
            }
            
            pos += str1_len;
            substr_start = pos;
        } else {
            pos++;
        }
    }

	pos = self_len;

	if (substr_start != pos) {
		GaList_Append(ret, GaStr_FromCStringEx(&self_str[substr_start], pos - substr_start));
	}

    return ret;
}


static GaObject *
str_upper(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    struct stringbuf *sb = GaStringBuilder_Dup(self->un.statep);
    unsigned char *ptr = (unsigned char*)STRINGBUF_VALUE(sb);

    for (unsigned int i = 0; i < STRINGBUF_LEN(sb); i++) {
        ptr[i] = (unsigned char)toupper(ptr[i]);
    }

    return GaStr_FromStringBuilder(sb);
}

static GaObject *
str_type_invoke(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("Str() requires one argument"));
        return NULL;
    }

    return GaObj_STR(args[0], vm);
}

GaObject *
GaStr_FromStringBuilder(struct stringbuf *sb)
{
    GaObject *obj = GaObj_New(&_GaStr_Type, &str_ops);
    obj->un.statep = sb;

    GaObj_SETATTR(obj, NULL, "contains", GaBuiltin_New(str_contains, obj));
    GaObj_SETATTR(obj, NULL, "join", GaBuiltin_New(str_join, obj));
    GaObj_SETATTR(obj, NULL, "lower", GaBuiltin_New(str_lower, obj));
    GaObj_SETATTR(obj, NULL, "replace", GaBuiltin_New(str_replace, obj));
    GaObj_SETATTR(obj, NULL, "split", GaBuiltin_New(str_split, obj));
    GaObj_SETATTR(obj, NULL, "upper", GaBuiltin_New(str_upper, obj));
    
    return obj;
}

GaObject *
GaStr_FromCString(const char *val)
{
    struct stringbuf *sb = GaStringBuilder_New();
    GaStringBuilder_Append(sb, val);
    return GaStr_FromStringBuilder(sb);
}

GaObject *
GaStr_FromCStringEx(const char *val, size_t len)
{
    struct stringbuf *sb = GaStringBuilder_New();
    GaStringBuilder_AppendEx(sb, val, len);
    return GaStr_FromStringBuilder(sb);
}

size_t
GaStr_Len(GaObject *str)
{
    struct stringbuf *sb = str->un.statep;

    return STRINGBUF_LEN(sb);
}

const char *
GaStr_ToCString(GaObject *str)
{
    struct stringbuf *sb = str->un.statep;

    return STRINGBUF_VALUE(sb);
}

struct stringbuf *
GaStr_ToStringBuilder(GaObject *str)
{
    str = GaObj_Super(str, &_GaStr_Type);

    return str->un.statep;
}

static GaObject *
str_add(GaObject *self, GaContext *vm, GaObject *right)
{
    GaObject *right_str = GaObj_Super(right, &_GaStr_Type);

    if (!right_str) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Str"));
        return NULL;
    }

    struct stringbuf *sb = GaStringBuilder_New();
    struct stringbuf *left_sb = self->un.statep;
    struct stringbuf *right_sb = right_str->un.statep;

    GaStringBuilder_Append(sb, STRINGBUF_VALUE(left_sb));
    GaStringBuilder_Append(sb, STRINGBUF_VALUE(right_sb));

    return GaStr_FromStringBuilder(sb);
}

static bool
str_equals(GaObject *self, GaContext *vm, GaObject *right)
{
    GaObject *right_str = GaObj_Super(right, &_GaStr_Type);
    
    if (!right_str) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Str"));
        return NULL;
    }

    struct stringbuf *left_sb = self->un.statep;
    struct stringbuf *right_sb = right_str->un.statep;

    if (STRINGBUF_LEN(left_sb) != STRINGBUF_LEN(right_sb)) {
        return false;
    }

    return memcmp(STRINGBUF_VALUE(left_sb), STRINGBUF_VALUE(right_sb), STRINGBUF_LEN(left_sb)) == 0;
}

static GaObject *
str_getindex(GaObject *self, GaContext *vm, GaObject *key)
{
    GaObject *key_int = GaObj_Super(key, &_GaInt_Type);

    if (!key_int) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Int"));
        return NULL;
    }

    uint32_t index = (uint32_t)GaInt_TO_I64(key_int);
    const char *self_str = GaStr_ToCString(self);
    size_t self_len = GaStr_Len(self);

    if (index < self_len) {
        return GaStr_FromCStringEx(&self_str[index], 1);
    }

    GaEval_RaiseException(vm, GaErr_NewIndexError("Index out of range"));

    return NULL;
}

static int64_t
str_hash(GaObject *self, GaContext *vm)
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
str_len(GaObject *self, GaContext *vm)
{
    return GaInt_FROM_I64(GaStr_Len(self));
}

static GaObject *
str_str(GaObject *self, GaContext *vm)
{
    return self;
}