#include <stdlib.h>
#include <gallium/builtins.h>
#include <gallium/object.h>
#include <gallium/stringbuf.h>

GA_BUILTIN_TYPE_DECL(exception_typedef_inst, "Exception", NULL);
GA_BUILTIN_TYPE_DECL(argument_error_typedef_inst, "ArgumentError", NULL);
GA_BUILTIN_TYPE_DECL(attribute_error_typedef_inst, "AttributeError", NULL);
GA_BUILTIN_TYPE_DECL(index_error_typedef_inst, "IndexError", NULL);
GA_BUILTIN_TYPE_DECL(key_error_typedef_inst, "KeyError", NULL);
GA_BUILTIN_TYPE_DECL(name_error_typedef_inst, "NameError", NULL);
GA_BUILTIN_TYPE_DECL(type_error_typedef_inst, "TypeError", NULL);
GA_BUILTIN_TYPE_DECL(value_error_typedef_inst, "ValueError", NULL);

static void             exception_destroy(struct ga_obj *);
static struct ga_obj *  exception_str(struct ga_obj *, struct vm *);

struct ga_obj_ops exception_ops = {
    .destroy    =   exception_destroy,
    .str        =   exception_str
};

static void
exception_destroy(struct ga_obj *self)
{
    stringbuf_destroy((struct stringbuf*)self->un.statep);
}

static struct ga_obj *
exception_str(struct ga_obj *self, struct vm *vm)
{
    return ga_str_from_stringbuf((struct stringbuf*)self->un.statep);
}

struct ga_obj *
ga_argument_error_new(const char *text)
{
    struct ga_obj *error = ga_obj_new(&attribute_error_typedef_inst, &exception_ops);
    struct stringbuf *sb = stringbuf_new();

    stringbuf_append(sb, "ArgumentError: ");
    stringbuf_append(sb, text);

    error->un.statep = sb;

    return error;
}

struct ga_obj *
ga_attribute_error_new(const char *attrname)
{
    struct ga_obj *error = ga_obj_new(&attribute_error_typedef_inst, &exception_ops);
    struct stringbuf *sb = stringbuf_new();

    stringbuf_append(sb, "AttributeError: No such attribute '");
    stringbuf_append(sb, attrname);
    stringbuf_append(sb, "'");

    error->un.statep = sb;

    return error;
}

struct ga_obj *
ga_index_error_new(const char *text)
{
    struct ga_obj *error = ga_obj_new(&index_error_typedef_inst, &exception_ops);
    struct stringbuf *sb = stringbuf_new();

    stringbuf_append(sb, "IndexError: ");
    stringbuf_append(sb, text);

    error->un.statep = sb;

    return error;
}

struct ga_obj *
ga_key_error_new()
{
    struct ga_obj *error = ga_obj_new(&key_error_typedef_inst, &exception_ops);
    struct stringbuf *sb = stringbuf_new();

    stringbuf_append(sb, "KeyError: The key does not exist");

    error->un.statep = sb;

    return error;
}

struct ga_obj *
ga_name_error_new(const char *name)
{
    struct ga_obj *error = ga_obj_new(&name_error_typedef_inst, &exception_ops);
    struct stringbuf *sb = stringbuf_new();

    stringbuf_append(sb, "NameError: No such name '");
    stringbuf_append(sb, name);
    stringbuf_append(sb, "'");

    error->un.statep = sb;

    return error;
}

struct ga_obj *
ga_type_error_new(const char *typename)
{
    struct ga_obj *error = ga_obj_new(&type_error_typedef_inst, &exception_ops);
    struct stringbuf *sb = stringbuf_new();

    stringbuf_append(sb, "TypeError: Expected type '");
    stringbuf_append(sb, typename);
    stringbuf_append(sb, "'");

    error->un.statep = sb;

    return error;
}

struct ga_obj *
ga_value_error_new(const char *msg)
{
    struct ga_obj *error = ga_obj_new(&type_error_typedef_inst, &exception_ops);
    struct stringbuf *sb = stringbuf_new();

    stringbuf_append(sb, "ValueError: ");
    stringbuf_append(sb, msg);

    error->un.statep = sb;

    return error;
}
