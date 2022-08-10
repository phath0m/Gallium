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
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <gallium/builtins.h>
#include <gallium/object.h>
#include <gallium/stringbuf.h>
#include <gallium/vm.h>

static GaObject *   str_type_invoke(GaObject *, GaContext *, int, GaObject **);

GaObject        *   _GaStr_type = NULL;

//GA_BUILTIN_TYPE_DECL(_GaStr_Type, "Str", str_type_invoke);

static GaObject *   str_add(GaObject *, GaContext *, GaObject *);
static bool         str_equals(GaObject *, GaContext *, GaObject *);
static GaObject *   str_getindex(GaObject *, GaContext *, GaObject *);
static int64_t      str_hash(GaObject *, GaContext *);
static GaObject *   str_len(GaObject *, GaContext *);
static GaObject *   str_str(GaObject *, GaContext *);

static struct Ga_Operators str_ops = {
    .add        = str_add,
    .equals     = str_equals,
    .getindex   = str_getindex,
    .hash       = str_hash,
    .len        = str_len,
    .str        = str_str
};

static GaObject *
str_contains(GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_EXACT(vm, 2, (GaObject*[]){ GA_STR_TYPE, GA_STR_TYPE},
                             argc, args))
    {
        return NULL;
    }

    GaObject *self = GaObj_Super(args[0], GA_STR_TYPE);
    GaObject *str1_obj = GaObj_Super(args[1], GA_STR_TYPE);

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
str_join(GaContext *vm, int argc, GaObject **args)
{
    if (argc != 2) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("Expected 2 arguments, but got %d", argc));
        return NULL;
    }

    GaObject *self = Ga_ENSURE_TYPE(vm, args[0], GA_STR_TYPE);

    if (!self) return NULL;

    GaObject *iter = Ga_ENSURE_HAS_ITER(vm, args[1]);

    if (!iter) return NULL;

    GaObj_INC_REF(iter);

    int i = 0;
    struct stringbuf *self_sb = self->un.statep;
    struct stringbuf *sb = GaStringBuilder_New();

    while (GaObj_ITER_NEXT(iter, vm)) {
        GaObject *cur = Ga_ENSURE_HAS_CUR(vm, iter);

        if (!cur) goto error;
      
        GaObj_INC_REF(cur);

        GaObject *cur_str = GaObj_INC_REF(GaObj_STR(cur, vm));
        GaObject *cur_str_obj = Ga_ENSURE_TYPE(vm, cur_str, GA_STR_TYPE);

        if (!cur_str) goto error;
        
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
str_lower(GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_EXACT(vm, 1, (GaObject*[]){ GA_STR_TYPE }, argc,
                             args))
    {
        return NULL;
    }

    GaObject *self = GaObj_Super(args[0], GA_STR_TYPE);
    struct stringbuf *sb = GaStringBuilder_Dup(self->un.statep);
    char *ptr = (char *)STRINGBUF_VALUE(sb);

    for (int i = 0; i < STRINGBUF_LEN(sb); i++) {
        ptr[i] = (char)tolower(ptr[i]);
    }

    return GaStr_FromStringBuilder(sb);
}

static GaObject *
str_replace(GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_EXACT(vm, 3, (GaObject*[]){ GA_STR_TYPE, GA_STR_TYPE,
                             GA_STR_TYPE }, argc, args))
    {
        return NULL;
    }

    GaObject *self = GaObj_Super(args[0], GA_STR_TYPE);
    GaObject *str1_obj = GaObj_Super(args[1], GA_STR_TYPE);
    GaObject *str2_obj = GaObj_Super(args[2], GA_STR_TYPE);

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
str_split(GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_EXACT(vm, 2, (GaObject*[]){ GA_STR_TYPE, GA_STR_TYPE},
                             argc, args))
    {
        return NULL;
    }

    GaObject *self = GaObj_Super(args[0], GA_STR_TYPE);
    GaObject *str1_obj = GaObj_Super(args[1], GA_STR_TYPE);

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
str_upper(GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_EXACT(vm, 1, (GaObject*[]){ GA_STR_TYPE }, argc,
                             args))
    {
        return NULL;
    }

    GaObject *self = GaObj_Super(args[0], GA_STR_TYPE);

    struct stringbuf *sb = GaStringBuilder_Dup(self->un.statep);
    char *ptr = STRINGBUF_VALUE(sb);

    for (int i = 0; i < STRINGBUF_LEN(sb); i++) {
        ptr[i] = (char)toupper(ptr[i]);
    }

    return GaStr_FromStringBuilder(sb);
}

static GaObject *
str_isdigit(GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_EXACT(vm, 1, (GaObject*[]){ GA_STR_TYPE }, argc,
                             args))
    {
        return NULL;
    }

    GaObject *self = GaObj_Super(args[0], GA_STR_TYPE);

    struct stringbuf *sb = self->un.statep;
    char *ptr = STRINGBUF_VALUE(sb);
    bool ret = STRINGBUF_LEN(sb) > 0;

    for (int i = 0; i < STRINGBUF_LEN(sb); i++) {
        if (!isdigit(ptr[i])) {
            ret = false;
            break;
        }
    }

    return GaBool_FROM_BOOL(ret);
}

static GaObject *
str_isspace(GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_EXACT(vm, 1, (GaObject*[]){ GA_STR_TYPE }, argc,
                             args))
    {
        return NULL;
    }

    GaObject *self = GaObj_Super(args[0], GA_STR_TYPE);

    struct stringbuf *sb = self->un.statep;
    char *ptr = STRINGBUF_VALUE(sb);
    bool ret = STRINGBUF_LEN(sb) > 0;

    for (int i = 0; i < STRINGBUF_LEN(sb); i++) {
        if (!isspace(ptr[i])) {
            ret = false;
            break;
        }
    }

    return GaBool_FROM_BOOL(ret);
}

static GaObject *
str_isalpha(GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_EXACT(vm, 1, (GaObject*[]){ GA_STR_TYPE }, argc,
                             args))
    {
        return NULL;
    }

    GaObject *self = GaObj_Super(args[0], GA_STR_TYPE);

    struct stringbuf *sb = self->un.statep;
    char *ptr = STRINGBUF_VALUE(sb);
    bool ret = STRINGBUF_LEN(sb) > 0;

    for (int i = 0; i < STRINGBUF_LEN(sb); i++) {
        if (!isalpha(ptr[i])) {
            ret = false;
            break;
        }
    }

    return GaBool_FROM_BOOL(ret);
}

static GaObject *
str_isalnum(GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARGS_EXACT(vm, 1, (GaObject*[]){ GA_STR_TYPE }, argc,
                             args))
    {
        return NULL;
    }

    GaObject *self = GaObj_Super(args[0], GA_STR_TYPE);
    struct stringbuf *sb = self->un.statep;
    char *ptr = STRINGBUF_VALUE(sb);
    bool ret = STRINGBUF_LEN(sb) > 0;

    for (int i = 0; i < STRINGBUF_LEN(sb); i++) {
        if (!isalnum(ptr[i])) {
            ret = false;
            break;
        }
    }

    return GaBool_FROM_BOOL(ret);
}

static GaObject *
str_type_invoke(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARG_COUNT_EXACT(vm, 1, argc)) {
        return NULL;
    }
    return GaObj_STR(args[0], vm);
}

static void
assign_methods(GaObject *obj, GaObject *self)
{
    GaObj_SETATTR(obj, NULL, "contains", GaBuiltin_New(str_contains, self));
    GaObj_SETATTR(obj, NULL, "join", GaBuiltin_New(str_join, self));
    GaObj_SETATTR(obj, NULL, "lower", GaBuiltin_New(str_lower, self));
    GaObj_SETATTR(obj, NULL, "replace", GaBuiltin_New(str_replace, self));
    GaObj_SETATTR(obj, NULL, "split", GaBuiltin_New(str_split, self));
    GaObj_SETATTR(obj, NULL, "upper", GaBuiltin_New(str_upper, self));
    GaObj_SETATTR(obj, NULL, "isdigit", GaBuiltin_New(str_isdigit, self));
    GaObj_SETATTR(obj, NULL, "isspace", GaBuiltin_New(str_isspace, self));
    GaObj_SETATTR(obj, NULL, "isalpha", GaBuiltin_New(str_isalpha, self));
    GaObj_SETATTR(obj, NULL, "isalnum", GaBuiltin_New(str_isalnum, self));
}

GaObject *
_GaStr_init()
{
    static struct Ga_Operators str_type_ops = {
        .invoke = str_type_invoke,
    };
    _GaStr_type = GaObj_NewType("Str", &str_type_ops);
    assign_methods(_GaStr_type, NULL);
    return GaObj_INC_REF(_GaStr_type);
}

void
_GaStr_fini()
{
    GaObj_XDEC_REF(_GaStr_type);
}

GaObject *
GaStr_FromStringBuilder(struct stringbuf *sb)
{
    GaObject *obj = GaObj_New(GA_STR_TYPE, &str_ops);
    obj->un.statep = sb;
    assign_methods(obj, obj);
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
    str = GaObj_Super(str, GA_STR_TYPE);

    return str->un.statep;
}

static GaObject *
str_add(GaObject *self, GaContext *vm, GaObject *right)
{
    GaObject *right_str = Ga_ENSURE_TYPE(vm, right, GA_STR_TYPE);

    if (!right_str) {
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
    GaObject *right_str = Ga_ENSURE_TYPE(vm, right, GA_STR_TYPE);
 
    if (!right_str) {
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
    GaObject *key_int = Ga_ENSURE_TYPE(vm, key, GA_INT_TYPE);

    if (!key_int) {
        return NULL;
    }

    uint32_t index = (uint32_t)GaInt_TO_I64(key_int);
    const char *self_str = GaStr_ToCString(self);
    size_t self_len = GaStr_Len(self);

    if (index < self_len) {
        return GaStr_FromCStringEx(&self_str[index], 1);
    }

    GaEval_RaiseException(vm, GaErr_NewIndexError("String index out of range"));

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