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

static GaObject *  int_type_invoke(GaObject *, GaContext *, int, GaObject **);

GA_BUILTIN_TYPE_DECL(_GaInt_Type, "Int", int_type_invoke);

static GaObject *   int_inverse(GaObject *, GaContext *);
static GaObject *   int_negate(GaObject *, GaContext *);
static bool         int_equals(GaObject *, GaContext *, GaObject *);
static bool         int_gt(GaObject *, GaContext *, GaObject *);
static bool         int_ge(GaObject *, GaContext *, GaObject *);
static bool         int_lt(GaObject *, GaContext *, GaObject *);
static bool         int_le(GaObject *, GaContext *, GaObject *);
static GaObject *   int_add(GaObject *, GaContext *, GaObject *);
static GaObject *   int_sub(GaObject *, GaContext *, GaObject *);
static GaObject *   int_mul(GaObject *, GaContext *, GaObject *);
static GaObject *   int_div(GaObject *, GaContext *, GaObject *);
static GaObject *   int_mod(GaObject *, GaContext *, GaObject *);
static GaObject *   int_and(GaObject *, GaContext *, GaObject *);
static GaObject *   int_or(GaObject *, GaContext *, GaObject *);
static GaObject *   int_xor(GaObject *, GaContext *, GaObject *);
static GaObject *   int_shl(GaObject *, GaContext *, GaObject *);
static GaObject *   int_shr(GaObject *, GaContext *, GaObject *);

static GaObject *   int_closed_range(GaObject *, GaContext *, GaObject *);
static GaObject *   int_half_range(GaObject *, GaContext *, GaObject *);

static GaObject *   int_str(GaObject *, GaContext *);

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
int_type_invoke(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARG_COUNT_MIN(vm, 1, argc)) {
        return NULL;
    }

    int base = 10;
    GaObject *arg = args[0];

    if (GaObj_IsInstanceOf(arg, &_GaInt_Type)) {
        GaObject *right_int = GaObj_Super(arg, &_GaInt_Type);
        return GaInt_FROM_I64(right_int->un.state_i64);
    }

    if (argc == 2) {
        GaObject *int_arg = Ga_ENSURE_TYPE(vm, args[1], GA_INT_TYPE);

        if (!int_arg) {
            return NULL;
        }

        base = (int)GaInt_TO_I64(int_arg);
    }

    GaObject *str = GaObj_STR(arg, vm);

    if (!str) return NULL;

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
int_inverse(GaObject *self, GaContext *vm)
{
    return GaInt_FROM_I64(~self->un.state_i64);
}

static GaObject *
int_negate(GaObject *self, GaContext *vm)
{
    return GaInt_FROM_I64(-self->un.state_i64);
}

static bool
int_equals(GaObject *self, GaContext *vm, GaObject *right)
{
    GaObject *right_int = Ga_ENSURE_TYPE(vm, right, GA_INT_TYPE);

    if (!right_int) {
        return false;
    }
    
    return self->un.state_i64 == right_int->un.state_i64;
}

static bool
int_gt(GaObject *self, GaContext *vm, GaObject *right)
{
    GaObject *right_int = Ga_ENSURE_TYPE(vm, right, GA_INT_TYPE);

    if (!right_int) {
        return false;
    }

    return self->un.state_i64 > right_int->un.state_i64;
}

static bool
int_ge(GaObject *self, GaContext *vm, GaObject *right)
{
    GaObject *right_int = Ga_ENSURE_TYPE(vm, right, GA_INT_TYPE);

    if (!right_int) {
        return false;
    }

    return self->un.state_i64 >= right_int->un.state_i64;
}
static bool
int_lt(GaObject *self, GaContext *vm, GaObject *right)
{
    GaObject *right_int = Ga_ENSURE_TYPE(vm, right, GA_INT_TYPE);

    if (!right_int) {
        return false;
    }

    return self->un.state_i64 < right_int->un.state_i64;
}

static bool
int_le(GaObject *self, GaContext *vm, GaObject *right)
{
    GaObject *right_int = Ga_ENSURE_TYPE(vm, right, GA_INT_TYPE);

    if (!right_int) {
        return false;
    }

    return self->un.state_i64 <= right_int->un.state_i64;
}

static GaObject *
int_add(GaObject *self, GaContext *vm, GaObject *right)
{
    GaObject *right_int = Ga_ENSURE_TYPE(vm, right, GA_INT_TYPE);

    if (!right_int) {
        return false;
    }

    return GaInt_FROM_I64(self->un.state_i64 + right_int->un.state_i64);
}

static GaObject *
int_sub(GaObject *self, GaContext *vm, GaObject *right)
{
    GaObject *right_int = Ga_ENSURE_TYPE(vm, right, GA_INT_TYPE);

    if (!right_int) {
        return false;
    }

    return GaInt_FROM_I64(self->un.state_i64 - right_int->un.state_i64);
}

static GaObject *
int_mul(GaObject *self, GaContext *vm, GaObject *right)
{
    GaObject *right_int = Ga_ENSURE_TYPE(vm, right, GA_INT_TYPE);

    if (!right_int) {
        return false;
    }

    return GaInt_FROM_I64(self->un.state_i64 * right_int->un.state_i64);
}

static GaObject *
int_div(GaObject *self, GaContext *vm, GaObject *right)
{
    GaObject *right_int = Ga_ENSURE_TYPE(vm, right, GA_INT_TYPE);

    if (!right_int) {
        return false;
    }

    return GaInt_FROM_I64(self->un.state_i64 / right_int->un.state_i64);
}

static GaObject *
int_mod(GaObject *self, GaContext *vm, GaObject *right)
{
    GaObject *right_int = Ga_ENSURE_TYPE(vm, right, GA_INT_TYPE);

    if (!right_int) {
        return false;
    }

    return GaInt_FROM_I64(self->un.state_i64 % right_int->un.state_i64);
}

static GaObject *
int_and(GaObject *self, GaContext *vm, GaObject *right)
{
    GaObject *right_int = Ga_ENSURE_TYPE(vm, right, GA_INT_TYPE);

    if (!right_int) {
        return false;
    }

    return GaInt_FROM_I64(self->un.state_i64 & right_int->un.state_i64);
}

static GaObject *
int_or(GaObject *self, GaContext *vm, GaObject *right)
{
    GaObject *right_int = Ga_ENSURE_TYPE(vm, right, GA_INT_TYPE);

    if (!right_int) {
        return false;
    }

    return GaInt_FROM_I64(self->un.state_i64 | right_int->un.state_i64);
}

static GaObject *
int_xor(GaObject *self, GaContext *vm, GaObject *right)
{
    GaObject *right_int = Ga_ENSURE_TYPE(vm, right, GA_INT_TYPE);

    if (!right_int) {
        return false;
    }

    return GaInt_FROM_I64(self->un.state_i64 ^ right_int->un.state_i64);
}

static GaObject *
int_shl(GaObject *self, GaContext *vm, GaObject *right)
{
    GaObject *right_int = Ga_ENSURE_TYPE(vm, right, GA_INT_TYPE);

    if (!right_int) {
        return false;
    }

    return GaInt_FROM_I64(self->un.state_i64 << right_int->un.state_i64);
}

static GaObject *
int_shr(GaObject *self, GaContext *vm, GaObject *right)
{
    GaObject *right_int = Ga_ENSURE_TYPE(vm, right, GA_INT_TYPE);

    if (!right_int) {
        return false;
    }

    return GaInt_FROM_I64(self->un.state_i64 >> right_int->un.state_i64);
}

static GaObject *
int_closed_range(GaObject *self, GaContext *vm, GaObject *right)
{
    GaObject *right_int = Ga_ENSURE_TYPE(vm, right, GA_INT_TYPE);

    if (!right_int) {
        return false;
    }

    return GaRange_New(self->un.state_i64, right_int->un.state_i64+1, 1);
}

static GaObject *
int_half_range(GaObject *self, GaContext *vm, GaObject *right)
{
    GaObject *right_int = Ga_ENSURE_TYPE(vm, right, GA_INT_TYPE);

    if (!right_int) {
        return false;
    }

    return GaRange_New(self->un.state_i64, right_int->un.state_i64, 1);
}

static GaObject *
int_str(GaObject *self, GaContext *vm)
{
    char buf[16];
    sprintf(buf, "%ld", (long int)self->un.state_i64);
    return GaStr_FromCString(buf);
}