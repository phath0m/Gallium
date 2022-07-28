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
#ifdef GALLIUM_USE_READLINE
#include <readline/readline.h>
#endif

static void
repl(GaContext *ctx)
{
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
        GaObject *res = GaObj_XINC_REF(Ga_DoString(ctx, line));
        if (res && res != Ga_NULL) GaObj_Print(res, ctx);
        GaObj_XDEC_REF(res);

#if GALLIUM_USE_READLINE
        free(line);
#endif
    }
}

int
main(int argc, const char *argv[])
{
    /* Keeping track of this to identify bugs in Gallium's memory managemnt. */
    int pre_exec_obj_count = ga_obj_stat.obj_count;

    GaContext *ctx = Ga_New();

    if (argc == 1) {
        repl(ctx);
        return -1;
    }

    if (argc != 2) {
        fprintf(stderr, "usage: %s <file>\n", argv[0]);
        return -1;
    }

    Ga_DoFile(ctx, argv[1]);
    Ga_Close(ctx);

    if (ga_obj_stat.obj_count != pre_exec_obj_count) {
        printf("DEBUG: Memory leak! detected %d undisposed objects!\n", ga_obj_stat.obj_count);
    }
 
#ifdef DEBUG_OBJECT_HEAP
    extern struct list *ga_obj_all;
    GaObject *obj = NULL;
    list_iter_t iter;
    GaList_GET_ITER(ga_obj_all, &iter);

    while (GaIter_Next(&iter, (void**)&obj)) {
        printf("DEBUG: Object <%s:0x%p> remains with %d references!\n", (char*)obj->type->un.statep, obj, obj->ref_count);
    }
#endif

    return 0;
}