/*
 * object.c - Gallium base object implementation
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
#include <string.h>
#include <gallium/builtins.h>
#include <gallium/dict.h>
#include <gallium/object.h>
#include <gallium/pool.h>
#include <gallium/vm.h>

#ifdef DEBUG_OBJECT_HEAP
struct list *ga_obj_all = NULL;
#endif

static struct pool          ga_obj_pool = {
    .size   =   sizeof(struct ga_obj)
};

static void                 ga_type_destroy(struct ga_obj *);
static struct ga_obj    *   ga_type_typedef_invoke(struct ga_obj *, struct vm *, int, struct ga_obj **);
static struct ga_obj    *   ga_obj_typedef_invoke(struct ga_obj *, struct vm *, int, struct ga_obj **);

struct ga_obj_ops ga_typedef_ops = {
    .destroy    =   ga_type_destroy,
    .invoke     =   ga_type_typedef_invoke
};

struct ga_obj_ops ga_obj_type_ops = {
    .invoke     =   ga_obj_typedef_invoke
};

struct ga_obj   ga_type_type_inst = {
    .ref_count  =   1,
    .type       =   NULL,
    .obj_ops    =   &ga_typedef_ops,
};

struct ga_obj   ga_obj_type_inst = {
    .ref_count  =   1,
    .type       =   &ga_type_type_inst,
    .obj_ops    =   &ga_obj_type_ops,
};

struct ga_obj_statistics ga_obj_stat;

static void
ga_type_destroy(struct ga_obj *self)
{
    free(self->un.statep);
}

static struct ga_obj *
ga_type_typedef_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (argc != 1) {
        vm_raise_exception(vm, ga_argument_error_new("Type() requires at least one argument"));
        return NULL;
    }

    if (args[0]->type != &ga_str_type_inst) {
        vm_raise_exception(vm, ga_type_error_new("Str"));
        return NULL;
    }

    const char *name = ga_str_to_cstring(args[0]);

    return ga_type_new(name);
}

struct ga_obj *
ga_type_new(const char *name)
{
    struct ga_obj *type = ga_obj_new(&ga_type_type_inst, NULL);
    size_t name_len = strlen(name);
    char *name_buf = calloc(name_len, 1);

    strcpy(name_buf, name);
    type->un.statep = name_buf;

    return type;
}

const char *
ga_type_name(struct ga_obj *type)
{
    struct ga_obj *type_inst = ga_obj_super(type, &ga_type_type_inst);
    
    return (const char*)type_inst->un.statep;
}

static struct ga_obj *
ga_obj_typedef_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    struct ga_obj *type = &ga_obj_type_inst;

    if (argc == 1) {
        type = args[0];
    }

    return ga_obj_new(type, NULL);
}

static void
obj_dict_destroy_cb(void *p, void *s)
{
    struct ga_obj *obj = p;

    GAOBJ_DEC_REF(obj);
}

void
ga_obj_destroy(struct ga_obj *self)
{
    struct ga_obj *type = self->type;
    struct ga_obj *super = self->super;

#ifdef DEBUG_OBJECT_HEAP
    list_remove(ga_obj_all, self, NULL, NULL);
#endif

    if (self->weak_refs) {
        struct ga_obj **ref;
        list_iter_t iter;
        list_get_iter(self->weak_refs, &iter);

        while (iter_next_elem(&iter, (void**)&ref)) {
            *ref = &ga_null_inst;
        }
        
        list_destroy(self->weak_refs, NULL, NULL);
    }

    if (self->obj_ops && self->obj_ops->destroy) {
        self->obj_ops->destroy(self);
    }

    dict_fini(&self->dict, obj_dict_destroy_cb, NULL);

    POOL_PUT(&ga_obj_pool, self);

    if (super) {
        GAOBJ_DEC_REF(super);
    }

    GAOBJ_DEC_REF(type);

    ga_obj_stat.obj_count--;
}

struct ga_obj *
ga_obj_new(struct ga_obj *type, struct ga_obj_ops *ops)
{
    if (!type) {
        /* generic object... default to generic object type */
        type = &ga_obj_type_inst;
    }

    struct ga_obj *obj = POOL_GET(&ga_obj_pool);
    obj->type = GAOBJ_INC_REF(type);
    obj->obj_ops = ops;

    ga_obj_stat.obj_count++;

#ifdef DEBUG_OBJECT_HEAP
    if (!ga_obj_all) ga_obj_all = list_new();

    list_append(ga_obj_all, obj);
#endif
    return obj;
}

void
ga_obj_print(struct ga_obj *self, struct vm *vm)
{
    struct ga_obj *str_val;

    str_val = GAOBJ_STR(self, vm);

    if (str_val) {
        GAOBJ_INC_REF(str_val);

        printf("%s\n", ga_str_to_cstring(str_val));

        GAOBJ_DEC_REF(str_val);
    }
}

struct ga_obj *
ga_obj_super(struct ga_obj *self, struct ga_obj *type)
{
    struct ga_obj *super = self;

    while (super) {
        if (super->type == type) {
            return super;
        }

        super = super->super;
    }

    return NULL;
}

bool
ga_obj_instanceof(struct ga_obj *self, struct ga_obj *type)
{
    struct ga_obj *super = self;

    while (super) {
        if (super->type == type) {
            return true;
        }
        
        super = super->super;
    }
    
    return NULL;
}
