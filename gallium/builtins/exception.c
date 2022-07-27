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


static void             exception_destroy(struct ga_obj *);
static struct ga_obj *  exception_str(struct ga_obj *, struct vm *);

struct ga_obj_ops exception_ops = {
    .destroy    =   exception_destroy,
    .str        =   exception_str
};

static void
exception_destroy(struct ga_obj *self)
{
    GaStringBuilder_Destroy((struct stringbuf*)self->un.statep);
}

static struct ga_obj *
exception_str(struct ga_obj *self, struct vm *vm)
{
    return ga_str_from_stringbuf((struct stringbuf*)self->un.statep);
}

struct ga_obj *
ga_argument_error_new(const char *text)
{
    struct ga_obj *error = GaObj_New(&attribute_error_typedef_inst, &exception_ops);
    struct stringbuf *sb = GaStringBuilder_New();

    GaStringBuilder_Append(sb, "ArgumentError: ");
    GaStringBuilder_Append(sb, text);

    error->un.statep = sb;

    return error;
}

struct ga_obj *
ga_attribute_error_new(const char *attrname)
{
    struct ga_obj *error = GaObj_New(&attribute_error_typedef_inst, &exception_ops);
    struct stringbuf *sb = GaStringBuilder_New();

    GaStringBuilder_Append(sb, "AttributeError: No such attribute '");
    GaStringBuilder_Append(sb, attrname);
    GaStringBuilder_Append(sb, "'");

    error->un.statep = sb;

    return error;
}

struct ga_obj *
ga_io_error_new(const char *text)
{
    struct ga_obj *error = GaObj_New(&attribute_error_typedef_inst, &exception_ops);
    struct stringbuf *sb = GaStringBuilder_New();

    GaStringBuilder_Append(sb, "IOError: ");
    GaStringBuilder_Append(sb, text);

    error->un.statep = sb;

    return error;
}

struct ga_obj *
ga_import_error_new(const char *text)
{
    struct ga_obj *error = GaObj_New(&attribute_error_typedef_inst, &exception_ops);
    struct stringbuf *sb = GaStringBuilder_New();

    GaStringBuilder_Append(sb, "ImportError: ");
    GaStringBuilder_Append(sb, text);

    error->un.statep = sb;

    return error;
}

struct ga_obj *
ga_index_error_new(const char *text)
{
    struct ga_obj *error = GaObj_New(&index_error_typedef_inst, &exception_ops);
    struct stringbuf *sb = GaStringBuilder_New();

    GaStringBuilder_Append(sb, "IndexError: ");
    GaStringBuilder_Append(sb, text);

    error->un.statep = sb;

    return error;
}

struct ga_obj *
ga_internal_error_new(const char *text)
{
    struct ga_obj *error = GaObj_New(&index_error_typedef_inst, &exception_ops);
    struct stringbuf *sb = GaStringBuilder_New();

    GaStringBuilder_Append(sb, "InternalError: ");
    GaStringBuilder_Append(sb, text);

    error->un.statep = sb;

    return error;
}

struct ga_obj *
ga_key_error_new()
{
    struct ga_obj *error = GaObj_New(&key_error_typedef_inst, &exception_ops);
    struct stringbuf *sb = GaStringBuilder_New();

    GaStringBuilder_Append(sb, "KeyError: The key does not exist");

    error->un.statep = sb;

    return error;
}

struct ga_obj *
ga_name_error_new(const char *name)
{
    struct ga_obj *error = GaObj_New(&name_error_typedef_inst, &exception_ops);
    struct stringbuf *sb = GaStringBuilder_New();

    GaStringBuilder_Append(sb, "NameError: No such name '");
    GaStringBuilder_Append(sb, name);
    GaStringBuilder_Append(sb, "'");

    error->un.statep = sb;

    return error;
}

struct ga_obj *
ga_operator_error_new(const char *text)
{
    struct ga_obj *error = GaObj_New(&attribute_error_typedef_inst, &exception_ops);
    struct stringbuf *sb = GaStringBuilder_New();

    GaStringBuilder_Append(sb, "OperatorError: ");
    GaStringBuilder_Append(sb, text);

    error->un.statep = sb;

    return error;
}

struct ga_obj *
ga_type_error_new(const char *typename)
{
    struct ga_obj *error = GaObj_New(&type_error_typedef_inst, &exception_ops);
    struct stringbuf *sb = GaStringBuilder_New();

    GaStringBuilder_Append(sb, "TypeError: Expected type '");
    GaStringBuilder_Append(sb, typename);
    GaStringBuilder_Append(sb, "'");

    error->un.statep = sb;

    return error;
}

struct ga_obj *
ga_value_error_new(const char *msg)
{
    struct ga_obj *error = GaObj_New(&type_error_typedef_inst, &exception_ops);
    struct stringbuf *sb = GaStringBuilder_New();

    GaStringBuilder_Append(sb, "ValueError: ");
    GaStringBuilder_Append(sb, msg);

    error->un.statep = sb;

    return error;
}

struct ga_obj *
ga_syntax_error_new(const char *msg)
{
    struct ga_obj *error = GaObj_New(&type_error_typedef_inst, &exception_ops);
    struct stringbuf *sb = GaStringBuilder_New();

    GaStringBuilder_Append(sb, "SyntaxError: ");
    GaStringBuilder_Append(sb, msg);

    error->un.statep = sb;

    return error;
}

