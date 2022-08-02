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
#include "../compiler.h"

#ifndef PATH_MAX
#define PATH_MAX    512
#endif

typedef GaObject *(*mod_open_func)();

struct builtin_mod_def {
    const char  *   name;
    mod_open_func   func;
};

struct builtin_mod_def builtin_mods[] = {
    {"gallium/ast", GaMod_OpenAst},
    {"gallium/parser", GaMod_OpenParser},
    {"std/os", GaMod_OpenOS},
    {NULL, NULL}
};

static void         mod_destroy(GaObject *);
static GaObject *   mod_invoke(GaObject *, GaContext *, int, GaObject **);
static GaObject *   mod_str(GaObject *, GaContext *);

GaObject ga_mod_typedef_inst = {
    .ref_count      =   1,
    .un.statep      =   "Module"
};

static struct ga_obj_ops mod_ops = {
    .destroy        =   mod_destroy,
    .invoke         =   mod_invoke,
    .str            =   mod_str
};

struct mod_state {
    struct ga_proc  *       constructor;
    GaObject        *       code;
    char                    path[PATH_MAX+1];
    char                    name[];
};


static void
mod_destroy(GaObject *self)
{
    struct mod_state *statep = self->un.statep;

    if (statep->code) {
        GaObj_DEC_REF(statep->code);
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

    struct compiler_state comp_state;
    memset(&comp_state, 0, sizeof(comp_state));

    GaObject *code = GaCode_Compile(&comp_state, src);

    free(src);

    if (!code) {
        GaEval_RaiseException(vm, GaErr_NewSyntaxError("Syntax Error"));
        return NULL;
    }

    mod = GaObj_INC_REF(GaModule_New("__default__", code, path));
    GaModule_Import(mod, NULL, GaMod_OpenBuiltins());
    GaObj_INVOKE(mod, vm, 0, NULL);

    return GaObj_MOVE_REF(mod);
}

static bool
mod_search_in_path(GaObject *calling_module, const char *name, char resolved[PATH_MAX+1])
{
    bool res = false;
    struct mod_state *statep = calling_module->un.statep;

    if (!strncmp(name, "/", 1) || !strncmp(name, "./", 2)) {
        if (snprintf(resolved, PATH_MAX, "%s/%s.ga", statep->path, name) > PATH_MAX) return false;
        res = access(resolved, F_OK) == 0;
        goto cleanup;
    }

    char *path = getenv("GALLIUM_PATH");

    if (!path) return false;

    static char path_copy[PATH_MAX+1];

    strncpy(path_copy, path, PATH_MAX);

    char *searchdir = strtok(path_copy, ":");

    while (searchdir != NULL) {
        if (snprintf(resolved, PATH_MAX, "%s/%s.ga", searchdir, name) > PATH_MAX) continue;
        if (access(resolved, F_OK) == 0) return true;
        if (snprintf(resolved, PATH_MAX, "%s/%s/mod.ga", searchdir, name) < PATH_MAX) {
            res = access(resolved, F_OK) == 0;
            goto cleanup;
        }
        searchdir = strtok(NULL, ":");
    }

cleanup:
    /*
     * Annoyingly, access() sets errno so I need to reset it here...
     */
    errno = 0;
    return res;
}
 
void
GaModule_SetConstructor(GaObject *self, GaObject *code)
{
    struct mod_state *statep = self->un.statep;
    GaObj_XDEC_REF(statep->code);
    if (code) {
        statep->constructor = GaCode_GetProc(code);
        statep->code = GaObj_INC_REF(code);
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
        strncpy(statep->path, dirname(path_copy), PATH_MAX);
    }

    mod->un.statep = statep;

    GaModule_SetConstructor(mod, code);

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

    if (mod_search_in_path(self, name, full_path)) {
        mod = mod_import_file(vm, full_path);
    } else {
        struct builtin_mod_def *def = &builtin_mods[0];

        while (def->name) {
            if (strcmp(def->name, name) == 0) {
                mod = def->func();
                break;
            }
            def++;
        }

        if (!mod) {
            GaEval_RaiseException(vm, GaErr_NewImportError(name));
            return NULL;
        }
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
        GaObj_SETATTR(self, vm, kvp->key, (struct ga_obj*)kvp->val);
    }
}