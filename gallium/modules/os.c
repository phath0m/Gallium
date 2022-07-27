/*
 * os.c - Gallium OS module
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
#include <stdio.h>
#include <stdlib.h>
#include <gallium/builtins.h>
#include <gallium/object.h>
#include <gallium/vm.h>

static struct ga_obj *
getenv_builtin(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, ga_argument_error_new("compile() requires one argument"));
        return NULL;
    }

    struct ga_obj *name = GaObj_Super(args[0], GA_STR_TYPE);

    if (!name) {
        GaEval_RaiseException(vm, ga_type_error_new("Str"));
        return NULL;
    }

    char *val = getenv(ga_str_to_cstring(name));

    if (val) {
        return ga_str_from_cstring(val);
    }

    
    return GA_NULL;

}

struct ga_obj *
ga_os_mod_open()
{
    static struct ga_obj *mod = NULL;

    if (mod) {
        return mod;
    }

    mod = ga_mod_new("os", NULL, NULL);

    GaObj_SETATTR(mod, NULL, "getenv", ga_builtin_new(getenv_builtin, NULL));

    return mod;
}