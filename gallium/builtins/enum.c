/*
 * enum.c - Enum type
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
#include <stdlib.h>
#include <string.h>
#include <gallium/builtins.h>
#include <gallium/dict.h>
#include <gallium/object.h>
#include <gallium/vm.h>

GA_BUILTIN_TYPE_DECL(ga_enum_type_inst, "Enum", NULL);

static bool ga_enum_equals(struct ga_obj *self, struct vm *vm, struct ga_obj *obj);

struct ga_obj_ops enum_ops = {
    .equals = ga_enum_equals
};


static bool
ga_enum_equals(struct ga_obj *self, struct vm *vm, struct ga_obj *obj)
{
    return obj->type == self->type && obj->un.state_u32 == self->un.state_u32;
}

struct ga_obj *
ga_enum_new(const char *name, struct ga_obj *values_obj)
{
    struct ga_obj *container = ga_obj_new(&ga_enum_type_inst, &enum_ops);
    struct ga_obj *enum_type = ga_obj_new(&ga_enum_type_inst, &enum_ops);

    enum_type->super = GAOBJ_INC_REF(ga_type_new(name));

    /* this is sort of a dangerous function if I ever have any sort of mult-
     * threading so I'm not going to publically export this. Don't use this
     * function please.
     */
    extern int ga_list_get_cells(struct ga_obj *, struct ga_obj ***);

    struct ga_obj **enum_values;

    int num_values = ga_list_get_cells(values_obj, &enum_values);

    for (int i = 0; i < num_values; i++) {
        struct ga_obj *enum_value = ga_obj_new(enum_type, &enum_ops);
        enum_value->un.state_u32 = i;
        const char *name = ga_str_to_cstring(enum_values[i]);
        GAOBJ_SETATTR(container, NULL, name, enum_value);
    }

    return container;
}