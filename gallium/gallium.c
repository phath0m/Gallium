#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gallium/builtins.h>
#include <gallium/compiler.h>
#include <gallium/object.h>
#include <gallium/stringbuf.h>
#include <gallium/vm.h>

int
main(int argc, const char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <file>\n", argv[0]);
        return -1;
    }

    FILE *fp = fopen(argv[1], "r");

    if (!fp) {
        perror("gallium: ");
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    size_t fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *src = calloc(fsize + 1, 1);

    if (fread(src, 1, fsize, fp) != fsize) {
        fputs("error reading\n", stdout);
        fclose(fp);
    }

    fclose(fp);

    struct compiler_state comp_state;
    
    memset(&comp_state, 0, sizeof(comp_state));

    struct ga_obj *builtin_mod = ga_builtin_mod();
    
    int pre_exec_obj_count = ga_obj_stat.obj_count;

    struct ga_obj *code = compiler_compile(&comp_state, src);

    if (!code) {
        compiler_explain(&comp_state);
        return -1;
    }

    struct ga_obj *mod = ga_mod_new("__default__", code);

    ga_mod_import(mod, NULL, builtin_mod);

    struct vm vm;
    memset(&vm, 0, sizeof(vm));

    GAOBJ_INVOKE(mod, &vm, 0, NULL);

    if (ga_obj_stat.obj_count != pre_exec_obj_count) {
        printf("DEBUG: Memory leak! detected %d undisposed objects!\n", ga_obj_stat.obj_count - pre_exec_obj_count);
    }

#ifdef DEBUG_OBJECT_HEAP
    extern struct list *ga_obj_all;
    struct ga_obj *obj = NULL;
    list_iter_t iter;
    list_get_iter(ga_obj_all, &iter);

    while (iter_next_elem(&iter, (void**)&obj)) {
        printf("DEBUG: Object <%s:0x%p> remains!\n", (char*)obj->type->un.statep, obj);
    }
#endif

    return 0;
}
