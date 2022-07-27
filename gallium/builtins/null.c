/*
 * null.c - Gallium's builtin null type.
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

static bool                 ga_null_istrue(struct ga_obj *, struct vm *);
static struct ga_obj    *   ga_null_str(struct ga_obj *, struct vm *);

struct ga_obj ga_null_typedef_inst = {
    .ref_count      =   1,
    .un.statep      =   "Null"
};

struct ga_obj_ops ga_null_ops = {
    .istrue         =   ga_null_istrue,
    .str            =   ga_null_str
};

struct ga_obj ga_null_inst = {
    .ref_count      =   1,
    .type           =   &ga_null_typedef_inst,
    .obj_ops        =   &ga_null_ops,
    .un.state_i8    =   0
};

static bool
ga_null_istrue(struct ga_obj *self, struct vm *vm)
{
    return false;
}

static struct ga_obj *
ga_null_str(struct ga_obj *self, struct vm *vm)
{
    return GaStr_FromCString("Null");
}

