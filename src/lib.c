/*
 * lib.c - Gallium library functions
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
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <gallium.h>
#include "compiler.h"
#ifdef GALLIUM_USE_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

#ifdef GALLIUM_USE_EMSCRIPTEN
EMSCRIPTEN_KEEPALIVE
#endif
GaContext *
Ga_New()
{
    /*
     * Initialize new types. These need to be initialized explicitly as
     * these type definitions must be objects on the heap.
     * 
     * This is because they contain method objects. I realize that this
     * could be done in a better way, or, with global constructors but I
     * don't like the portability of that
     */
    _GaAst_init();
    _GaCode_init();
    _GaFile_init();
    _GaList_init();
    _GaMutStr_init();
    _GaParser_init();
    _GaStr_init();

    GaContext *ctx = calloc(sizeof(GaContext), 1);
    GaObject *builtins = GaMod_OpenBuiltins();
    GaObject *mod = GaModule_New("__default__", NULL, NULL);

    _Ga_hashmap_set(&ctx->import_cache, "__builtins__",
                    GaObj_INC_REF(builtins));

    GaModule_Import(mod, NULL, builtins);

    ctx->globals = GaObj_INC_REF(builtins);
    ctx->default_module = GaObj_INC_REF(mod);

    return ctx;
}

static void
dict_destroy_cb(void *p, void *s)
{
    GaObject *obj = p;
    GaObj_DEC_REF(obj);
}

#ifdef GALLIUM_USE_EMSCRIPTEN
EMSCRIPTEN_KEEPALIVE
#endif
void
Ga_Close(GaContext *ctx)
{
    /* Dispose of any imported modules */
    _Ga_hashmap_fini(&ctx->import_cache, dict_destroy_cb, NULL);
    /* Dispose of the default module */
    GaObj_DEC_REF(ctx->default_module);
    /* Dispose of the "globals" */
    GaObj_DEC_REF(ctx->globals);
    /* Free the built in types... */
    _GaAst_fini();
    _GaCode_fini();
    _GaFile_fini();
    _GaList_fini();
    _GaParser_fini();
    _GaMutStr_fini();
    _GaStr_fini();
    /* bye */
    free(ctx);
}

#ifdef GALLIUM_USE_EMSCRIPTEN
EMSCRIPTEN_KEEPALIVE
#endif
void
Ga_SetGlobal(GaContext *ctx, const char *name, GaObject *val)
{
    GaObj_SETATTR(ctx->default_module, ctx, name, val);
}

GaObject *
Ga_GetError(GaContext *ctx)
{
    return ctx->error;
}

void
Ga_ClearError(GaContext *ctx)
{
    GaObj_XDEC_REF(ctx->error);
    ctx->error = NULL;
}

#ifdef GALLIUM_USE_EMSCRIPTEN
EMSCRIPTEN_KEEPALIVE
#endif
GaObject *
Ga_DoString(GaContext *ctx, const char *source)
{
    struct compiler_state comp_state;
    memset(&comp_state, 0, sizeof(comp_state));
    GaObject *res = Ga_NULL;
    GaObject *code = GaCode_Compile(ctx, source);

    if (ctx->error) {
        return NULL;
    }

    assert(code != NULL);

    GaObj_INC_REF(code);
    GaModule_SetConstructor(ctx->default_module, code);
    res = GaObj_XINC_REF(GaObj_INVOKE(ctx->default_module, ctx, 0, NULL));
    GaObj_DEC_REF(code);

    return GaObj_XMOVE_REF(res);
}

#ifdef GALLIUM_USE_EMSCRIPTEN
EMSCRIPTEN_KEEPALIVE
#endif
GaObject *
Ga_DoFile(GaContext *ctx, const char *file)
{
    FILE *fp = fopen(file, "r");

    if (!fp) {
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    size_t fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *src = calloc(fsize + 1, 1);

    if (fread(src, 1, fsize, fp) != fsize) {
        fclose(fp);
        return NULL;
    }

    fclose(fp);

    GaObject *res = Ga_DoString(ctx, src);

    free(src);

    return res;
}