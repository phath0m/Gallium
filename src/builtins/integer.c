/*
 * integer.c - Builtin integer type
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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <gallium/builtins.h>
#include <gallium/object.h>
#include <gallium/vm.h>

static GaObject *  int_type_invoke(GaObject *, struct vm *, int, GaObject **);

GA_BUILTIN_TYPE_DECL(_GaInt_Type, "Int", int_type_invoke);

static GaObject *   int_inverse(GaObject *, struct vm *);
static GaObject *   int_negate(GaObject *, struct vm *);
static bool         int_equals(GaObject *, struct vm *, GaObject *);
static bool         int_gt(GaObject *, struct vm *, GaObject *);
static bool         int_ge(GaObject *, struct vm *, GaObject *);
static bool         int_lt(GaObject *, struct vm *, GaObject *);
static bool         int_le(GaObject *, struct vm *, GaObject *);
static GaObject *   int_add(GaObject *, struct vm *, GaObject *);
static GaObject *   int_sub(GaObject *, struct vm *, GaObject *);
static GaObject *   int_mul(GaObject *, struct vm *, GaObject *);
static GaObject *   int_div(GaObject *, struct vm *, GaObject *);
static GaObject *   int_mod(GaObject *, struct vm *, GaObject *);
static GaObject *   int_and(GaObject *, struct vm *, GaObject *);
static GaObject *   int_or(GaObject *, struct vm *, GaObject *);
static GaObject *   int_xor(GaObject *, struct vm *, GaObject *);
static GaObject *   int_shl(GaObject *, struct vm *, GaObject *);
static GaObject *   int_shr(GaObject *, struct vm *, GaObject *);

static GaObject *   int_closed_range(GaObject *, struct vm *, GaObject *);
static GaObject *   int_half_range(GaObject *, struct vm *, GaObject *);

static GaObject *   int_str(GaObject *, struct vm *);

struct ga_obj_ops Ga_IntOps = {
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

static GaObject *
int_type_invoke(GaObject *self, struct vm *vm, int argc, GaObject **args)
{
    if (argc > 2) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("Int() requires one argument and one optional argument"));
        return NULL;
    }

    int base = 10;
    GaObject *arg = args[0];

    if (GaObj_IsInstanceOf(arg, &_GaInt_Type)) {
        GaObject *right_int = GaObj_Super(arg, &_GaInt_Type);
        return GaInt_FROM_I64(right_int->un.state_i64);
    }


    if (argc == 2) {
        GaObject *int_arg = GaObj_Super(args[1], GA_INT_TYPE);

        if (!int_arg) {
            GaEval_RaiseException(vm, GaErr_NewTypeError("Int"));
            return NULL;
        }

        base = (int)GaInt_TO_I64(int_arg);

    }

    GaObject *str = GaObj_STR(arg, vm);

    if (!str) {
        return NULL;
    }

    GaObj_INC_REF(str);

    char *endptr = NULL;
    const char *nptr = GaStr_ToCString(str);
    int64_t val = strtoll(nptr, &endptr, base);

    GaObj_DEC_REF(str);

    if (nptr == endptr) {
        GaEval_RaiseException(vm, GaErr_NewValueError("Invalid numeric value supplied to Int()"));
        return NULL;
    }

    /* TODO: check for overflow */
    return GaInt_FROM_I64(val);
}

static GaObject *
int_inverse(GaObject *self, struct vm *vm)
{
    return GaInt_FROM_I64(~self->un.state_i64);
}

static GaObject *
int_negate(GaObject *self, struct vm *vm)
{
    return GaInt_FROM_I64(-self->un.state_i64);
}

static bool
int_equals(GaObject *self, struct vm *vm, GaObject *right)
{
    GaObject *right_int = GaObj_Super(right, &_GaInt_Type);

    if (!right_int) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Int"));
        return false;
    }
    
    return self->un.state_i64 == right_int->un.state_i64;
}

static bool
int_gt(GaObject *self, struct vm *vm, GaObject *right)
{
    GaObject *right_int = GaObj_Super(right, &_GaInt_Type);

    if (!right_int) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Int"));
        return false;
    }

    return self->un.state_i64 > right_int->un.state_i64;
}

static bool
int_ge(GaObject *self, struct vm *vm, GaObject *right)
{
    GaObject *right_int = GaObj_Super(right, &_GaInt_Type);

    if (!right_int) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Int"));
        return false;
    }

    return self->un.state_i64 >= right_int->un.state_i64;
}
static bool
int_lt(GaObject *self, struct vm *vm, GaObject *right)
{
    GaObject *right_int = GaObj_Super(right, &_GaInt_Type);

    if (!right_int) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Int"));
        return false;
    }

    return self->un.state_i64 < right_int->un.state_i64;
}

static bool
int_le(GaObject *self, struct vm *vm, GaObject *right)
{
    GaObject *right_int = GaObj_Super(right, &_GaInt_Type);

    if (!right_int) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Int"));
        return false;
    }

    return self->un.state_i64 <= right_int->un.state_i64;
}

static GaObject *
int_add(GaObject *self, struct vm *vm, GaObject *right)
{
    GaObject *right_int = GaObj_Super(right, &_GaInt_Type);

    if (!right_int) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Int"));
        return false;
    }

    return GaInt_FROM_I64(self->un.state_i64 + right_int->un.state_i64);
}

static GaObject *
int_sub(GaObject *self, struct vm *vm, GaObject *right)
{
    GaObject *right_int = GaObj_Super(right, &_GaInt_Type);

    if (!right_int) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Int"));
        return false;
    }

    return GaInt_FROM_I64(self->un.state_i64 - right_int->un.state_i64);
}

static GaObject *
int_mul(GaObject *self, struct vm *vm, GaObject *right)
{
    GaObject *right_int = GaObj_Super(right, &_GaInt_Type);

    if (!right_int) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Int"));
        return false;
    }

    return GaInt_FROM_I64(self->un.state_i64 * right_int->un.state_i64);
}

static GaObject *
int_div(GaObject *self, struct vm *vm, GaObject *right)
{
    GaObject *right_int = GaObj_Super(right, &_GaInt_Type);

    if (!right_int) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Int"));
        return false;
    }

    return GaInt_FROM_I64(self->un.state_i64 / right_int->un.state_i64);
}

static GaObject *
int_mod(GaObject *self, struct vm *vm, GaObject *right)
{
    GaObject *right_int = GaObj_Super(right, &_GaInt_Type);

    if (!right_int) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Int"));
        return false;
    }

    return GaInt_FROM_I64(self->un.state_i64 % right_int->un.state_i64);
}

static GaObject *
int_and(GaObject *self, struct vm *vm, GaObject *right)
{
    GaObject *right_int = GaObj_Super(right, &_GaInt_Type);

    if (!right_int) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Int"));
        return false;
    }

    return GaInt_FROM_I64(self->un.state_i64 & right_int->un.state_i64);
}

static GaObject *
int_or(GaObject *self, struct vm *vm, GaObject *right)
{
    GaObject *right_int = GaObj_Super(right, &_GaInt_Type);

    if (!right_int) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Int"));
        return false;
    }

    return GaInt_FROM_I64(self->un.state_i64 | right_int->un.state_i64);
}

static GaObject *
int_xor(GaObject *self, struct vm *vm, GaObject *right)
{
    GaObject *right_int = GaObj_Super(right, &_GaInt_Type);

    if (!right_int) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Int"));
        return false;
    }

    return GaInt_FROM_I64(self->un.state_i64 ^ right_int->un.state_i64);
}

static GaObject *
int_shl(GaObject *self, struct vm *vm, GaObject *right)
{
    GaObject *right_int = GaObj_Super(right, &_GaInt_Type);

    if (!right_int) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Int"));
        return false;
    }

    return GaInt_FROM_I64(self->un.state_i64 << right_int->un.state_i64);
}

static GaObject *
int_shr(GaObject *self, struct vm *vm, GaObject *right)
{
    GaObject *right_int = GaObj_Super(right, &_GaInt_Type);

    if (!right_int) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Int"));
        return false;
    }

    return GaInt_FROM_I64(self->un.state_i64 >> right_int->un.state_i64);
}

static GaObject *
int_closed_range(GaObject *self, struct vm *vm, GaObject *right)
{
    GaObject *right_int = GaObj_Super(right, &_GaInt_Type);

    if (!right_int) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Int"));
        return false;
    }

    return GaRange_New(self->un.state_i64, right_int->un.state_i64+1, 1);
}

static GaObject *
int_half_range(GaObject *self, struct vm *vm, GaObject *right)
{
    GaObject *right_int = GaObj_Super(right, &_GaInt_Type);

    if (!right_int) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Int"));
        return false;
    }

    return GaRange_New(self->un.state_i64, right_int->un.state_i64, 1);
}

static GaObject *
int_str(GaObject *self, struct vm *vm)
{
    char buf[16];
    sprintf(buf, "%ld", (long int)self->un.state_i64);
    return GaStr_FromCString(buf);
}