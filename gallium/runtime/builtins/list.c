#include <stdlib.h>
#include <gallium/builtins.h>
#include <gallium/object.h>
#include <gallium/vm.h>

#define GA_LIST_INITIAL_SIZE    32

static struct ga_obj    *   ga_list_type_invoke(struct ga_obj *, struct vm *, int, struct ga_obj**);

GA_BUILTIN_TYPE_DECL(ga_list_type_inst, "List", ga_list_type_invoke);
GA_BUILTIN_TYPE_DECL(ga_list_iter_type_inst, "ListIter", NULL);

static void                 ga_list_destroy(struct ga_obj *);
static struct ga_obj    *   ga_list_iter(struct ga_obj *, struct vm *);
static struct ga_obj    *   ga_list_getindex(struct ga_obj *, struct vm *, struct ga_obj *);
static void                 ga_list_setindex(struct ga_obj *, struct vm *, struct ga_obj *, struct ga_obj *);
static struct ga_obj    *   ga_list_len(struct ga_obj *, struct vm *);

struct ga_obj_ops   list_obj_ops = {
    .destroy    =   ga_list_destroy,
    .iter       =   ga_list_iter,
    .getindex   =   ga_list_getindex,
    .setindex   =   ga_list_setindex,
    .len        =   ga_list_len
};

struct list_state {
    int                 used_cells;
    int                 avail_cells;
    struct ga_obj   **  cells;
};

static void                 ga_list_iter_destroy(struct ga_obj *);
static bool                 ga_list_iter_next(struct ga_obj *, struct vm *);
static struct ga_obj    *   ga_list_iter_cur(struct ga_obj *, struct vm *);

struct ga_obj_ops   list_iter_obj_ops = {
    .destroy    =   ga_list_iter_destroy,
    .iter_next  =   ga_list_iter_next,
    .iter_cur   =   ga_list_iter_cur,
};

struct list_iter_state {
    struct ga_obj   *   listp;
    int                 index; 
};

static struct ga_obj *
ga_list_type_invoke(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    return ga_list_new();
}

static struct ga_obj *
ga_list_append_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    for (int i = 0; i < argc; i++) {
        ga_list_append(self, args[i]);
    }

    return &ga_null_inst;
}

static struct ga_obj *
ga_list_remove_method(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    for (int i = 0; i < argc; i++) {
        ga_list_remove(self, vm, args[i]);
    }

    return &ga_null_inst;
}

static void
ga_list_destroy(struct ga_obj *self)
{
    struct list_state *statep = self->un.statep;

    for (int i = 0; i < statep->used_cells; i++) {
        GAOBJ_DEC_REF(statep->cells[i]);
    }

    free(statep->cells);
    free(statep);
}

static struct ga_obj *
ga_list_getindex(struct ga_obj *self, struct vm *vm, struct ga_obj *key)
{
    struct list_state *statep = self->un.statep;
    struct ga_obj *key_int = ga_obj_super(key, &ga_int_type_inst);

    if (!key_int) {
        vm_raise_exception(vm, ga_type_error_new("Int"));
        return NULL;
    }

    uint32_t index = (uint32_t)ga_int_to_i64(key_int);

    if (index < statep->used_cells) {
        return statep->cells[index];
    }

    vm_raise_exception(vm, ga_index_error_new("Index out of range"));
    return NULL;
}

static void
ga_list_setindex(struct ga_obj *self, struct vm *vm, struct ga_obj *key, struct ga_obj *val)
{
    struct list_state *statep = self->un.statep;
    struct ga_obj *key_int = ga_obj_super(key, &ga_int_type_inst);

    if (!key_int) {
        vm_raise_exception(vm, ga_type_error_new("Int"));
        return;
    }

    uint32_t index = (uint32_t)ga_int_to_i64(key_int);

    if (index < statep->used_cells) {
        GAOBJ_INC_REF(val);
        GAOBJ_DEC_REF(statep->cells[index]);
        statep->cells[index] = val;
        
        return;
    }

    vm_raise_exception(vm, ga_index_error_new("Index out of range"));
}

static struct ga_obj *
ga_list_len(struct ga_obj *self, struct vm *vm)
{
    return ga_int_from_i64(ga_list_size(self));
}

static struct ga_obj *
ga_list_iter_new(struct ga_obj *listp)
{
    struct ga_obj *obj = ga_obj_new(&ga_list_iter_type_inst, &list_iter_obj_ops);
    struct list_iter_state *statep = calloc(sizeof(struct list_iter_state), 1);

    statep->listp = GAOBJ_INC_REF(listp);
    statep->index = -1;
    obj->un.statep = statep;

    return obj;
}

static void
ga_list_iter_destroy(struct ga_obj *self)
{
    struct list_iter_state *statep = self->un.statep;
    GAOBJ_DEC_REF(statep->listp);
    free(statep);
}

static struct ga_obj *
ga_list_iter(struct ga_obj *self, struct vm *vm)
{
    return ga_list_iter_new(self);
}

static bool
ga_list_iter_next(struct ga_obj *self, struct vm *vm)
{
    struct list_iter_state *statep = self->un.statep;

    statep->index++;

    return statep->index < ga_list_size(statep->listp);
}

static struct ga_obj *
ga_list_iter_cur(struct ga_obj *self, struct vm *vm)
{
    struct list_iter_state *statep = self->un.statep;
    struct ga_obj *list_obj = ga_obj_super(statep->listp, &ga_list_type_inst);
    struct list_state *list_statep = list_obj->un.statep;

    return list_statep->cells[statep->index];
}

struct ga_obj *
ga_list_new()
{
    struct ga_obj *obj = ga_obj_new(&ga_list_type_inst, &list_obj_ops);
    struct list_state *statep = calloc(sizeof(struct list_state), 1);

    statep->cells = calloc(sizeof(struct ga_obj*)*GA_LIST_INITIAL_SIZE, 1);
    statep->avail_cells = GA_LIST_INITIAL_SIZE;

    obj->un.statep = statep;

    GAOBJ_SETATTR(obj, NULL, "append", ga_builtin_new(ga_list_append_method, obj));
    GAOBJ_SETATTR(obj, NULL, "remove", ga_builtin_new(ga_list_remove_method, obj));

    return obj;
}

void
ga_list_append(struct ga_obj *self, struct ga_obj *val)
{
    struct list_state *statep = self->un.statep;
    int index = statep->used_cells;

    if (index >= statep->avail_cells) {
        statep->avail_cells *= 2;
        statep->cells = realloc(statep->cells, sizeof(struct ga_obj *) * statep->avail_cells);
    }

    statep->cells[index] = GAOBJ_INC_REF(val);
    statep->used_cells++;
}

void
ga_list_remove(struct ga_obj *self, struct vm *vm, struct ga_obj *val)
{
    struct list_state *statep = self->un.statep;
    int needle_index = -1;

    for (int i = 0; i < statep->used_cells; i++) {
        if (GAOBJ_EQUALS(statep->cells[i], vm, val)) {
            needle_index = i;
            break;
        }
    }

    if (needle_index == -1) {
        return;
    }

    GAOBJ_DEC_REF(statep->cells[needle_index]);
    
    statep->used_cells--;

    for (int i = needle_index; i < statep->used_cells; i++) {
        statep->cells[i] = statep->cells[i + 1];
    }
}

int
ga_list_size(struct ga_obj *self)
{
    struct list_state *statep = self->un.statep;

    return statep->used_cells;
}
