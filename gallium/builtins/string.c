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

static struct ga_obj    *   ga_str_type_invoke(struct ga_obj *, struct vm *, int, struct ga_obj **);

GA_BUILTIN_TYPE_DECL(ga_str_type_inst, "Str", ga_str_type_invoke);

static struct ga_obj    *   ga_str_add(struct ga_obj *, struct vm *, struct ga_obj *);
static bool                 ga_str_equals(struct ga_obj *, struct vm *, struct ga_obj *);
static struct ga_obj *      ga_str_getindex(struct ga_obj *, struct vm *, struct ga_obj *);
static int64_t              ga_str_hash(struct ga_obj *, struct vm *);
static struct ga_obj    *   ga_str_len_op(struct ga_obj *, struct vm *);
static struct ga_obj    *   ga_str_str(struct ga_obj *, struct vm *);

struct ga_obj_ops   str_obj_ops = {
    .add        = ga_str_add,
    .equals     = ga_str_equals,
    .getindex   = ga_str_getindex,
    .hash       = ga_str_hash,
    .len        = ga_str_len_op,
    .str        = ga_str_str
};

static struct ga_obj *
ga_str_contains_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        vm_raise_exception(vm, ga_argument_error_new("contains() requires one argument"));
        return NULL;
    }

    struct ga_obj *str1_obj = ga_obj_super(args[0], GA_STR_TYPE);

    if (!str1_obj) {
        vm_raise_exception(vm, ga_type_error_new("Str"));
        return NULL;
    }

    size_t self_len = ga_str_len(self);
    size_t str1_len = ga_str_len(str1_obj);

    const char *self_str = ga_str_to_cstring(self);
    const char *str1 = ga_str_to_cstring(str1_obj);

    int pos = 0;

    while (pos < (self_len - str1_len)) {
        if (memcmp(&self_str[pos], str1, str1_len) == 0) {
            return GA_TRUE;
        }
    }

    return GA_FALSE;
}

static struct ga_obj *
ga_str_join_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        vm_raise_exception(vm, ga_argument_error_new("join() requires one argument"));
        return NULL;
    }

    struct ga_obj *iter = ga_obj_iter(args[0], vm);

    if (!iter) {
        vm_raise_exception(vm, ga_type_error_new("Iter"));
        return NULL;
    }

    GAOBJ_INC_REF(iter);

    int i = 0;
    struct stringbuf *self_sb = self->un.statep;
    struct stringbuf *sb = stringbuf_new();

    while (GAOBJ_ITER_NEXT(iter, vm)) {
        struct ga_obj *cur = GAOBJ_ITER_CUR(iter, vm);

        if (!cur) {
            vm_raise_exception(vm, ga_type_error_new("Iter"));
            goto error;
        }
      
        GAOBJ_INC_REF(cur);

        struct ga_obj *cur_str = GAOBJ_INC_REF(GAOBJ_STR(cur, vm));
        struct ga_obj *cur_str_obj = ga_obj_super(cur_str, &ga_str_type_inst);

        if (!cur_str) {
            vm_raise_exception(vm, ga_type_error_new("Str"));
            goto error;
        }
        
        if (i > 0) {
            stringbuf_append_sb(sb, self_sb);
        }
        
        stringbuf_append_sb(sb, cur_str_obj->un.statep);
        GAOBJ_DEC_REF(cur_str);
        GAOBJ_DEC_REF(cur);
        i++;
    }

    GAOBJ_DEC_REF(iter);

    return ga_str_from_stringbuf(sb);

error:
    GAOBJ_DEC_REF(iter);
    stringbuf_destroy(sb);
    return NULL;
}

static struct ga_obj *
ga_str_lower_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    struct stringbuf *sb = stringbuf_dup(self->un.statep);
    unsigned char *ptr = (unsigned char *)STRINGBUF_VALUE(sb);

    for (int i = 0; i < STRINGBUF_LEN(sb); i++) {
        ptr[i] = (unsigned char)tolower(ptr[i]);
    }

    return ga_str_from_stringbuf(sb);
}

static struct ga_obj *
ga_str_replace_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 2) {
        vm_raise_exception(vm, ga_argument_error_new("replace() requires two arguments"));
        return NULL;
    }

    struct ga_obj *str1_obj = ga_obj_super(args[0], GA_STR_TYPE);
    struct ga_obj *str2_obj = ga_obj_super(args[1], GA_STR_TYPE);

    if (!str1_obj || !str2_obj) {
        vm_raise_exception(vm, ga_type_error_new("Str"));
        return NULL;
    }

    size_t self_len = ga_str_len(self);
    size_t str1_len = ga_str_len(str1_obj);
    size_t str2_len = ga_str_len(str2_obj);

    const char *self_str = ga_str_to_cstring(self);
    const char *str1 = ga_str_to_cstring(str1_obj);
    const char *str2 = ga_str_to_cstring(str2_obj);

    struct stringbuf *new_sb = stringbuf_new();

    int substr_start = 0;

    int pos = 0;

    while (pos < (self_len - str1_len)) {
        if (memcmp(&self_str[pos], str1, str1_len) == 0) {
            
            if (substr_start != pos) {
                stringbuf_append_range(new_sb, &self_str[substr_start], pos - substr_start);
            }

            stringbuf_append_range(new_sb, str2, str2_len);
            pos += str1_len;
            substr_start = pos;
        } else {
            pos++;
        }
    }

    pos = self_len;

	if (substr_start != pos) {
		stringbuf_append_range(new_sb, &self_str[substr_start], pos - substr_start);
	}

    return ga_str_from_stringbuf(new_sb);
}

static struct ga_obj *
ga_str_split_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        vm_raise_exception(vm, ga_argument_error_new("split() requires one argument"));
        return NULL;
    }

    struct ga_obj *str1_obj = ga_obj_super(args[0], GA_STR_TYPE);

    if (!str1_obj) {
        vm_raise_exception(vm, ga_type_error_new("Str"));
        return NULL;
    }

    size_t self_len = ga_str_len(self);
    size_t str1_len = ga_str_len(str1_obj);

    const char *self_str = ga_str_to_cstring(self);
    const char *str1 = ga_str_to_cstring(str1_obj);

    struct ga_obj *ret = ga_list_new();

    int pos = 0;
    int substr_start = 0;

    while (pos < (self_len - str1_len)) {
        if (memcmp(&self_str[pos], str1, str1_len) == 0) {
            
            if (substr_start != pos) {
                ga_list_append(ret, ga_str_from_cstring_range(&self_str[substr_start], pos - substr_start));
            }
            
            pos += str1_len;
            substr_start = pos;
        } else {
            pos++;
        }
    }

	pos = self_len;

	if (substr_start != pos) {
		ga_list_append(ret, ga_str_from_cstring_range(&self_str[substr_start], pos - substr_start));
	}


    return ret;
}


static struct ga_obj *
ga_str_upper_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    struct stringbuf *sb = stringbuf_dup(self->un.statep);
    unsigned char *ptr = (unsigned char*)STRINGBUF_VALUE(sb);

    for (unsigned int i = 0; i < STRINGBUF_LEN(sb); i++) {
        ptr[i] = (unsigned char)toupper(ptr[i]);
    }

    return ga_str_from_stringbuf(sb);
}

static struct ga_obj *
ga_str_type_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        vm_raise_exception(vm, ga_argument_error_new("Str() requires one argument"));
        return NULL;
    }

    return GAOBJ_STR(args[0], vm);
}

struct ga_obj *
ga_str_from_stringbuf(struct stringbuf *sb)
{
    struct ga_obj *obj = ga_obj_new(&ga_str_type_inst, &str_obj_ops);
    obj->un.statep = sb;

    GAOBJ_SETATTR(obj, NULL, "contains", ga_builtin_new(ga_str_contains_method, obj));
    GAOBJ_SETATTR(obj, NULL, "join", ga_builtin_new(ga_str_join_method, obj));
    GAOBJ_SETATTR(obj, NULL, "lower", ga_builtin_new(ga_str_lower_method, obj));
    GAOBJ_SETATTR(obj, NULL, "replace", ga_builtin_new(ga_str_replace_method, obj));
    GAOBJ_SETATTR(obj, NULL, "split", ga_builtin_new(ga_str_split_method, obj));
    GAOBJ_SETATTR(obj, NULL, "upper", ga_builtin_new(ga_str_upper_method, obj));
    
    return obj;
}

struct ga_obj *
ga_str_from_cstring(const char *val)
{
    struct stringbuf *sb = stringbuf_new();
    stringbuf_append(sb, val);
    return ga_str_from_stringbuf(sb);
}

struct ga_obj *
ga_str_from_cstring_range(const char *val, size_t len)
{
    struct stringbuf *sb = stringbuf_new();
    stringbuf_append_range(sb, val, len);
    return ga_str_from_stringbuf(sb);
}

size_t
ga_str_len(struct ga_obj *str)
{
    struct stringbuf *sb = str->un.statep;

    return STRINGBUF_LEN(sb);
}

const char *
ga_str_to_cstring(struct ga_obj *str)
{
    struct stringbuf *sb = str->un.statep;

    return STRINGBUF_VALUE(sb);
}

struct stringbuf *
ga_str_to_stringbuf(struct ga_obj *str)
{
    str = ga_obj_super(str, &ga_str_type_inst);

    return str->un.statep;
}

static struct ga_obj *
ga_str_add(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    struct ga_obj *right_str = ga_obj_super(right, &ga_str_type_inst);

    if (!right_str) {
        vm_raise_exception(vm, ga_type_error_new("Str"));
        return NULL;
    }

    struct stringbuf *sb = stringbuf_new();
    struct stringbuf *left_sb = self->un.statep;
    struct stringbuf *right_sb = right_str->un.statep;

    stringbuf_append(sb, STRINGBUF_VALUE(left_sb));
    stringbuf_append(sb, STRINGBUF_VALUE(right_sb));

    return ga_str_from_stringbuf(sb);
}

static bool
ga_str_equals(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    struct ga_obj *right_str = ga_obj_super(right, &ga_str_type_inst);
    
    if (!right_str) {
        vm_raise_exception(vm, ga_type_error_new("Str"));
        return NULL;
    }

    struct stringbuf *left_sb = self->un.statep;
    struct stringbuf *right_sb = right_str->un.statep;

    if (STRINGBUF_LEN(left_sb) != STRINGBUF_LEN(right_sb)) {
        return false;
    }

    return memcmp(STRINGBUF_VALUE(left_sb), STRINGBUF_VALUE(right_sb), STRINGBUF_LEN(left_sb)) == 0;
}

static struct ga_obj *
ga_str_getindex(struct ga_obj *self, struct vm *vm, struct ga_obj *key)
{
    struct ga_obj *key_int = ga_obj_super(key, &ga_int_type_inst);

    if (!key_int) {
        vm_raise_exception(vm, ga_type_error_new("Int"));
        return NULL;
    }

    uint32_t index = (uint32_t)ga_int_to_i64(key_int);
    const char *self_str = ga_str_to_cstring(self);
    size_t self_len = ga_str_len(self);

    if (index < self_len) {
        return ga_str_from_cstring_range(&self_str[index], 1);
    }

    vm_raise_exception(vm, ga_index_error_new("Index out of range"));

    return NULL;
}

static int64_t
ga_str_hash(struct ga_obj *self, struct vm *vm)
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
ga_str_len_op(struct ga_obj *self, struct vm *vm)
{
    return ga_int_from_i64(ga_str_len(self));
}

static struct ga_obj *
ga_str_str(struct ga_obj *self, struct vm *vm)
{
    return self;
}
