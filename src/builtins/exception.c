/*
 * exception.c - Gallium's builtin exception type.
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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <gallium/builtins.h>
#include <gallium/object.h>
#include <gallium/stringbuf.h>

GA_BUILTIN_TYPE_DECL(exception_typedef_inst, "Exception", NULL);
GA_BUILTIN_TYPE_DECL(argument_error_typedef_inst, "ArgumentError", NULL);
GA_BUILTIN_TYPE_DECL(attribute_error_typedef_inst, "AttributeError", NULL);
GA_BUILTIN_TYPE_DECL(io_error_typedef_inst, "IOError", NULL);
GA_BUILTIN_TYPE_DECL(import_error_typedef_inst, "ImportError", NULL);
GA_BUILTIN_TYPE_DECL(index_error_typedef_inst, "IndexError", NULL);
GA_BUILTIN_TYPE_DECL(internal_error_typedef_inst, "InternalError", NULL);
GA_BUILTIN_TYPE_DECL(key_error_typedef_inst, "KeyError", NULL);
GA_BUILTIN_TYPE_DECL(name_error_typedef_inst, "NameError", NULL);
GA_BUILTIN_TYPE_DECL(operator_error_type_inst, "OperatorError", NULL);
GA_BUILTIN_TYPE_DECL(type_error_typedef_inst, "TypeError", NULL);
GA_BUILTIN_TYPE_DECL(value_error_typedef_inst, "ValueError", NULL);
GA_BUILTIN_TYPE_DECL(syntax_error_typedef_inst, "SyntaxError", NULL);

static void         exception_destroy(GaObject *);
static GaObject *   exception_str(GaObject *, GaContext *);

static struct ga_obj_ops exception_ops = {
    .destroy    =   exception_destroy,
    .str        =   exception_str
};

static void
exception_destroy(GaObject *self)
{
    GaStringBuilder_Destroy((struct stringbuf*)self->un.statep);
}

static GaObject *
exception_str(GaObject *self, GaContext *vm)
{
    return GaStr_FromStringBuilder((struct stringbuf*)self->un.statep);
}

static void
format_vargs(struct stringbuf *sb, const char *fmt, va_list arg)
{
    char msg[512];
    vsnprintf(msg, sizeof(msg), fmt, arg);
    GaStringBuilder_Append(sb, msg);
}

GaObject *
GaErr_NewArgumentError(const char *fmt, ...)
{

    GaObject *error = GaObj_New(&attribute_error_typedef_inst, &exception_ops);
    struct stringbuf *sb = GaStringBuilder_New();

    GaStringBuilder_Append(sb, "ArgumentError: ");
    va_list vlist;
    va_start(vlist, fmt);
    format_vargs(sb, fmt, vlist);
    va_end(vlist);

    error->un.statep = sb;

    return error;
}

GaObject *
GaErr_NewAttributeError(const char *attrname)
{
    GaObject *error = GaObj_New(&attribute_error_typedef_inst, &exception_ops);
    struct stringbuf *sb = GaStringBuilder_New();

    GaStringBuilder_Append(sb, "AttributeError: No such attribute '");
    GaStringBuilder_Append(sb, attrname);
    GaStringBuilder_Append(sb, "'");

    error->un.statep = sb;

    return error;
}

GaObject *
GaErr_NewIOError(const char *text)
{
    GaObject *error = GaObj_New(&attribute_error_typedef_inst, &exception_ops);
    struct stringbuf *sb = GaStringBuilder_New();

    GaStringBuilder_Append(sb, "IOError: ");
    GaStringBuilder_Append(sb, text);

    error->un.statep = sb;

    return error;
}

GaObject *
GaErr_NewImportError(const char *text)
{
    GaObject *error = GaObj_New(&attribute_error_typedef_inst, &exception_ops);
    struct stringbuf *sb = GaStringBuilder_New();

    GaStringBuilder_Append(sb, "ImportError: ");
    GaStringBuilder_Append(sb, text);

    error->un.statep = sb;

    return error;
}

GaObject *
GaErr_NewIndexError(const char *text)
{
    GaObject *error = GaObj_New(&index_error_typedef_inst, &exception_ops);
    struct stringbuf *sb = GaStringBuilder_New();

    GaStringBuilder_Append(sb, "IndexError: ");
    GaStringBuilder_Append(sb, text);

    error->un.statep = sb;

    return error;
}

GaObject *
GaErr_NewInternalError(const char *text)
{
    GaObject *error = GaObj_New(&index_error_typedef_inst, &exception_ops);
    struct stringbuf *sb = GaStringBuilder_New();

    GaStringBuilder_Append(sb, "InternalError: ");
    GaStringBuilder_Append(sb, text);

    error->un.statep = sb;

    return error;
}

GaObject *
GaErr_NewKeyError()
{
    GaObject *error = GaObj_New(&key_error_typedef_inst, &exception_ops);
    struct stringbuf *sb = GaStringBuilder_New();

    GaStringBuilder_Append(sb, "KeyError: The key does not exist");

    error->un.statep = sb;

    return error;
}

GaObject *
GaErr_NewNameError(const char *name)
{
    GaObject *error = GaObj_New(&name_error_typedef_inst, &exception_ops);
    struct stringbuf *sb = GaStringBuilder_New();

    GaStringBuilder_Append(sb, "NameError: No such name '");
    GaStringBuilder_Append(sb, name);
    GaStringBuilder_Append(sb, "'");

    error->un.statep = sb;

    return error;
}

GaObject *
GaErr_NewOperatorError(const char *text)
{
    GaObject *error = GaObj_New(&attribute_error_typedef_inst, &exception_ops);
    struct stringbuf *sb = GaStringBuilder_New();

    GaStringBuilder_Append(sb, "OperatorError: ");
    GaStringBuilder_Append(sb, text);

    error->un.statep = sb;

    return error;
}

GaObject *
GaErr_NewTypeError(const char *fmt, ...)
{
    GaObject *error = GaObj_New(&type_error_typedef_inst, &exception_ops);
    struct stringbuf *sb = GaStringBuilder_New();

    GaStringBuilder_Append(sb, "TypeError: ");
    va_list vlist;
    va_start(vlist, fmt);
    format_vargs(sb, fmt, vlist);
    va_end(vlist);

    error->un.statep = sb;

    return error;
}

GaObject *
GaErr_NewValueError(const char *msg)
{
    GaObject *error = GaObj_New(&type_error_typedef_inst, &exception_ops);
    struct stringbuf *sb = GaStringBuilder_New();

    GaStringBuilder_Append(sb, "ValueError: ");
    GaStringBuilder_Append(sb, msg);

    error->un.statep = sb;

    return error;
}

GaObject *
GaErr_NewSyntaxError(const char *fmt, ...)
{
    GaObject *error = GaObj_New(&type_error_typedef_inst, &exception_ops);
    struct stringbuf *sb = GaStringBuilder_New();
    GaStringBuilder_Append(sb, "SyntaxError: ");
    va_list vlist;
    va_start(vlist, fmt);
    format_vargs(sb, fmt, vlist);
    va_end(vlist);
    error->un.statep = sb;
    return error;
}