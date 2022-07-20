/*
 * gallium.c - Main entry point
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
#include <stdio.h>
#include <string.h>
#include <gallium/builtins.h>
#include <gallium/compiler.h>
#include <gallium/object.h>
#include <gallium/stringbuf.h>
#include <gallium/vm.h>
#include <readline/readline.h>

static void
repl()
{
    bool sentinel;

    struct compiler_state comp_state;
    struct vm vm;
    struct ga_obj *builtin_mod;
    struct ga_obj *code;
    struct ga_obj *mod;
    struct ga_obj *res;
    struct stackframe *frame;

    builtin_mod = ga_builtin_mod();
    mod = ga_mod_new("__default__", NULL, NULL);

    frame = STACKFRAME_NEW(mod, NULL, NULL);
    frame->interrupt_flag_ptr = &sentinel;

    memset(&vm, 0, sizeof(vm));
    memset(&comp_state, 0, sizeof(comp_state));

    vm.top = frame;
    sentinel = false;

    ga_mod_import(GAOBJ_INC_REF(mod), NULL, builtin_mod);

    for (;;) {
        char *line = readline(">>> ");

        code = compiler_compile(&comp_state, line);

        if (!code) {
            compiler_explain(&comp_state);
        } else {
            GAOBJ_INC_REF(code);
            res = GAOBJ_INVOKE(code, &vm, 0, NULL);
            if (res && res != GA_NULL) ga_obj_print(res, &vm);
            GAOBJ_DEC_REF(code);
        }
        free(line);
    }
}

int
main(int argc, const char *argv[])
{
    if (argc == 1) {
        repl();
        return -1;
    }

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

    struct ga_obj *builtin_mod = GAOBJ_INC_REF(ga_builtin_mod());
    struct ga_obj *code = compiler_compile(&comp_state, src);

    if (!code) {
        compiler_explain(&comp_state);
        return -1;
    }

    struct ga_obj *mod = GAOBJ_INC_REF(ga_mod_new("__default__", code, argv[1]));

    ga_mod_import(mod, NULL, builtin_mod);

    struct vm vm;
    memset(&vm, 0, sizeof(vm));

    GAOBJ_INVOKE(mod, &vm, 0, NULL);

    fflush(stdout);

    GAOBJ_DEC_REF(mod);
    GAOBJ_DEC_REF(builtin_mod);

    if (ga_obj_stat.obj_count != 0) {
        printf("DEBUG: Memory leak! detected %d undisposed objects!\n", ga_obj_stat.obj_count);
    }

#ifdef DEBUG_OBJECT_HEAP
    extern struct list *ga_obj_all;
    struct ga_obj *obj = NULL;
    list_iter_t iter;
    list_get_iter(ga_obj_all, &iter);

    while (iter_next_elem(&iter, (void**)&obj)) {
        printf("DEBUG: Object <%s:0x%p> remains with %d references!\n", (char*)obj->type->un.statep, obj, obj->ref_count);
    }
#endif

    return 0;
}
