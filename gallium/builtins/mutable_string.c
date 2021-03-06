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
        vm_raise_exception(vm, ga_argument_error_new("append() requires one argument"));
        return NULL;
    }

    struct ga_obj *str_obj = ga_obj_super(args[0], GA_STR_TYPE);

    if (!str_obj) {
        vm_raise_exception(vm, ga_type_error_new("Str"));
        return NULL;
    }

    const char *str = ga_str_to_cstring(str_obj);
    size_t len = ga_str_len(str_obj);

    struct stringbuf *sb = self->un.statep;

    stringbuf_append_range(sb, str, len);

    return GA_NULL;
}

static struct ga_obj *
ga_mutstr_type_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc > 1) {
        vm_raise_exception(vm, ga_argument_error_new("MutStr() accepts one optional argument"));
        return NULL;
    }

    struct ga_obj *obj = ga_obj_new(&ga_mutstr_type_inst, &mutstr_obj_ops);

    obj->un.statep = stringbuf_new();

    GAOBJ_SETATTR(obj, NULL, "append", ga_builtin_new(ga_mutstr_append_method, obj));

    return obj;
}

static bool
ga_mutstr_equals(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    struct ga_obj *right_str = ga_obj_super(right, &ga_mutstr_type_inst);

    if (!right_str) {
        vm_raise_exception(vm, ga_type_error_new("MutStr"));
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
    struct stringbuf *sb = stringbuf_dup(self->un.statep);
    return ga_str_from_stringbuf(sb);
}
