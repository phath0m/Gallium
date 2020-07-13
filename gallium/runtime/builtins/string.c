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
static int64_t              ga_str_hash(struct ga_obj *, struct vm *);
static struct ga_obj    *   ga_str_str(struct ga_obj *, struct vm *);

struct ga_obj_ops   str_obj_ops = {
    .add    = ga_str_add,
    .equals = ga_str_equals,
    .hash   = ga_str_hash,
    .str    = ga_str_str
};

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
    char *ptr = STRINGBUF_VALUE(sb);

    for (int i = 0; i < STRINGBUF_LEN(sb); i++) {
        ptr[i] = tolower(ptr[i]);
    }

    return ga_str_from_stringbuf(sb);
}

static struct ga_obj *
ga_str_upper_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    struct stringbuf *sb = stringbuf_dup(self->un.statep);
    char *ptr = STRINGBUF_VALUE(sb);

    for (int i = 0; i < STRINGBUF_LEN(sb); i++) {
        ptr[i] = toupper(ptr[i]);
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
    GAOBJ_SETATTR(obj, NULL, "join", ga_builtin_new(ga_str_join_method, obj));
    GAOBJ_SETATTR(obj, NULL, "lower", ga_builtin_new(ga_str_lower_method, obj));
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
ga_str_str(struct ga_obj *self, struct vm *vm)
{
    return self;
}
