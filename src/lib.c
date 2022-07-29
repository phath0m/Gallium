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
#include <stdio.h>
#include <stdlib.h>
#include <gallium.h>
#ifdef GALLIUM_USE_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

#ifdef GALLIUM_USE_EMSCRIPTEN
EMSCRIPTEN_KEEPALIVE
#endif
GaContext *
Ga_New()
{
    GaContext *ctx = calloc(sizeof(GaContext), 1);
    GaObject *builtin_mod = GaMod_OpenBuiltins();
    GaObject *mod = GaModule_New("__default__", NULL, NULL);

    GaModule_Import(mod, NULL, builtin_mod);

    ctx->default_module = GaObj_INC_REF(mod);

    return ctx;
}

#ifdef GALLIUM_USE_EMSCRIPTEN
EMSCRIPTEN_KEEPALIVE
#endif
void
Ga_Close(GaContext *ctx)
{
    GaObj_DEC_REF(ctx->default_module);
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

#ifdef GALLIUM_USE_EMSCRIPTEN
EMSCRIPTEN_KEEPALIVE
#endif
GaObject *
Ga_DoString(GaContext *ctx, const char *source)
{
    struct compiler_state comp_state;
    memset(&comp_state, 0, sizeof(comp_state));
    GaObject *res = Ga_NULL;
    GaObject *code = GaCode_Compile(&comp_state, source);

    if (!code) {
        compiler_explain(&comp_state);
        return NULL;
    } else {
        GaObj_INC_REF(code);
        GaModule_SetConstructor(ctx->default_module, code);
        res = GaObj_XINC_REF(GaObj_INVOKE(ctx->default_module, ctx, 0, NULL));
        GaObj_DEC_REF(code);
    }
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