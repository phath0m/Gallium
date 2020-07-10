#include <stdbool.h>
#include <gallium/builtins.h>
#include <gallium/object.h>

static bool                 ga_bool_istrue(struct ga_obj *, struct vm *);
static struct ga_obj    *   ga_bool_logical_not(struct ga_obj *, struct vm *);
static struct ga_obj    *   ga_bool_str(struct ga_obj *, struct vm *);

struct ga_obj ga_bool_typedef_inst = {
    .ref_count      =   1,
    .un.statep      =   "Bool"
};

struct ga_obj_ops ga_bool_ops = {
    .istrue         =   ga_bool_istrue,
    .logical_not    =   ga_bool_logical_not,
    .str            =   ga_bool_str
};

struct ga_obj ga_bool_true_inst = {
    .ref_count      =   1,
    .obj_ops        =   &ga_bool_ops,
    .type           =   &ga_bool_typedef_inst,
    .un.state_i8    =   1
};

struct ga_obj ga_bool_false_inst = {
    .ref_count      =   1,
    .obj_ops        =   &ga_bool_ops,
    .type           =   &ga_bool_typedef_inst,
    .un.state_i8    =   0
};

static bool
ga_bool_istrue(struct ga_obj *self, struct vm *vm)
{
    return self->un.state_i8 != 0;
}

static struct ga_obj *
ga_bool_logical_not(struct ga_obj *self, struct vm *vm)
{
    switch (self->un.state_i8) {
        case 0:
            return &ga_bool_true_inst;
        default:
            return &ga_bool_false_inst;
    }
}

static struct ga_obj *
ga_bool_str(struct ga_obj *self, struct vm *vm)
{
    if (ga_bool_to_bool(self)) {
        return ga_str_from_cstring("True");
    }

    return ga_str_from_cstring("False");
}

struct ga_obj *
ga_bool_from_bool(bool b)
{
    return b ? &ga_bool_true_inst : &ga_bool_false_inst;
}

bool
ga_bool_to_bool(struct ga_obj *obj)
{
    return obj->un.state_i8 != 0;
}
