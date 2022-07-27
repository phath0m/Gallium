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
#include <gallium/compiler.h>
#include <gallium/dict.h>
#include <gallium/object.h>
#include <gallium/vm.h>

#ifndef PATH_MAX
#define PATH_MAX    512
#endif

typedef struct ga_obj *(*mod_open_func)();

struct builtin_mod_def {
    const char  *   name;
    mod_open_func   func;
};

struct builtin_mod_def builtin_mods[] = {
    {"gallium/ast", ga_ast_mod_open},
    {"gallium/parser", ga_parser_mod_open},
    {"std/os", ga_os_mod_open},
    {NULL, NULL}
};

static void             ga_mod_destroy(struct ga_obj *);
static struct ga_obj *  ga_mod_invoke(struct ga_obj *, struct vm *, int, struct ga_obj **);
static struct ga_obj *  ga_mod_str(struct ga_obj *, struct vm *);

struct ga_obj ga_mod_typedef_inst = {
    .ref_count      =   1,
    .un.statep      =   "Module"
};

struct ga_obj_ops ga_mod_ops = {
    .destroy        =   ga_mod_destroy,
    .invoke         =   ga_mod_invoke,
    .str            =   ga_mod_str
};

struct ga_mod_state {
    struct ga_proc  *       constructor;
    struct ga_obj   *       code;
    struct dict             imports;
    char                    path[PATH_MAX+1];
    char                    name[];
};

static void
mod_dict_destroy_cb(void *p, void *s)
{
    struct ga_obj *obj = p;

    GaObj_DEC_REF(obj);
}

static void
ga_mod_destroy(struct ga_obj *self)
{
    struct ga_mod_state *statep = self->un.statep;

    if (statep->code) {
        GaObj_DEC_REF(statep->code);
        
        dict_fini(&statep->imports, mod_dict_destroy_cb, NULL);
    }

    free(statep);
}

static struct ga_obj *
ga_mod_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    struct ga_mod_state *statep = self->un.statep;
    return GaEval_ExecFrame(vm, STACKFRAME_NEW(self, statep->constructor, vm->top), 0, NULL);
}

static struct ga_obj *
ga_mod_str(struct ga_obj *self, struct vm *vm)
{
    struct ga_mod_state *statep = self->un.statep;

    return ga_str_from_cstring(statep->name);
}

static struct ga_obj *
ga_mod_import_source(struct vm *vm, const char *path)
{
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

    struct ga_obj *code = GaCode_Compile(&comp_state, src);

    free(src);

    if (!code) {
        /* raise syntax exception */
        return NULL;
    }

    struct ga_obj *mod = GaObj_INC_REF(ga_mod_new("__default__", code, path));

    ga_mod_import(mod, NULL, ga_builtin_mod());

    GaObj_INVOKE(mod, vm, 0, NULL);

    return GaObj_MOVE_REF(mod);
}

static bool
ga_mod_search_in_path(struct ga_obj *calling_module, const char *name, char resolved[PATH_MAX+1])
{
    bool res = false;
    struct ga_mod_state *statep = calling_module->un.statep;

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
 
struct ga_obj *
ga_mod_new(const char *name, struct ga_obj *code, const char *path)
{
    size_t name_len = strlen(name);
    struct ga_mod_state *statep = calloc(sizeof(struct ga_mod_state) + name_len + 1, 1);
    struct ga_obj *mod = GaObj_New(&ga_mod_typedef_inst, &ga_mod_ops);
    strncpy(statep->name, name, name_len + 1);

    if (code) {
        struct ga_proc *constructor = ga_code_get_proc(code);
        statep->constructor = constructor;
        statep->code = GaObj_INC_REF(code);
    }

    if (path) {
        char path_copy[PATH_MAX+1];
        strncpy(path_copy, path, PATH_MAX);
        strncpy(statep->path, dirname(path_copy), PATH_MAX);
    }

    mod->un.statep = statep;

    return mod;
}

struct ga_obj *
ga_mod_open(struct ga_obj *self, struct vm *vm, const char *name)
{
    struct ga_mod_state *statep = self->un.statep;
    struct ga_obj *mod = NULL;

    if (GaHashMap_Get(&statep->imports, name, (void**)&mod)) {
        return mod;
    }

    char full_path[PATH_MAX+1];

    if (ga_mod_search_in_path(self, name, full_path)) {
        mod = ga_mod_import_source(vm, full_path);
        goto end;
    }

    struct builtin_mod_def *def = &builtin_mods[0];

    while (def->name) {
        if (strcmp(def->name, name) == 0) {
            mod = def->func();
            goto end;
        }
        def++;
    }

    if (!mod) {
        GaEval_RaiseException(vm, ga_import_error_new(name));
        return NULL;
    }

end:
    GaHashMap_Set(&statep->imports, name, GaObj_INC_REF(mod));
    return mod;
}

void
ga_mod_import(struct ga_obj *self, struct vm *vm, struct ga_obj *mod)
{
    list_iter_t iter;
    GaHashMap_GetIter(&mod->dict, &iter);
    struct dict_kvp *kvp;

    while (GaIter_Next(&iter, (void**)&kvp)) {
        GaObj_SETATTR(self, vm, kvp->key, (struct ga_obj*)kvp->val);
    }
}