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
#ifdef GALLIUM_USE_READLINE
#include <readline/readline.h>
#endif
#ifdef GALLIUM_USE_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

#ifdef GALLIUM_USE_EMSCRIPTEN
EMSCRIPTEN_KEEPALIVE
#endif
int
gallium_eval(const char *file, const char *src)
{
    struct compiler_state comp_state;
    
    memset(&comp_state, 0, sizeof(comp_state));

    GaObject *builtin_mod = GaObj_INC_REF(GaMod_OpenBuiltins());
    GaObject *code = GaCode_Compile(&comp_state, src);

    if (!code) {
        compiler_explain(&comp_state);
        return -1;
    }

    GaObject *mod = GaObj_INC_REF(GaModule_New("__default__", code, file));

    GaModule_Import(mod, NULL, builtin_mod);

    struct vm vm;
    memset(&vm, 0, sizeof(vm));

    GaObj_INVOKE(mod, &vm, 0, NULL);

    fflush(stdout);

    GaObj_DEC_REF(mod);
    GaObj_DEC_REF(builtin_mod);

    return 0;
}

#ifndef GALLIUM_TARGET_LIBRARY
static void
repl()
{
    bool sentinel;

    struct compiler_state comp_state;
    struct vm vm;
    GaObject *builtin_mod;
    GaObject *code;
    GaObject *mod;
    GaObject *res;
    struct stackframe *frame;

    builtin_mod = GaMod_OpenBuiltins();
    mod = GaModule_New("__default__", NULL, NULL);

    frame = GaFrame_NEW(mod, NULL, NULL);
    frame->interrupt_flag_ptr = &sentinel;

    memset(&vm, 0, sizeof(vm));
    memset(&comp_state, 0, sizeof(comp_state));

    vm.top = frame;
    sentinel = false;

    GaModule_Import(GaObj_INC_REF(mod), NULL, builtin_mod);

    for (;;) {
#if GALLIUM_USE_READLINE
        char *line = readline(">>> ");
#else
        char line[512];
        fprintf(stdout, ">>> ");
        fflush(stdout);
        if (fgets(line, sizeof(line), stdin) <= 0) {
            continue;
        }
#endif
        code = GaCode_Compile(&comp_state, line);

        if (!code) {
            compiler_explain(&comp_state);
        } else {
            GaObj_INC_REF(code);
            res = GaObj_INVOKE(code, &vm, 0, NULL);
            if (res && res != Ga_NULL) GaObj_Print(res, &vm);
            GaObj_DEC_REF(code);
        }
#if GALLIUM_USE_READLINE
        free(line);
#endif
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

    /* Some debugging logic here to detect issues with Gallium's reference counting */
    int pre_exec_obj_count = ga_obj_stat.obj_count;

    gallium_eval(argv[1], src);

    if (ga_obj_stat.obj_count != pre_exec_obj_count) {
        printf("DEBUG: Memory leak! detected %d undisposed objects!\n", ga_obj_stat.obj_count);
    }

#ifdef DEBUG_OBJECT_HEAP
    extern struct list *ga_obj_all;
    GaObject *obj = NULL;
    list_iter_t iter;
    list_get_iter(ga_obj_all, &iter);

    while (iter_next_elem(&iter, (void**)&obj)) {
        printf("DEBUG: Object <%s:0x%p> remains with %d references!\n", (char*)obj->type->un.statep, obj, obj->ref_count);
    }
#endif

    return 0;
}
#endif