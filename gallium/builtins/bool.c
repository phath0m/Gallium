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

static bool                 ga_bool_istrue(struct ga_obj *, struct vm *);
static struct ga_obj    *   ga_bool_logical_not(struct ga_obj *, struct vm *);
static struct ga_obj    *   ga_bool_str(struct ga_obj *, struct vm *);

struct ga_obj ga_bool_typedef_inst = {
    .ref_count      =   1,
    .un.statep      =   "Bool"
};

struct ga_obj_ops ga_bool_ops = {
    .istrue         =   ga_bool_istrue,
    .logical_not    =   ga_bool_logical_not,
    .str            =   ga_bool_str
};

struct ga_obj ga_bool_true_inst = {
    .ref_count      =   1,
    .obj_ops        =   &ga_bool_ops,
    .type           =   &ga_bool_typedef_inst,
    .un.state_i8    =   1
};

struct ga_obj ga_bool_false_inst = {
    .ref_count      =   1,
    .obj_ops        =   &ga_bool_ops,
    .type           =   &ga_bool_typedef_inst,
    .un.state_i8    =   0
};

static bool
ga_bool_istrue(struct ga_obj *self, struct vm *vm)
{
    return self->un.state_i8 != 0;
}

static struct ga_obj *
ga_bool_logical_not(struct ga_obj *self, struct vm *vm)
{
    switch (self->un.state_i8) {
        case 0:
            return &ga_bool_true_inst;
        default:
            return &ga_bool_false_inst;
    }
}

static struct ga_obj *
ga_bool_str(struct ga_obj *self, struct vm *vm)
{
    if (GA_BOOL_TO_BOOL(self)) {
        return ga_str_from_cstring("True");
    }

    return ga_str_from_cstring("False");
}