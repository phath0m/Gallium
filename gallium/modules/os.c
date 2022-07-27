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

static GaObject *
getenv_builtin(GaObject *self, struct vm *vm, int argc, GaObject **args)
{
    if (argc != 1) {
        GaEval_RaiseException(vm, GaErr_NewArgumentError("compile() requires one argument"));
        return NULL;
    }

    GaObject *name = GaObj_Super(args[0], GA_STR_TYPE);

    if (!name) {
        GaEval_RaiseException(vm, GaErr_NewTypeError("Str"));
        return NULL;
    }

    char *val = getenv(GaStr_ToCString(name));

    if (val) {
        return GaStr_FromCString(val);
    }

    
    return GA_NULL;

}

GaObject *
GaMod_OpenOS()
{
    static GaObject *mod = NULL;

    if (mod) {
        return mod;
    }

    mod = GaModule_New("os", NULL, NULL);

    GaObj_SETATTR(mod, NULL, "getenv", GaBuiltin_New(getenv_builtin, NULL));

    return mod;
}