#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gallium/builtins.h>
#include <gallium/compiler.h>
#include <gallium/dict.h>
#include <gallium/object.h>
#include <gallium/vm.h>

typedef struct ga_obj *(*mod_open_func)();

struct builtin_mod_def {
    const char  *   name;
    mod_open_func   func;
};

struct builtin_mod_def builtin_mods[] = {
    {"gallium/ast", ga_ast_mod_open},
    {"gallium/parser", ga_parser_mod_open},
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
    char                    name[];
};

static void
mod_dict_destroy_cb(void *p, void *s)
{
    struct ga_obj *obj = p;

    GAOBJ_DEC_REF(obj);
}

static void
ga_mod_destroy(struct ga_obj *self)
{
    struct ga_mod_state *statep = self->un.statep;

    if (statep->code) {
        GAOBJ_DEC_REF(statep->code);
        
        dict_fini(&statep->imports, mod_dict_destroy_cb, NULL);
    }

    free(statep);
}

static struct ga_obj *
ga_mod_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    struct ga_mod_state *statep = self->un.statep;

    
    return vm_eval_frame(vm, STACKFRAME_NEW(self, statep->constructor, vm->top), 0, NULL);
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

    struct ga_obj *code = compiler_compile(&comp_state, src);

    free(src);

    if (!code) {
        /* raise syntax exception */
        return NULL;
    }

    struct ga_obj *mod = GAOBJ_INC_REF(ga_mod_new("__default__", code));

    ga_mod_import(mod, NULL, ga_builtin_mod());

    GAOBJ_INVOKE(mod, vm, 0, NULL);

    return GAOBJ_MOVE_REF(mod);
}

static bool
ga_mod_search_in_path(const char *name, char *resolved)
{
    if (!strncmp(name, "/", 1) || !strncmp(name, "./", 2)) {
        strcpy(resolved, name);
        return true;
    }

    char *path = getenv("GALLIUM_PATH");

    if (!path) {
        return false;
    }

    static char path_copy[1024];

    strncpy(path_copy, path, 1024);

    char *searchdir = strtok(path_copy, ":");

    while (searchdir != NULL) {
        bool succ = false;

        DIR *dirp = opendir(searchdir);

        if (!dirp) {
            searchdir = strtok(NULL, ":");
            continue;
        }

        struct dirent *dirent;

        while ((dirent = readdir(dirp))) {
            if (strcmp(dirent->d_name, name) == 0) {
                sprintf(resolved, "%s/%s", searchdir, name);
                succ = true;
                break;
            }
        }

        closedir(dirp);

        if (succ) {
            return true;
        }

        searchdir = strtok(NULL, ":");
    }

    return false;
}
 
struct ga_obj *
ga_mod_new(const char *name, struct ga_obj *code)
{
    size_t name_len = strlen(name);
    struct ga_mod_state *statep = calloc(sizeof(struct ga_mod_state) + name_len, 1);
    struct ga_obj *mod = ga_obj_new(&ga_mod_typedef_inst, &ga_mod_ops);
    strcpy(statep->name, name);

    if (code) {
        struct ga_proc *constructor = ga_code_get_proc(code);
        statep->constructor = constructor;
        statep->code = GAOBJ_INC_REF(code);
    }

    mod->un.statep = statep;

    return mod;
}

struct ga_obj *
ga_mod_open(struct ga_obj *self, struct vm *vm, const char *name)
{
    struct ga_mod_state *statep = self->un.statep;
    struct ga_obj *mod = NULL;

    if (dict_get(&statep->imports, name, (void**)&mod)) {
        return mod;
    }

    char name_with_ext[PATH_MAX+1];

    sprintf(name_with_ext, "%s.ga", name);

    char full_path[PATH_MAX+1];

    if (ga_mod_search_in_path(name_with_ext, full_path)) {
        struct ga_obj *mod = ga_mod_import_source(vm, full_path);

        return mod;
    }

    struct builtin_mod_def *def = &builtin_mods[0];

    while (def->name) {
        if (strcmp(def->name, name) == 0) {
            mod = def->func();
            break;
        }

        def++;
    }

    if (!mod) {
        vm_raise_exception(vm, ga_import_error_new(name));
        return NULL;
    }

    dict_set(&statep->imports, name, GAOBJ_INC_REF(mod));

    return mod;
}

void
ga_mod_import(struct ga_obj *self, struct vm *vm, struct ga_obj *mod)
{
    list_iter_t iter;
    dict_get_iter(&mod->dict, &iter);

    struct dict_kvp *kvp;

    while (iter_next_elem(&iter, (void**)&kvp)) {
        GAOBJ_SETATTR(self, vm, kvp->key, (struct ga_obj*)kvp->val);
    }
}
