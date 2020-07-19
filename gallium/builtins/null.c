#include <stdbool.h>
#include <gallium/builtins.h>
#include <gallium/object.h>

static bool                 ga_null_istrue(struct ga_obj *, struct vm *);
static struct ga_obj    *   ga_null_str(struct ga_obj *, struct vm *);

struct ga_obj ga_null_typedef_inst = {
    .ref_count      =   1,
    .un.statep      =   "Null"
};

struct ga_obj_ops ga_null_ops = {
    .istrue         =   ga_null_istrue,
    .str            =   ga_null_str
};

struct ga_obj ga_null_inst = {
    .ref_count      =   1,
    .type           =   &ga_null_typedef_inst,
    .obj_ops        =   &ga_null_ops,
    .un.state_i8    =   0
};

static bool
ga_null_istrue(struct ga_obj *self, struct vm *vm)
{
    return false;
}

static struct ga_obj *
ga_null_str(struct ga_obj *self, struct vm *vm)
{
    return ga_str_from_cstring("Null");
}

