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

static GaObject *  float_type_invoke(GaObject *, GaContext *, int, GaObject **);

GA_BUILTIN_TYPE_DECL(_GaFloat_Type, "Float", float_type_invoke);

static GaObject *   float_negate(GaObject *, GaContext *);
static bool         float_equals(GaObject *, GaContext *, GaObject *);
static bool         float_gt(GaObject *, GaContext *, GaObject *);
static bool         float_ge(GaObject *, GaContext *, GaObject *);
static bool         float_lt(GaObject *, GaContext *, GaObject *);
static bool         float_le(GaObject *, GaContext *, GaObject *);
static GaObject *   float_add(GaObject *, GaContext *, GaObject *);
static GaObject *   float_sub(GaObject *, GaContext *, GaObject *);
static GaObject *   float_mul(GaObject *, GaContext *, GaObject *);
static GaObject *   float_div(GaObject *, GaContext *, GaObject *);
static GaObject *   float_mod(GaObject *, GaContext *, GaObject *);
static GaObject *   float_str(GaObject *, GaContext *);

struct Ga_Operators Ga_FloatOps = {
    .negate         = float_negate,
    .equals         = float_equals,
    .gt             = float_gt,
    .ge             = float_ge,
    .lt             = float_lt,
    .le             = float_le,
    .add            = float_add,
    .sub            = float_sub,
    .mul            = float_mul,
    .div            = float_div,
    .str            = float_str,
    .mod            = float_mod,
};

static GaObject *
float_type_invoke(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    if (!Ga_CHECK_ARG_COUNT_EXACT(vm, 1, argc)) {
        return NULL;
    }

    GaObject *arg = args[0];

    if (GaObj_IsInstanceOf(arg, &_GaFloat_Type)) {
        GaObject *right_float = GaObj_Super(arg, &_GaFloat_Type);
        return GaFloat_FROM_DOUBLE(right_float->un.state_f64);
    }

    GaObject *str = GaObj_STR(arg, vm);

    if (!str) return NULL;

    GaObj_INC_REF(str);

    double val = atof(GaStr_ToCString(str));

    GaObj_DEC_REF(str);

    return GaFloat_FROM_DOUBLE(val);
}

static GaObject *
float_negate(GaObject *self, GaContext *vm)
{
    return GaFloat_FROM_DOUBLE(-self->un.state_f64);
}

static bool
float_equals(GaObject *self, GaContext *vm, GaObject *right)
{
    GaObject *right_float = Ga_ENSURE_TYPE(vm, right, Ga_FLOAT_TYPE);

    if (!right_float) {
        return false;
    }
    
    return self->un.state_f64 == right_float->un.state_f64;
}

static bool
float_gt(GaObject *self, GaContext *vm, GaObject *right)
{
    GaObject *right_float = Ga_ENSURE_TYPE(vm, right, Ga_FLOAT_TYPE);

    if (!right_float) {
        return false;
    }

    return self->un.state_f64 > right_float->un.state_f64;
}

static bool
float_ge(GaObject *self, GaContext *vm, GaObject *right)
{
    GaObject *right_float = Ga_ENSURE_TYPE(vm, right, Ga_FLOAT_TYPE);

    if (!right_float) {
        return false;
    }

    return self->un.state_f64 >= right_float->un.state_f64;
}
static bool
float_lt(GaObject *self, GaContext *vm, GaObject *right)
{
    GaObject *right_float = Ga_ENSURE_TYPE(vm, right, Ga_FLOAT_TYPE);

    if (!right_float) {
        return false;
    }

    return self->un.state_f64 < right_float->un.state_f64;
}

static bool
float_le(GaObject *self, GaContext *vm, GaObject *right)
{
    GaObject *right_float = Ga_ENSURE_TYPE(vm, right, Ga_FLOAT_TYPE);

    if (!right_float) {
        return false;
    }

    return self->un.state_f64 <= right_float->un.state_f64;
}

static GaObject *
float_add(GaObject *self, GaContext *vm, GaObject *right)
{
    GaObject *right_float = Ga_ENSURE_TYPE(vm, right, Ga_FLOAT_TYPE);

    if (!right_float) {
        return false;
    }

    return GaFloat_FROM_DOUBLE(self->un.state_f64 + right_float->un.state_f64);
}

static GaObject *
float_sub(GaObject *self, GaContext *vm, GaObject *right)
{
    GaObject *right_float = Ga_ENSURE_TYPE(vm, right, Ga_FLOAT_TYPE);

    if (!right_float) {
        return false;
    }

    return GaFloat_FROM_DOUBLE(self->un.state_f64 - right_float->un.state_f64);
}

static GaObject *
float_mul(GaObject *self, GaContext *vm, GaObject *right)
{
    GaObject *right_float = Ga_ENSURE_TYPE(vm, right, Ga_FLOAT_TYPE);

    if (!right_float) {
        return false;
    }

    return GaFloat_FROM_DOUBLE(self->un.state_f64 * right_float->un.state_f64);
}

static GaObject *
float_div(GaObject *self, GaContext *vm, GaObject *right)
{
    GaObject *right_float = Ga_ENSURE_TYPE(vm, right, Ga_FLOAT_TYPE);

    if (!right_float) {
        return false;
    }

    return GaFloat_FROM_DOUBLE(self->un.state_f64 / right_float->un.state_f64);
}

static GaObject *
float_mod(GaObject *self, GaContext *vm, GaObject *right)
{
    GaObject *right_float = Ga_ENSURE_TYPE(vm, right, Ga_FLOAT_TYPE);

    if (!right_float) {
        return false;
    }
    /* TODO: Implement this. */
    return GaFloat_FROM_DOUBLE(-1.0);
}

static GaObject *
float_str(GaObject *self, GaContext *vm)
{
    char buf[16];
    sprintf(buf, "%f", self->un.state_f64);
    return GaStr_FromCString(buf);
}