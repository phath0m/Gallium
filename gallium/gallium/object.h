#ifndef _GALLIUM_OBJECT_H
#define _GALLIUM_OBJECT_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <gallium/dict.h>

struct ga_obj;
struct vm;

typedef void            (*obj_destroy_t)(struct ga_obj *);
typedef bool            (*obj_istrue_t)(struct ga_obj *, struct vm *);
typedef struct ga_obj * (*obj_str_t)(struct ga_obj *, struct vm *);
typedef void            (*obj_setattr_t)(struct ga_obj *, struct vm *, const char *, struct ga_obj *);
typedef struct ga_obj * (*obj_getattr_t)(struct ga_obj *, struct vm *, const char *);
typedef void            (*obj_setindex_t)(struct ga_obj *, struct vm *, struct ga_obj *, struct ga_obj *);
typedef struct ga_obj * (*obj_getindex_t)(struct ga_obj *, struct vm *, struct ga_obj *);
typedef struct ga_obj * (*obj_invoke_t)(struct ga_obj *, struct vm *, int, struct ga_obj **);
typedef struct ga_obj * (*obj_iter_t)(struct ga_obj *, struct vm *);
typedef bool            (*obj_iter_next_t)(struct ga_obj *, struct vm *);
typedef struct ga_obj * (*obj_iter_cur_t)(struct ga_obj *, struct vm *);
typedef int64_t         (*op_hash_t)(struct ga_obj *, struct vm *);
typedef bool            (*op_equals_t)(struct ga_obj *, struct vm *, struct ga_obj *);
typedef struct ga_obj * (*op_len_t)(struct ga_obj *, struct vm *);
typedef struct ga_obj * (*op_logical_not_t)(struct ga_obj *, struct vm *);
typedef struct ga_obj * (*op_negate_t)(struct ga_obj *, struct vm *);
typedef struct ga_obj * (*op_not_t)(struct ga_obj *, struct vm *);
typedef bool            (*op_gt_t)(struct ga_obj *, struct vm *, struct ga_obj *);
typedef bool            (*op_ge_t)(struct ga_obj *, struct vm *, struct ga_obj *);
typedef bool            (*op_lt_t)(struct ga_obj *, struct vm *, struct ga_obj *);
typedef bool            (*op_le_t)(struct ga_obj *, struct vm *, struct ga_obj *);
typedef struct ga_obj * (*op_add_t)(struct ga_obj *, struct vm *, struct ga_obj *);
typedef struct ga_obj * (*op_sub_t)(struct ga_obj *, struct vm *, struct ga_obj *);
typedef struct ga_obj * (*op_mul_t)(struct ga_obj *, struct vm *, struct ga_obj *);
typedef struct ga_obj * (*op_div_t)(struct ga_obj *, struct vm *, struct ga_obj *);
typedef struct ga_obj * (*op_mod_t)(struct ga_obj *, struct vm *, struct ga_obj *);
typedef struct ga_obj * (*op_and_t)(struct ga_obj *, struct vm *, struct ga_obj *);
typedef struct ga_obj * (*op_or_t)(struct ga_obj *, struct vm *, struct ga_obj *);
typedef struct ga_obj * (*op_xor_t)(struct ga_obj *, struct vm *, struct ga_obj *);
typedef struct ga_obj * (*op_shl_t)(struct ga_obj *, struct vm *, struct ga_obj *);
typedef struct ga_obj * (*op_shr_t)(struct ga_obj *, struct vm *, struct ga_obj *);
typedef struct ga_obj * (*op_closed_range_t)(struct ga_obj *, struct vm *, struct ga_obj *);
typedef struct ga_obj * (*op_half_range_t)(struct ga_obj *, struct vm *, struct ga_obj *);

struct ga_obj_ops {
    obj_destroy_t       destroy;
    obj_istrue_t        istrue;
    obj_str_t           str;
    obj_setattr_t       setattr;
    obj_getattr_t       getattr;
    obj_setindex_t      setindex;
    obj_getindex_t      getindex;
    obj_invoke_t        invoke;
    obj_iter_t          iter;
    obj_iter_next_t     iter_next;
    obj_iter_cur_t      iter_cur;
    op_hash_t           hash;
    op_equals_t         equals;
    op_len_t            len;
    op_logical_not_t    logical_not;
    op_negate_t         negate;
    op_not_t            inverse;
    op_gt_t             gt;
    op_ge_t             ge;
    op_lt_t             lt;
    op_le_t             le;
    op_add_t            add;
    op_sub_t            sub;
    op_mul_t            mul;
    op_div_t            div;
    op_mod_t            mod;
    op_and_t            band;
    op_or_t             bor;
    op_xor_t            bxor;
    op_shl_t            shl;
    op_shr_t            shr;
    op_closed_range_t   closed_range;
    op_half_range_t     half_range;
};

struct ga_obj {
    int                     ref_count;
    struct ga_obj_ops   *   obj_ops;
    struct ga_obj       *   type;
    struct ga_obj       *   super;
    struct dict             dict;
    struct list         *   weak_refs;
    
    union {
        void    *   statep;
        int8_t      state_i8;
        int64_t     state_i64;
        uint8_t     state_u8;
        uint32_t    state_u32;
    } un;
};

struct ga_obj_statistics {
    int     obj_count;
};

/* singleton instance of Gallium "TypeDef" object instance */
extern struct ga_obj            ga_type_type_inst;

/* debugging... */
extern struct ga_obj_statistics ga_obj_stat;

struct ga_obj   *   ga_type_new(const char *);
const char      *   ga_type_name(struct ga_obj *);

void                ga_obj_destroy(struct ga_obj *);
struct ga_obj   *   ga_obj_new(struct ga_obj *, struct ga_obj_ops *);
struct ga_obj   *   ga_obj_super(struct ga_obj *, struct ga_obj *);
bool                ga_obj_instanceof(struct ga_obj *, struct ga_obj *);
void                ga_obj_print(struct ga_obj *, struct vm *vm);

/*
 * these should (and are) actually defined inside builtins.h
 * However... I need them here, since I'm using all of these
 * inline functions
 */
struct ga_obj   *       ga_operator_error_new(const char *);
void                    vm_raise_exception(struct vm *, struct ga_obj *);


/* increments the object reference counter */
__attribute__((always_inline))
static inline struct ga_obj *
GAOBJ_INC_REF(struct ga_obj *obj)
{
    obj->ref_count++;
    return obj;
}

/* increments the object reference counter (may be NULL) */
__attribute__((always_inline))
static inline struct ga_obj *
GAOBJ_XINC_REF(struct ga_obj *obj)
{
    if (obj) {
        obj->ref_count++;
    }

    return obj;
}

/* decrements the object reference counter, will destroy if it reaches 0 */
__attribute__((always_inline))
static inline void
GAOBJ_DEC_REF(struct ga_obj *obj)
{
    obj->ref_count--;

    if (obj->ref_count == 0) {
        ga_obj_destroy(obj);
    }
}

__attribute__((always_inline))
static inline void
GAOBJ_CLEAR_REF(struct ga_obj **obj)
{
    GAOBJ_DEC_REF(*obj);
    *obj = NULL;
}

/*
 * decrements the object reference counter, intended for transfering control of the
 * reference counter to another function. Will not destroy object
 */
__attribute__((always_inline))
static inline struct ga_obj *
GAOBJ_MOVE_REF(struct ga_obj *obj)
{
    obj->ref_count--;

    return obj;
}

__attribute__((always_inline))
static inline struct ga_obj *
GAOBJ_XMOVE_REF(struct ga_obj *obj)
{
    if (obj) obj->ref_count--;

    return obj;
}

__attribute__((always_inline))
static inline struct ga_obj *
GAOBJ_GETATTR_FAST(struct ga_obj *self, struct vm *vm, uint32_t hash, const char *name)
{
    struct ga_obj *obj = NULL;

    while (self) {
        if (self->obj_ops && self->obj_ops->getattr) {
            return self->obj_ops->getattr(self, vm, name);
        }
       
        if (dict_get_prehashed(&self->dict, hash, name, (void**)&obj)) {
            break;
        }

        self = self->super;
    }

    return obj;
}

__attribute__((always_inline))
static inline struct ga_obj *
GAOBJ_GETATTR(struct ga_obj *self, struct vm *vm, const char *name)
{
    struct ga_obj *obj = NULL;

    while (self) {
        if (self->obj_ops && self->obj_ops->getattr) {
            return self->obj_ops->getattr(self, vm, name);
        }
       
        if (dict_get(&self->dict, name, (void**)&obj)) {
            break;
        }

        self = self->super;
    }

    return obj;
}

__attribute__((always_inline))
static inline struct ga_obj *
GAOBJ_INVOKE(struct ga_obj *self, struct vm *vm, int argc, struct ga_obj **args)
{
    if (self->obj_ops && self->obj_ops->invoke) {
        return self->obj_ops->invoke(self, vm, argc, args);
    }

    vm_raise_exception(vm, ga_operator_error_new("__invoke__ is not implemented"));

    return NULL;
}

__attribute__((always_inline))
static inline bool
GAOBJ_IS_TRUE(struct ga_obj *self, struct vm *vm)
{
    while (self) {
        if (self->obj_ops && self->obj_ops->istrue) {
            return self->obj_ops->istrue(self, vm);
        }
        self = self->super;
    }

    return true;
}

__attribute__((always_inline))
static inline struct ga_obj *
ga_obj_iter(struct ga_obj *self, struct vm *vm)
{
    for (struct ga_obj *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->iter) {
            return i->obj_ops->iter(i, vm);
        }
        struct ga_obj *operator = GAOBJ_XINC_REF(GAOBJ_GETATTR(self, vm, "__iter__"));
        if (operator) {
            struct ga_obj *ret = GAOBJ_INVOKE(operator, vm, 0, NULL);
            GAOBJ_DEC_REF(operator);
            return ret;
        }
    }
    vm_raise_exception(vm, ga_operator_error_new("__iter__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline bool
GAOBJ_ITER_NEXT(struct ga_obj *self, struct vm *vm)
{
    for (struct ga_obj *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->iter_next) {
            return i->obj_ops->iter_next(i, vm);
        }
        struct ga_obj *operator = GAOBJ_XINC_REF(GAOBJ_GETATTR(self, vm, "__next__"));
        if (operator) {
            struct ga_obj *ret = GAOBJ_INVOKE(operator, vm, 0, NULL);
            GAOBJ_DEC_REF(operator);
            return GAOBJ_IS_TRUE(ret, vm);
        }
    }
    vm_raise_exception(vm, ga_operator_error_new("__next__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline struct ga_obj *
GAOBJ_ITER_CUR(struct ga_obj *self, struct vm *vm)
{
    for (struct ga_obj *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->iter_cur) {
            return i->obj_ops->iter_cur(i, vm);
        }
        struct ga_obj *operator = GAOBJ_XINC_REF(GAOBJ_GETATTR(self, vm, "__cur__"));
        if (operator) {
            struct ga_obj *ret = GAOBJ_INVOKE(operator, vm, 0, NULL);
            GAOBJ_DEC_REF(operator);
            return ret;
        }
    }
    vm_raise_exception(vm, ga_operator_error_new("__cur__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline void
GAOBJ_SETATTR(struct ga_obj *self, struct vm *vm, const char *name, struct ga_obj *val)
{
    if (self->obj_ops && self->obj_ops->setattr) {
        self->obj_ops->setattr(self, vm, name, val);
        return;
    }

    GAOBJ_INC_REF(val);

    struct ga_obj *old_val;

    if (dict_get(&self->dict, name, (void**)&old_val)) {
        GAOBJ_DEC_REF(old_val);
    }

    dict_set(&self->dict, name, val);
}


__attribute__((always_inline))
static inline void 
GAOBJ_SETINDEX(struct ga_obj *self, struct vm *vm, struct ga_obj *key, struct ga_obj *val)
{
    while (self) {
        if (self->obj_ops && self->obj_ops->setindex) {
            self->obj_ops->setindex(self, vm, key, val);
            break;
        }
        self = self->super;
    }

}

__attribute__((always_inline))
static inline struct ga_obj *
GAOBJ_GETINDEX(struct ga_obj *self, struct vm *vm, struct ga_obj *key)
{
    while (self) {
        if (self->obj_ops && self->obj_ops->getindex) {
            return self->obj_ops->getindex(self, vm, key);
        }
        self = self->super;
    }
    vm_raise_exception(vm, ga_operator_error_new("__getindex__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline struct ga_obj *
GAOBJ_STR(struct ga_obj *self, struct vm *vm)
{
    for (struct ga_obj *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->str) {
            return i->obj_ops->str(i, vm);
        }
        struct ga_obj *op = GAOBJ_XINC_REF(GAOBJ_GETATTR(self, vm, "__str__"));
        if (op) {
            struct ga_obj *ret = GAOBJ_INVOKE(op, vm, 0, NULL);
            GAOBJ_DEC_REF(op);
            return ret;
        }
    }
    extern struct ga_obj *ga_str_from_cstring(const char *);
    return ga_str_from_cstring(ga_type_name(self->type));
}

__attribute__((always_inline))
static inline int64_t
GAOBJ_HASH(struct ga_obj *self, struct vm *vm)
{
    for (struct ga_obj *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->hash) {
            return i->obj_ops->hash(i, vm);
        }
    }
    return (int64_t)(uintptr_t)self;
}

__attribute__((always_inline))
static inline bool
GAOBJ_EQUALS(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    for (struct ga_obj *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->equals) {
            return i->obj_ops->equals(i, vm, right);
        }
        struct ga_obj *op = GAOBJ_XINC_REF(GAOBJ_GETATTR(self, vm, "__equals__"));
        if (op) {
            struct ga_obj *ret = GAOBJ_INVOKE(op, vm, 1, &right);
            GAOBJ_DEC_REF(op);
            return GAOBJ_IS_TRUE(ret, vm);
        }
    }
    return self == right;
}

__attribute__((always_inline))
static inline struct ga_obj *
ga_obj_len(struct ga_obj *self, struct vm *vm)
{
    for (struct ga_obj *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->len) {
            return i->obj_ops->len(i, vm);
        }
        struct ga_obj *op = GAOBJ_XINC_REF(GAOBJ_GETATTR(self, vm, "__len__"));
        if (op) {
            struct ga_obj *ret = GAOBJ_INVOKE(op, vm, 0, NULL);
            GAOBJ_DEC_REF(op);
            return ret;
        }
    }
    vm_raise_exception(vm, ga_operator_error_new("__len__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline struct ga_obj *
GAOBJ_LOGICAL_NOT(struct ga_obj *self, struct vm *vm)
{
    for (struct ga_obj *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->logical_not) {
            return i->obj_ops->logical_not(i, vm);
        }
        struct ga_obj *op = GAOBJ_XINC_REF(GAOBJ_GETATTR(self, vm, "__not__"));
        if (op) {
            struct ga_obj *ret = GAOBJ_INVOKE(op, vm, 0, NULL);
            GAOBJ_DEC_REF(op);
            return ret;
        }
    }
    vm_raise_exception(vm, ga_operator_error_new("__not__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline struct ga_obj *
GAOBJ_NEGATE(struct ga_obj *self, struct vm *vm)
{
    for (struct ga_obj *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->negate) {
            return i->obj_ops->negate(i, vm);
        }
        struct ga_obj *op = GAOBJ_XINC_REF(GAOBJ_GETATTR(self, vm, "__negate__"));
        if (op) {
            struct ga_obj *ret = GAOBJ_INVOKE(op, vm, 0, NULL);
            GAOBJ_DEC_REF(op);
            return ret;
        }
    }
    vm_raise_exception(vm, ga_operator_error_new("__negate__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline struct ga_obj *
GAOBJ_NOT(struct ga_obj *self, struct vm *vm)
{
    for (struct ga_obj *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->inverse) {
            return i->obj_ops->inverse(i, vm);
        }
        struct ga_obj *op = GAOBJ_XINC_REF(GAOBJ_GETATTR(self, vm, "__inverse__"));
        if (op) {
            struct ga_obj *ret = GAOBJ_INVOKE(op, vm, 0, NULL);
            GAOBJ_DEC_REF(op);
            return ret;
        }
    }
    vm_raise_exception(vm, ga_operator_error_new("__inverse__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline bool
GAOBJ_GT(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    for (struct ga_obj *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->gt) {
            return i->obj_ops->gt(i, vm, right);
        }
        struct ga_obj *op = GAOBJ_XINC_REF(GAOBJ_GETATTR(self, vm, "__gt__"));
        if (op) {
            struct ga_obj *ret = GAOBJ_INVOKE(op, vm, 1, &right);
            GAOBJ_DEC_REF(op);
            return GAOBJ_IS_TRUE(ret, vm);
        }
    }
    vm_raise_exception(vm, ga_operator_error_new("__gt__ is not implemented"));
    return false;
}

__attribute__((always_inline))
static inline bool
ga_obj_ge(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    for (struct ga_obj *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->ge) {
            return i->obj_ops->ge(i, vm, right);
        }
        struct ga_obj *op = GAOBJ_XINC_REF(GAOBJ_GETATTR(self, vm, "__ge__"));
        if (op) {
            struct ga_obj *ret = GAOBJ_INVOKE(op, vm, 1, &right);
            GAOBJ_DEC_REF(op);
            return GAOBJ_IS_TRUE(ret, vm);
        }
    }
    vm_raise_exception(vm, ga_operator_error_new("__ge__ is not implemented"));
    return false;
}

__attribute__((always_inline))
static inline bool
GAOBJ_LT(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    for (struct ga_obj *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->lt) {
            return i->obj_ops->lt(i, vm, right);
        }
        struct ga_obj *op = GAOBJ_XINC_REF(GAOBJ_GETATTR(self, vm, "__lt__"));
        if (op) {
            struct ga_obj *ret = GAOBJ_INVOKE(op, vm, 1, &right);
            GAOBJ_DEC_REF(op);
            return GAOBJ_IS_TRUE(ret, vm);
        }
    }
    vm_raise_exception(vm, ga_operator_error_new("__lt__ is not implemented"));
    return false;
}

__attribute__((always_inline))
static inline bool
GAOBJ_LE(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    for (struct ga_obj *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->le) {
            return i->obj_ops->le(i, vm, right);
        }
        struct ga_obj *op = GAOBJ_XINC_REF(GAOBJ_GETATTR(self, vm, "__le__"));
        if (op) {
            struct ga_obj *ret = GAOBJ_INVOKE(op, vm, 1, &right);
            GAOBJ_DEC_REF(op);
            return GAOBJ_IS_TRUE(ret, vm);
        }
    }
    vm_raise_exception(vm, ga_operator_error_new("__le__ is not implemented"));
    return false;
}

__attribute__((always_inline))
static inline struct ga_obj *
GAOBJ_ADD(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    for (struct ga_obj *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->add) {
            return i->obj_ops->add(i, vm, right);
        }
        struct ga_obj *operator = GAOBJ_XINC_REF(GAOBJ_GETATTR(self, vm, "__add__"));
        if (operator) {
            struct ga_obj *ret = GAOBJ_INVOKE(operator, vm, 1, &right);
            GAOBJ_DEC_REF(operator);
            return ret;
        }
    }
    vm_raise_exception(vm, ga_operator_error_new("__add__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline struct ga_obj *
GAOBJ_SUB(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    for (struct ga_obj *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->sub) {
            return i->obj_ops->sub(i, vm, right);
        }
        struct ga_obj *op = GAOBJ_XINC_REF(GAOBJ_GETATTR(self, vm, "__sub__"));
        if (op) {
            struct ga_obj *ret = GAOBJ_INVOKE(op, vm, 1, &right);
            GAOBJ_DEC_REF(op);
            return ret;
        }
    }
    vm_raise_exception(vm, ga_operator_error_new("__sub__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline struct ga_obj *
GAOBJ_MUL(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    for (struct ga_obj *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->mul) {
            return i->obj_ops->mul(i, vm, right);
        }
        struct ga_obj *op = GAOBJ_XINC_REF(GAOBJ_GETATTR(self, vm, "__mul__"));
        if (op) {
            struct ga_obj *ret = GAOBJ_INVOKE(op, vm, 1, &right);
            GAOBJ_DEC_REF(op);
            return ret;
        }
    }
    vm_raise_exception(vm, ga_operator_error_new("__mul__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline struct ga_obj *
GAOBJ_DIV(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    for (struct ga_obj *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->div) {
            return i->obj_ops->div(i, vm, right);
        }
        struct ga_obj *op = GAOBJ_XINC_REF(GAOBJ_GETATTR(self, vm, "__div__"));
        if (op) {
            struct ga_obj *ret = GAOBJ_INVOKE(op, vm, 1, &right);
            GAOBJ_DEC_REF(op);
            return ret;
        }
    }
    vm_raise_exception(vm, ga_operator_error_new("__div__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline struct ga_obj *
GAOBJ_MOD(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    for (struct ga_obj *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->mod) {
            return i->obj_ops->mod(i, vm, right);
        }
        struct ga_obj *op = GAOBJ_XINC_REF(GAOBJ_GETATTR(self, vm, "__mod__"));
        if (op) {
            struct ga_obj *ret = GAOBJ_INVOKE(op, vm, 1, &right);
            GAOBJ_DEC_REF(op);
            return ret;
        }
    }
    vm_raise_exception(vm, ga_operator_error_new("__mod__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline struct ga_obj *
GAOBJ_AND(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    for (struct ga_obj *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->band) {
            return i->obj_ops->band(i, vm, right);
        }
        struct ga_obj *op = GAOBJ_XINC_REF(GAOBJ_GETATTR(self, vm, "__and__"));
        if (op) {
            struct ga_obj *ret = GAOBJ_INVOKE(op, vm, 1, &right);
            GAOBJ_DEC_REF(op);
            return ret;
        }
    }
    vm_raise_exception(vm, ga_operator_error_new("__and__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline struct ga_obj *
GAOBJ_OR(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    for (struct ga_obj *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->bor) {
            return i->obj_ops->bor(i, vm, right);
        }
        struct ga_obj *op = GAOBJ_XINC_REF(GAOBJ_GETATTR(self, vm, "__or__"));
        if (op) {
            struct ga_obj *ret = GAOBJ_INVOKE(op, vm, 1, &right);
            GAOBJ_DEC_REF(op);
            return ret;
        }
    }
    vm_raise_exception(vm, ga_operator_error_new("__or__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline struct ga_obj *
GAOBJ_XOR(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    for (struct ga_obj *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->bxor) {
            return i->obj_ops->bxor(i, vm, right);
        }
        struct ga_obj *op = GAOBJ_XINC_REF(GAOBJ_GETATTR(self, vm, "__xor__"));
        if (op) {
            struct ga_obj *ret = GAOBJ_INVOKE(op, vm, 1, &right);
            GAOBJ_DEC_REF(op);
            return ret;
        }
    }
    vm_raise_exception(vm, ga_operator_error_new("__xor__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline struct ga_obj *
GAOBJ_SHL(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    for (struct ga_obj *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->shl) {
            return i->obj_ops->shl(i, vm, right);
        }
        struct ga_obj *op = GAOBJ_XINC_REF(GAOBJ_GETATTR(self, vm, "__shl__"));
        if (op) {
            struct ga_obj *ret = GAOBJ_INVOKE(op, vm, 1, &right);
            GAOBJ_DEC_REF(op);
            return ret;
        }
    }
    vm_raise_exception(vm, ga_operator_error_new("__shl__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline struct ga_obj *
GAOBJ_SHR(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    for (struct ga_obj *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->shr) {
            return i->obj_ops->shr(i, vm, right);
        }
        struct ga_obj *op = GAOBJ_XINC_REF(GAOBJ_GETATTR(self, vm, "__shr__"));
        if (op) {
            struct ga_obj *ret = GAOBJ_INVOKE(op, vm, 1, &right);
            GAOBJ_DEC_REF(op);
            return ret;
        }
    }
    vm_raise_exception(vm, ga_operator_error_new("__shr__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline struct ga_obj *
GAOBJ_CLOSED_RANGE(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    for (struct ga_obj *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->closed_range) {
            return i->obj_ops->closed_range(i, vm, right);
        }
        struct ga_obj *op = GAOBJ_XINC_REF(GAOBJ_GETATTR(self, vm, "__closed_range__"));
        if (op) {
            struct ga_obj *ret = GAOBJ_INVOKE(op, vm, 1, &right);
            GAOBJ_DEC_REF(op);
            return ret;
        }
    }
    vm_raise_exception(vm, ga_operator_error_new("__closed_range__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline struct ga_obj *
GAOBJ_HALF_RANGE(struct ga_obj *self, struct vm *vm, struct ga_obj *right)
{
    for (struct ga_obj *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->half_range) {
            return i->obj_ops->half_range(i, vm, right);
        }
        struct ga_obj *op = GAOBJ_XINC_REF(GAOBJ_GETATTR(self, vm, "__half_range__"));
        if (op) {
            struct ga_obj *ret = GAOBJ_INVOKE(op, vm, 1, &right);
            GAOBJ_DEC_REF(op);
            return ret;
        }
    }
    vm_raise_exception(vm, ga_operator_error_new("__half_range__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline void
GAOBJ_WEAKREF_ADD(struct ga_obj *obj, struct ga_obj **ref)
{
    if (!obj->weak_refs) {
        obj->weak_refs = list_new();
    }
    *ref = obj;
    list_append(obj->weak_refs, ref);
}

__attribute__((always_inline))
static inline void
GAOBJ_WEAKREF_DEL(struct ga_obj **ref)
{
    extern struct ga_obj ga_null_inst;
    struct ga_obj *obj = *ref;

    if (obj == &ga_null_inst) {
        return;
    }
    
    list_remove(obj->weak_refs, ref, NULL, NULL);
}

#endif
