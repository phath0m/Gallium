#include <stdlib.h>
#include <string.h>
#include <gallium/builtins.h>
#include <gallium/object.h>
#include <gallium/list.h>

static struct ga_obj    *   ga_weakref_type_invoke(struct ga_obj *, struct vm *, int, struct ga_obj **);

GA_BUILTIN_TYPE_DECL(ga_weakref_type_inst, "WeakRef", ga_weakref_type_invoke);

static void                 ga_weakref_destroy(struct ga_obj *);
static struct ga_obj    *   ga_weakref_getattr(struct ga_obj *, struct vm *, const char *);

struct ga_obj_ops ga_weakref_ops = {
    .destroy    =   ga_weakref_destroy,
    .getattr    =   ga_weakref_getattr
};

static struct ga_obj *
ga_weakref_type_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        /* exception */
        return NULL;
    }
    
    return ga_weakref_new(args[0]);
}

static void
ga_weakref_destroy(struct ga_obj *self)
{
    struct ga_obj *ref = self->un.statep;

    if (ref != &ga_null_inst) {
        list_remove(ref->weak_refs, &self->un.statep, NULL, NULL);
    }
}

static struct ga_obj *
ga_weakref_getattr(struct ga_obj *self, struct vm *vm, const char *name)
{
    if (strcmp(name, "value") == 0) {
        return (struct ga_obj*)self->un.statep;
    }

    return NULL;
}

struct ga_obj *
ga_weakref_new(struct ga_obj *ref)
{
    struct ga_obj *weak = ga_obj_new(&ga_weakref_type_inst, &ga_weakref_ops);

    if (!ref->weak_refs) {
        ref->weak_refs = list_new();
    }

    weak->un.statep = ref;

    list_append(ref->weak_refs, &weak->un.statep);
    
    return weak;
}

struct ga_obj *
ga_weakref_val(struct ga_obj *self)
{
    struct ga_obj *ref_cell = self->un.statep;

    return ref_cell;
}
