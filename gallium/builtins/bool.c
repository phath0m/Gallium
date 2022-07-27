/*
 * bool.c - Builtin boolean type
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
#include <stdbool.h>
#include <gallium/builtins.h>
#include <gallium/object.h>

static bool            bool_istrue(GaObject *, struct vm *);
static GaObject    *   bool_logical_not(GaObject *, struct vm *);
static GaObject    *   bool_str(GaObject *, struct vm *);

GaObject ga_bool_typedef_inst = {
    .ref_count      =   1,
    .un.statep      =   "Bool"
};

static struct ga_obj_ops bool_ops = {
    .istrue         =   bool_istrue,
    .logical_not    =   bool_logical_not,
    .str            =   bool_str
};

GaObject ga_bool_true_inst = {
    .ref_count      =   1,
    .obj_ops        =   &bool_ops,
    .type           =   &ga_bool_typedef_inst,
    .un.state_i8    =   1
};

GaObject ga_bool_false_inst = {
    .ref_count      =   1,
    .obj_ops        =   &bool_ops,
    .type           =   &ga_bool_typedef_inst,
    .un.state_i8    =   0
};

static bool
bool_istrue(GaObject *self, struct vm *vm)
{
    return self->un.state_i8 != 0;
}

static GaObject *
bool_logical_not(GaObject *self, struct vm *vm)
{
    switch (self->un.state_i8) {
        case 0:
            return &ga_bool_true_inst;
        default:
            return &ga_bool_false_inst;
    }
}

static GaObject *
bool_str(GaObject *self, struct vm *vm)
{
    if (GaBool_TO_BOOL(self)) {
        return GaStr_FromCString("True");
    }

    return GaStr_FromCString("False");
}