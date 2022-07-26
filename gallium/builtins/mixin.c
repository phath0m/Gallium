/*
 * mixin.c - Mixin type
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

GA_BUILTIN_TYPE_DECL(ga_mixin_type_inst, "Mixin", NULL);

static void
apply_methods(struct ga_obj *obj, struct ga_obj *mixin)
{
    struct ga_dict_kvp *kvp;
    list_iter_t iter;
    ga_dict_get_iter(mixin, &iter);

    while (iter_next_elem(&iter, (void**)&kvp)) {
        if (kvp->key->type != &ga_str_type_inst) {
            /* this shouldn't happen */
            return;
        }

        const char *str = ga_str_to_cstring(kvp->key);

        GAOBJ_SETATTR(obj, NULL, str, kvp->val);
    }
}

struct ga_obj *
ga_mixin_new(struct ga_obj *dict)
{
    struct ga_obj *mixin = ga_obj_new(&ga_mixin_type_inst, NULL);

    apply_methods(mixin, dict);

    return mixin;
}