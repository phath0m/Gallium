/*
 * module.c - Gallium's module type. Also contains logic for importing other
 * modules.
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
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gallium/builtins.h>
#include <gallium/dict.h>
#include <gallium/object.h>
#include <gallium/vm.h>
#include "../config.h"
#include "../compiler.h"

#ifndef PATH_MAX
# define PATH_MAX    512
#endif

typedef GaObject *(*mod_open_func)();

GaObject *  GaModule_system_path;

struct builtin_mod_def {
    const char  *   name;
    mod_open_func   func;
};

struct builtin_mod_def builtin_mods[] = {
    {"gallium/ast", GaMod_OpenAst},
    {"gallium/parser", GaMod_OpenParser},
    {"std/os", GaMod_OpenOS},
    {"std/sys", GaMod_OpenSys},
    {NULL, NULL}
};


static void         mod_destroy(GaObject *);
static GaObject *   mod_invoke(GaObject *, GaContext *, int, GaObject **);
static GaObject *   mod_str(GaObject *, GaContext *);

GaObject ga_mod_typedef_inst = {
    .ref_count      =   1,
    .un.statep      =   "Module"
};

static struct Ga_Operators mod_ops = {
    .destroy        =   mod_destroy,
    .invoke         =   mod_invoke,
    .str            =   mod_str
};

struct mod_state {
    GaCodeObject  * constructor;
    char            directory[PATH_MAX+1];
    char            name[];
};

static void
mod_destroy(GaObject *self)
{
    struct mod_state *statep = self->un.statep;
    if (statep->constructor) {
        GaObj_DEC_REF((GaObject *)statep->constructor);
    }
    free(statep);
}

static GaObject *
mod_invoke(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    struct mod_state *statep = self->un.statep;
    return GaEval_ExecFrame(vm, GaFrame_NEW(self, statep->constructor, vm->top), 0, NULL);
}

static GaObject *
mod_str(GaObject *self, GaContext *vm)
{
    struct mod_state *statep = self->un.statep;
    return GaStr_FromCString(statep->name);
}

static const char *
get_module_directory(GaObject *module)
{
    struct mod_state *statep = module->un.statep;
    return statep->directory;
}

static GaObject *
mod_import_file(GaContext *vm, const char *path)
{
    GaObject *mod;

    FILE *fp = fopen(path, "r");

    if (!fp) {
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    size_t fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *src = calloc(fsize + 1, 1);

    if (fread(src, 1, fsize, fp) != fsize) {
        fclose(fp);
    }

    fclose(fp);

    GaObject *code = GaCode_Compile(vm, src);
    free(src);

    if (!code) {
        GaEval_RaiseException(vm, GaObj_MOVE_REF(vm->error));
        vm->error = NULL;
        return NULL;
    }

    mod = GaObj_INC_REF(GaModule_New("__default__", code, path));
    GaModule_Import(mod, NULL, vm->globals);
    GaObj_INVOKE(mod, vm, 0, NULL);

    return GaObj_MOVE_REF(mod);
}

static bool
is_relative_import(const char *name)
{
    return !strncmp(name, "/", 1) || !strncmp(name, "./", 2);
}

static bool
get_module_abspath(const char *module_directory, const char *name,
                   char resolved_path[PATH_MAX+1])
{
    return snprintf(resolved_path, PATH_MAX, "%s/%s.ga", module_directory,
                    name) < PATH_MAX;
}

static bool
resolve_module_path(GaObject *calling_module, const char *name, char module_path[PATH_MAX+1])
{
    bool resolved = false;

    if (is_relative_import(name)) {
        const char *module_directory = get_module_directory(calling_module);
        if (!get_module_abspath(module_directory, name, module_path)) {
            return false;
        }
        resolved = access(module_path, F_OK) == 0;
        goto cleanup_1;
    }

    GaObject *cur = NULL;
    GaObject *iter_obj = GaObj_ITER(GaModule_system_path, NULL);

    if (!iter_obj) {
        return NULL;
    }

    GaObj_INC_REF(iter_obj);

    while (GaObj_ITER_NEXT(iter_obj, NULL)) {
        cur = GaObj_INC_REF(GaObj_ITER_CUR(iter_obj, NULL));

        GaObject *module_path_obj = GaObj_STR(cur, NULL);

        if (!module_path_obj)
            continue;
        
        const char *module_path_c_string = GaStr_ToCString(module_path_obj);

        if (!get_module_abspath(module_path_c_string, name, module_path)) {
            continue;
        }

        if (access(module_path, F_OK) == 0) {
            resolved = true;
            goto cleanup_2;
        }

        if (!get_module_abspath(module_path_c_string, "mod", module_path)) {
            continue;
        }

        if (access(module_path, F_OK) == 0) {
            resolved = true;
            goto cleanup_2;
        }

        GaObj_DEC_REF(cur);
    }

cleanup_2:
    GaObj_DEC_REF(iter_obj);
cleanup_1:
    /*
     * Annoyingly, access() sets errno so I need to reset it here...
     */
    errno = 0;
    return resolved;
}
 
void
GaModule_SetConstructor(GaObject *self, GaObject *code)
{
    struct mod_state *statep = self->un.statep;
    GaObj_XDEC_REF((GaObject *)statep->constructor);
    if (code) {
        statep->constructor = (GaCodeObject *)code;
        GaObj_INC_REF(code);
    }
}

GaObject *
GaModule_New(const char *name, GaObject *code, const char *path)
{
    size_t name_len = strlen(name);
    struct mod_state *statep = calloc(sizeof(struct mod_state) + name_len + 1, 1);
    GaObject *mod = GaObj_New(&ga_mod_typedef_inst, &mod_ops);
    strncpy(statep->name, name, name_len + 1);

    if (path) {
        char path_copy[PATH_MAX+1];
        strncpy(path_copy, path, PATH_MAX);
        strncpy(statep->directory, dirname(path_copy), PATH_MAX);
    }

    mod->un.statep = statep;

    GaModule_SetConstructor(mod, code);

    return mod;
}

static GaObject *
try_import_builtin_module(const char *name)
{
    GaObject *mod = NULL;
    struct builtin_mod_def *def = &builtin_mods[0];
    while (def->name) {
        if (strcmp(def->name, name) == 0) {
            mod = def->func();
            break;
        }
        def++;
    }
    return mod;
}

GaObject *
GaModule_Open(GaObject *self, GaContext *vm, const char *name)
{
    GaObject *mod = NULL;

    if (_Ga_hashmap_get(&vm->import_cache, name, (void**)&mod)) {
        return mod;
    }

    char full_path[PATH_MAX+1];

    if (resolve_module_path(self, name, full_path)) {
        mod = mod_import_file(vm, full_path);
    } else if (!(mod = try_import_builtin_module(name))) {
        GaEval_RaiseException(vm, GaErr_NewImportError(name));
        return NULL;
    }
    if (mod) {
        _Ga_hashmap_set(&vm->import_cache, name, GaObj_INC_REF(mod));
    }
    return mod;
}

void
GaModule_Import(GaObject *self, GaContext *vm, GaObject *mod)
{
    _Ga_iter_t iter;
    _Ga_hashmap_getiter(&mod->dict, &iter);
    _Ga_dict_kvp_t *kvp;

    while (_Ga_iter_next(&iter, (void**)&kvp)) {
        GaObj_SETATTR(self, vm, kvp->key, (struct Ga_Object*)kvp->val);
    }
}


static void
initialize_system_path()
{
    GaList_Append(GaModule_system_path, GaStr_FromCString(MODULE_PATH));

    char *path = getenv("GALLIUM_PATH");

    if (!path) return;

    char *searchdir = strtok(path, ":");

    while (searchdir != NULL) {
        GaList_Append(GaModule_system_path, GaStr_FromCString(searchdir));
        searchdir = strtok(NULL, ":");
    }
}

void
_GaModule_init()
{
    GaModule_system_path = GaList_New();
    GaObj_INC_REF(GaModule_system_path);
    initialize_system_path();
}

void
_GaModule_fini()
{
    GaObj_DEC_REF(GaModule_system_path);
}