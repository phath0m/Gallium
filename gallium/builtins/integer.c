#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <gallium/builtins.h>
#include <gallium/object.h>
#include <gallium/vm.h>

static struct ga_obj *  int_type_invoke(struct ga_obj *, struct vm *, int, struct ga_obj **);

GA_BUILTIN_TYPE_DECL(ga_int_type_inst, "Int", int_type_invoke);

static struct ga_obj *  int_inverse(struct ga_obj *, struct vm *);
static struct ga_obj *  int_negate(struct ga_obj *, struct vm *);
static bool             int_equals(struct ga_obj *, struct vm *, struct ga_obj *);
static bool             int_gt(struct ga_obj *, struct vm *, struct ga_obj *);
static bool             int_ge(struct ga_obj *, struct vm *, struct ga_obj *);
static bool             int_lt(struct ga_obj *, struct vm *, struct ga_obj *);
static bool             int_le(struct ga_obj *, struct vm *, struct ga_obj *);
static struct ga_obj *  int_add(struct ga_obj *, struct vm *, struct ga_obj *);
static struct ga_obj *  int_sub(struct ga_obj *, struct vm *, struct ga_obj *);
static struct ga_obj *  int_mul(struct ga_obj *, struct vm *, struct ga_obj *);
static struct ga_obj *  int_div(struct ga_obj *, struct vm *, struct ga_obj *);
static struct ga_obj *  int_mod(struct ga_obj *, struct vm *, struct ga_obj *);
static struct ga_obj *  int_and(struct ga_obj *, struct vm *, struct ga_obj *);
static struct ga_obj *  int_or(struct ga_obj *, struct vm *, struct ga_obj *);
static struct ga_obj *  int_xor(struct ga_obj *, struct vm *, struct ga_obj *);
static struct ga_obj *  int_shl(struct ga_obj *, struct vm *, struct ga_obj *);
static struct ga_obj *  int_shr(struct ga_obj *, struct vm *, struct ga_obj *);

static struct ga_obj *  int_closed_range(struct ga_obj *, struct vm *, struct ga_obj *);
static struct ga_obj *  int_half_range(struct ga_obj *, struct vm *, struct ga_obj *);

static struct ga_obj *  int_str(struct ga_obj *, struct vm *);

struct ga_obj_ops   int_obj_ops = {
    .inverse        = int_inverse,
    .negate         = int_negate,
    .equals         = int_equals,
    .gt             = int_gt,
    .ge             = int_ge,
    .lt             = int_lt,
    .le             = int_le,
    .add            = int_add,
    .sub            = int_sub,
    .mul            = int_mul,
    .div            = int_div,
    .str            = int_str,
    .mod            = int_mod,
    .band           = int_and,
    .bor            = int_or,
    .bxor           = int_xor,
    .shl            = int_shl,
    .shr            = int_shr,
    .closed_range   = int_closed_range,
    .half_range     = int_half_range
};

static struct ga_obj *
int_type_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc > 2) {
        vm_raise_exception(vm, ga_argument_error_new("Int() requires one argument and one optional argument"));
        return NULL;
    }

    int base = 10;
    struct ga_obj *arg = args[0];

    if (ga_obj_instanceof(arg, &ga_int_type_inst)) {
        struct ga_obj *right_int = ga_obj_super(arg, &ga_int_type_inst);
        return ga_int_from_i64(right_int->un.state_i64);
    }


    if (argc == 2) {
        struct ga_obj *int_arg = ga_obj_super(args[1], GA_INT_TYPE);

        if (!int_arg) {
            vm_raise_exception(vm, ga_type_error_new("Int"));
            return NULL;
        }

        base = (int)ga_int_to_i64(int_arg);

    }

    struct ga_obj *str = GAOBJ_STR(arg, vm);

    if (!str) {
        return NULL;
    }

    GAOBJ_INC_REF(str);

    char *endptr = NULL;
    const char *nptr = ga_str_to_cstring(str);
    int64_t val = strtoll(nptr, &endptr, base);

    GAOBJ_DEC_REF(str);

    if (nptr == endptr) {
        vm_raise_exception(vm, ga_value_error_new("Invalid numeric value supplied to Int()"));
        return NULL;
    }

    /* TODO: check for overflow */
    return ga_int_from_i64(val);
}

static struct ga_obj *
int_inverse(struct ga_obj *self, struct vm *vm)
{
    return ga_int_from_i64(~self->un.state_i64);
}

static struct ga_obj *
int_negate(struct ga_obj *self, struct vm *vm)
{
    return ga_int_from_i64(-self->un.state_i64);
}

static bool
int_equals(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    struct ga_obj *right_int = ga_obj_super(right, &ga_int_type_inst);

    if (!right_int) {
        vm_raise_exception(vm, ga_type_error_new("Int"));
        return false;
    }
    
    return self->un.state_i64 == right_int->un.state_i64;
}

static bool
int_gt(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    struct ga_obj *right_int = ga_obj_super(right, &ga_int_type_inst);

    if (!right_int) {
        vm_raise_exception(vm, ga_type_error_new("Int"));
        return false;
    }

    return self->un.state_i64 > right_int->un.state_i64;
}

static bool
int_ge(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    struct ga_obj *right_int = ga_obj_super(right, &ga_int_type_inst);

    if (!right_int) {
        vm_raise_exception(vm, ga_type_error_new("Int"));
        return false;
    }

    return self->un.state_i64 >= right_int->un.state_i64;
}
static bool
int_lt(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    struct ga_obj *right_int = ga_obj_super(right, &ga_int_type_inst);

    if (!right_int) {
        vm_raise_exception(vm, ga_type_error_new("Int"));
        return false;
    }

    return self->un.state_i64 < right_int->un.state_i64;
}

static bool
int_le(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    struct ga_obj *right_int = ga_obj_super(right, &ga_int_type_inst);

    if (!right_int) {
        vm_raise_exception(vm, ga_type_error_new("Int"));
        return false;
    }

    return self->un.state_i64 <= right_int->un.state_i64;
}

static struct ga_obj *
int_add(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    struct ga_obj *right_int = ga_obj_super(right, &ga_int_type_inst);

    if (!right_int) {
        vm_raise_exception(vm, ga_type_error_new("Int"));
        return false;
    }

    return ga_int_from_i64(self->un.state_i64 + right_int->un.state_i64);
}

static struct ga_obj *
int_sub(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    struct ga_obj *right_int = ga_obj_super(right, &ga_int_type_inst);

    if (!right_int) {
        vm_raise_exception(vm, ga_type_error_new("Int"));
        return false;
    }

    return ga_int_from_i64(self->un.state_i64 - right_int->un.state_i64);
}

static struct ga_obj *
int_mul(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    struct ga_obj *right_int = ga_obj_super(right, &ga_int_type_inst);

    if (!right_int) {
        vm_raise_exception(vm, ga_type_error_new("Int"));
        return false;
    }

    return ga_int_from_i64(self->un.state_i64 * right_int->un.state_i64);
}

static struct ga_obj *
int_div(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    struct ga_obj *right_int = ga_obj_super(right, &ga_int_type_inst);

    if (!right_int) {
        vm_raise_exception(vm, ga_type_error_new("Int"));
        return false;
    }

    return ga_int_from_i64(self->un.state_i64 / right_int->un.state_i64);
}

static struct ga_obj *
int_mod(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    struct ga_obj *right_int = ga_obj_super(right, &ga_int_type_inst);

    if (!right_int) {
        vm_raise_exception(vm, ga_type_error_new("Int"));
        return false;
    }

    return ga_int_from_i64(self->un.state_i64 % right_int->un.state_i64);
}

static struct ga_obj *
int_and(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    struct ga_obj *right_int = ga_obj_super(right, &ga_int_type_inst);

    if (!right_int) {
        vm_raise_exception(vm, ga_type_error_new("Int"));
        return false;
    }

    return ga_int_from_i64(self->un.state_i64 & right_int->un.state_i64);
}

static struct ga_obj *
int_or(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    struct ga_obj *right_int = ga_obj_super(right, &ga_int_type_inst);

    if (!right_int) {
        vm_raise_exception(vm, ga_type_error_new("Int"));
        return false;
    }

    return ga_int_from_i64(self->un.state_i64 | right_int->un.state_i64);
}

static struct ga_obj *
int_xor(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    struct ga_obj *right_int = ga_obj_super(right, &ga_int_type_inst);

    if (!right_int) {
        vm_raise_exception(vm, ga_type_error_new("Int"));
        return false;
    }

    return ga_int_from_i64(self->un.state_i64 ^ right_int->un.state_i64);
}

static struct ga_obj *
int_shl(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    struct ga_obj *right_int = ga_obj_super(right, &ga_int_type_inst);

    if (!right_int) {
        vm_raise_exception(vm, ga_type_error_new("Int"));
        return false;
    }

    return ga_int_from_i64(self->un.state_i64 << right_int->un.state_i64);
}

static struct ga_obj *
int_shr(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    struct ga_obj *right_int = ga_obj_super(right, &ga_int_type_inst);

    if (!right_int) {
        vm_raise_exception(vm, ga_type_error_new("Int"));
        return false;
    }

    return ga_int_from_i64(self->un.state_i64 >> right_int->un.state_i64);
}

static struct ga_obj *
int_closed_range(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    struct ga_obj *right_int = ga_obj_super(right, &ga_int_type_inst);

    if (!right_int) {
        vm_raise_exception(vm, ga_type_error_new("Int"));
        return false;
    }

    return ga_range_new(self->un.state_i64, right_int->un.state_i64+1, 1);
}

static struct ga_obj *
int_half_range(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    struct ga_obj *right_int = ga_obj_super(right, &ga_int_type_inst);

    if (!right_int) {
        vm_raise_exception(vm, ga_type_error_new("Int"));
        return false;
    }

    return ga_range_new(self->un.state_i64, right_int->un.state_i64, 1);
}

static struct ga_obj *
int_str(struct ga_obj *self, struct vm *vm)
{
    char buf[16];
    sprintf(buf, "%ld", (long int)self->un.state_i64);
    return ga_str_from_cstring(buf);
}

struct ga_obj *
ga_int_from_i64(int64_t val)
{
    struct ga_obj *obj = ga_obj_new(&ga_int_type_inst, &int_obj_ops);
    obj->un.state_i64 = val;
    return obj;
}

int64_t
ga_int_to_i64(struct ga_obj *obj)
{
    return obj->un.state_i64;
}
