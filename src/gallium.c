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
#include <gallium.h>
#include "config.h"
#ifdef HAVE_LIBREADLINE
# include <readline/readline.h>
#endif

static bool sentinel = true;

static GaObject *
quit(GaContext *ctx, int argc, GaObject **args)
{
    sentinel = false;
    return Ga_NULL;
}

static void
repl(GaContext *ctx)
{
    Ga_SetGlobal(ctx, "quit", GaBuiltin_New(quit, NULL));

    do {
#ifdef HAVE_LIBREADLINE
        char *line = readline(">>> ");
#else
        char line[512];
        fprintf(stdout, ">>> ");
        fflush(stdout);
        if (fgets(line, sizeof(line), stdin) <= 0) {
            continue;
        }
#endif
        GaObject *res = GaObj_XINC_REF(Ga_DoString(ctx, line));
        GaObject *err = Ga_GetError(ctx);
        if (err) {
            GaObj_Print(err, ctx);
            Ga_ClearError(ctx);
        }
        else if (res && res != Ga_NULL) GaObj_Print(res, ctx);
        GaObj_XDEC_REF(res);
#ifdef HAVE_LIBREADLINE
        free(line);
#endif
    } while (sentinel);
}

int
main(int argc, const char *argv[])
{
    /* Keeping track of this to identify bugs in Gallium's memory managemnt. */
#ifdef ENABLE_MEMORY_DEBUG
    int pre_exec_obj_count = ga_obj_stat.obj_count;
#endif

    GaContext *ctx = Ga_New();

    if (argc == 1) {
        repl(ctx);
        return -1;
    }

    if (argc != 2) {
        fprintf(stderr, "usage: %s <file>\n", argv[0]);
        return -1;
    }

    GaObject *res = Ga_DoFile(ctx, argv[1]);
    GaObject *err = Ga_GetError(ctx);
    GaObj_XINC_REF(res);

    if (err) {
        GaObj_Print(err, ctx);
        Ga_ClearError(ctx);
    }

    GaObj_XDEC_REF(res);
    Ga_Close(ctx);
 
#ifdef ENABLE_MEMORY_DEBUG
    puts("MEMORY USAGE STATISTICS");
    puts("=======================");
    printf("Undisposed objects: %d\n", ga_obj_stat.obj_count);
    printf("Global objects:     %d\n", pre_exec_obj_count);
    puts("HEAP TRACE");
    puts("=======================");
    extern _Ga_list_t *ga_obj_all;
    GaObject *obj = NULL;
    _Ga_iter_t iter;
    _Ga_LIST_GET_ITER(ga_obj_all, &iter);

    while (_Ga_iter_next(&iter, (void**)&obj)) {
        printf("DEBUG: Object <%s:0x%p> remains with %d references!\n", (char*)obj->type->un.statep, obj, obj->ref_count);
    }
#endif

    return 0;
}