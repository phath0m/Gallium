#ifndef _GALLIUM_OBJECT_H
#define _GALLIUM_OBJECT_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <gallium/dict.h>

typedef struct ga_obj GaObject;
typedef struct ga_context GaContext;

typedef void            (*_GaDestroy)(GaObject *);
typedef bool            (*_GaIsTrue)(GaObject *, GaContext *);
typedef GaObject    *   (*_GaStr)(GaObject *, GaContext *);
typedef void            (*_GaSetAttr)(GaObject *, GaContext *, const char *, GaObject *);
typedef GaObject    *   (*_GaGetAttr)(GaObject *, GaContext *, const char *);
typedef void            (*_GaSetIndex)(GaObject *, GaContext *, GaObject *, GaObject *);
typedef GaObject    *   (*_GaGetIndex)(GaObject *, GaContext *, GaObject *);
typedef GaObject    *   (*_GaInvoke)(GaObject *, GaContext *, int, GaObject **);
typedef GaObject    *   (*_GaIter)(GaObject *, GaContext *);
typedef bool            (*_GaIterNext)(GaObject *, GaContext *);
typedef GaObject    *   (*_GaIterCur)(GaObject *, GaContext *);
typedef int64_t         (*_GaHash)(GaObject *, GaContext *);
typedef bool            (*_GaEquals)(GaObject *, GaContext *, GaObject *);
typedef bool            (*_GaMatch)(GaObject *, GaContext *, GaObject *);
typedef GaObject    *   (*_GaLen)(GaObject *, GaContext *);
typedef GaObject    *   (*_GaLogicalNot)(GaObject *, GaContext *);
typedef GaObject    *   (*_GaNegate)(GaObject *, GaContext *);
typedef GaObject    *   (*_GaNot)(GaObject *, GaContext *);
typedef bool            (*_GaGreaterThan)(GaObject *, GaContext *, GaObject *);
typedef bool            (*_GaGreaterThanOrEqual)(GaObject *, GaContext *, GaObject *);
typedef bool            (*_GaLessThan)(GaObject *, GaContext *, GaObject *);
typedef bool            (*_GaLessThanOrEqual)(GaObject *, GaContext *, GaObject *);
typedef GaObject    *   (*_GaAdd)(GaObject *, GaContext *, GaObject *);
typedef GaObject    *   (*_GaSub)(GaObject *, GaContext *, GaObject *);
typedef GaObject    *   (*_GaMul)(GaObject *, GaContext *, GaObject *);
typedef GaObject    *   (*_GaDiv)(GaObject *, GaContext *, GaObject *);
typedef GaObject    *   (*_GaMod)(GaObject *, GaContext *, GaObject *);
typedef GaObject    *   (*_GaAnd)(GaObject *, GaContext *, GaObject *);
typedef GaObject    *   (*_GaOr)(GaObject *, GaContext *, GaObject *);
typedef GaObject    *   (*_GaXor)(GaObject *, GaContext *, GaObject *);
typedef GaObject    *   (*_GaShl)(GaObject *, GaContext *, GaObject *);
typedef GaObject    *   (*_GaShr)(GaObject *, GaContext *, GaObject *);
typedef GaObject    *   (*_GaClosedRange)(GaObject *, GaContext *, GaObject *);
typedef GaObject    *   (*_GaHalfRange)(GaObject *, GaContext *, GaObject *);

struct ga_obj_ops {
    _GaDestroy              destroy;
    _GaIsTrue               istrue;
    _GaStr                  str;
    _GaSetAttr              setattr;
    _GaGetAttr              getattr;
    _GaSetIndex             setindex;
    _GaGetIndex             getindex;
    _GaInvoke               invoke;
    _GaIter                 iter;
    _GaIterNext             iter_next;
    _GaIterCur              iter_cur;
    _GaHash                 hash;
    _GaEquals               equals;
    _GaMatch                match;
    _GaLen                  len;
    _GaLogicalNot           logical_not;
    _GaNegate               negate;
    _GaNot                  inverse;
    _GaGreaterThan          gt;
    _GaGreaterThanOrEqual   ge;
    _GaLessThan             lt;
    _GaLessThanOrEqual      le;
    _GaAdd                  add;
    _GaSub                  sub;
    _GaMul                  mul;
    _GaDiv                  div;
    _GaMod                  mod;
    _GaAnd                  band;
    _GaOr                   bor;
    _GaXor                  bxor;
    _GaShl                  shl;
    _GaShr                  shr;
    _GaClosedRange          closed_range;
    _GaHalfRange            half_range;
};

struct ga_obj {
    int                     ref_count;
    struct ga_obj_ops   *   obj_ops;
    GaObject            *   type;
    GaObject            *   super;
    _Ga_dict_t              dict;
    _Ga_list_t          *   weak_refs;
    
    union {
        void    *   statep;
        int8_t      state_i8;
        int64_t     state_i64;
        uint8_t     state_u8;
        uint32_t    state_u32;
        double      state_f64;
    } un;
};

struct ga_obj_statistics {
    int     obj_count;
};

/* singleton instance of Gallium "TypeDef" object instance */
extern GaObject            ga_type_type_inst;

/* debugging... */
extern struct ga_obj_statistics ga_obj_stat;

GaObject    *   GaObj_NewType(const char *);
const char  *   GaObj_TypeName(GaObject *);

/*
 * "match" operator for type operator, publically exported for the VM
 * interpreter.
 */
bool            _GaType_Match(GaObject *, GaContext *, GaObject *);


void            GaObj_Destroy(GaObject *);
GaObject    *   GaObj_New(GaObject *, struct ga_obj_ops *);
GaObject    *   GaObj_Super(GaObject *, GaObject *);
bool            GaObj_IsInstanceOf(GaObject *, GaObject *);
void            GaObj_Print(GaObject *, GaContext *vm);
void            GaObj_Assign(GaObject *, GaObject *);
/*
 * these should (and are) actually defined inside builtins.h
 * However... I need them here, since I'm using all of these
 * inline functions
 */
GaObject    *   GaErr_NewOperatorError(const char *);
void            GaEval_RaiseException(GaContext *, GaObject *);


/* increments the object reference counter */
__attribute__((always_inline))
static inline GaObject *
GaObj_INC_REF(GaObject *obj)
{
    obj->ref_count++;
    return obj;
}

/* increments the object reference counter (may be NULL) */
__attribute__((always_inline))
static inline GaObject *
GaObj_XINC_REF(GaObject *obj)
{
    if (obj) {
        obj->ref_count++;
    }

    return obj;
}

/* decrements the object reference counter, will destroy if it reaches 0 */
__attribute__((always_inline))
static inline void
GaObj_DEC_REF(GaObject *obj)
{
    obj->ref_count--;
    if (obj->ref_count == 0) GaObj_Destroy(obj);
}

__attribute__((always_inline))
static inline void
GaObj_XDEC_REF(GaObject *obj)
{
    if (obj) {
        obj->ref_count--;
        if (obj->ref_count == 0) GaObj_Destroy(obj);
    }
}

__attribute__((always_inline))
static inline void
GaObj_CLEAR_REF(GaObject **obj)
{
    GaObj_DEC_REF(*obj);
    *obj = NULL;
}

/*
 * decrements the object reference counter, intended for transfering control of the
 * reference counter to another function. Will not destroy object
 */
__attribute__((always_inline))
static inline GaObject *
GaObj_MOVE_REF(GaObject *obj)
{
    obj->ref_count--;

    return obj;
}

__attribute__((always_inline))
static inline GaObject *
GaObj_XMOVE_REF(GaObject *obj)
{
    if (obj) obj->ref_count--;

    return obj;
}

__attribute__((always_inline))
static inline GaObject *
_GaObj_GETATTR_FAST(GaObject *self, GaContext *vm, uint32_t hash, const char *name)
{
    GaObject *obj = NULL;

    while (self) {
        if (self->obj_ops && self->obj_ops->getattr) {
            return self->obj_ops->getattr(self, vm, name);
        }
       
        if (_Ga_hashmap_get_prehashed(&self->dict, hash, name, (void**)&obj)) {
            break;
        }

        self = self->super;
    }

    return obj;
}

__attribute__((always_inline))
static inline GaObject *
GaObj_GETATTR(GaObject *self, GaContext *vm, const char *name)
{
    GaObject *obj = NULL;

    while (self) {
        if (self->obj_ops && self->obj_ops->getattr) {
            return self->obj_ops->getattr(self, vm, name);
        }
       
        if (_Ga_hashmap_get(&self->dict, name, (void**)&obj)) {
            break;
        }

        self = self->super;
    }

    return obj;
}

__attribute__((always_inline))
static inline GaObject *
GaObj_INVOKE(GaObject *self, GaContext *vm, int argc, GaObject **args)
{
    if (self->obj_ops && self->obj_ops->invoke) {
        return self->obj_ops->invoke(self, vm, argc, args);
    }

    GaEval_RaiseException(vm, GaErr_NewOperatorError("__invoke__ is not implemented"));

    return NULL;
}

__attribute__((always_inline))
static inline bool
GaObj_IS_TRUE(GaObject *self, GaContext *vm)
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
static inline GaObject *
GaObj_ITER(GaObject *self, GaContext *vm)
{
    for (GaObject *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->iter) {
            return i->obj_ops->iter(i, vm);
        }
        GaObject *operator = GaObj_XINC_REF(GaObj_GETATTR(self, vm, "__iter__"));
        if (operator) {
            GaObject *ret = GaObj_INVOKE(operator, vm, 0, NULL);
            GaObj_DEC_REF(operator);
            return ret;
        }
    }
    GaEval_RaiseException(vm, GaErr_NewOperatorError("__iter__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline bool
GaObj_ITER_NEXT(GaObject *self, GaContext *vm)
{
    for (GaObject *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->iter_next) {
            return i->obj_ops->iter_next(i, vm);
        }
        GaObject *operator = GaObj_XINC_REF(GaObj_GETATTR(self, vm, "__next__"));
        if (operator) {
            GaObject *ret = GaObj_INVOKE(operator, vm, 0, NULL);
            GaObj_DEC_REF(operator);
            return GaObj_IS_TRUE(ret, vm);
        }
    }
    GaEval_RaiseException(vm, GaErr_NewOperatorError("__next__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline GaObject *
GaObj_ITER_CUR(GaObject *self, GaContext *vm)
{
    for (GaObject *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->iter_cur) {
            return i->obj_ops->iter_cur(i, vm);
        }
        GaObject *operator = GaObj_XINC_REF(GaObj_GETATTR(self, vm, "__cur__"));
        if (operator) {
            GaObject *ret = GaObj_INVOKE(operator, vm, 0, NULL);
            GaObj_DEC_REF(operator);
            return ret;
        }
    }
    GaEval_RaiseException(vm, GaErr_NewOperatorError("__cur__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline void
GaObj_SETATTR(GaObject *self, GaContext *vm, const char *name, GaObject *val)
{
    if (self->obj_ops && self->obj_ops->setattr) {
        self->obj_ops->setattr(self, vm, name, val);
        return;
    }

    GaObj_INC_REF(val);

    GaObject *old_val;

    if (_Ga_hashmap_get(&self->dict, name, (void**)&old_val)) {
        GaObj_DEC_REF(old_val);
    }

    _Ga_hashmap_set(&self->dict, name, val);
}


__attribute__((always_inline))
static inline void 
GaObj_SETINDEX(GaObject *self, GaContext *vm, GaObject *key, GaObject *val)
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
static inline GaObject *
GaObj_GETINDEX(GaObject *self, GaContext *vm, GaObject *key)
{
    while (self) {
        if (self->obj_ops && self->obj_ops->getindex) {
            return self->obj_ops->getindex(self, vm, key);
        }
        self = self->super;
    }
    GaEval_RaiseException(vm, GaErr_NewOperatorError("__getindex__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline GaObject *
GaObj_STR(GaObject *self, GaContext *vm)
{
    for (GaObject *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->str) {
            return i->obj_ops->str(i, vm);
        }
        GaObject *op = GaObj_XINC_REF(GaObj_GETATTR(self, vm, "__str__"));
        if (op) {
            GaObject *ret = GaObj_INVOKE(op, vm, 0, NULL);
            GaObj_DEC_REF(op);
            return ret;
        }
    }
    extern GaObject *GaStr_FromCString(const char *);
    return GaStr_FromCString(GaObj_TypeName(self->type));
}

__attribute__((always_inline))
static inline int64_t
GaObj_HASH(GaObject *self, GaContext *vm)
{
    for (GaObject *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->hash) {
            return i->obj_ops->hash(i, vm);
        }
    }
    return (int64_t)(uintptr_t)self;
}

__attribute__((always_inline))
static inline bool
GaObj_EQUALS(GaObject *self, GaContext *vm, GaObject *right)
{
    for (GaObject *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->equals) {
            return i->obj_ops->equals(i, vm, right);
        }
        GaObject *op = GaObj_XINC_REF(GaObj_GETATTR(self, vm, "__equals__"));
        if (op) {
            GaObject *ret = GaObj_INVOKE(op, vm, 1, &right);
            GaObj_DEC_REF(op);
            return GaObj_IS_TRUE(ret, vm);
        }
    }
    return self == right;
}

__attribute__((always_inline))
static inline bool
GaObj_MATCH(GaObject *self, GaContext *vm, GaObject *right)
{
    for (GaObject *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->match) {
            return i->obj_ops->match(i, vm, right);
        }
        GaObject *op = GaObj_XINC_REF(GaObj_GETATTR(self, vm, "__match__"));
        if (op) {
            GaObject *ret = GaObj_INVOKE(op, vm, 1, &right);
            GaObj_DEC_REF(op);
            return GaObj_IS_TRUE(ret, vm);
        }
    }

    return GaObj_EQUALS(self, vm, right);
}

__attribute__((always_inline))
static inline GaObject *
GaObj_LEN(GaObject *self, GaContext *vm)
{
    for (GaObject *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->len) {
            return i->obj_ops->len(i, vm);
        }
        GaObject *op = GaObj_XINC_REF(GaObj_GETATTR(self, vm, "__len__"));
        if (op) {
            GaObject *ret = GaObj_INVOKE(op, vm, 0, NULL);
            GaObj_DEC_REF(op);
            return ret;
        }
    }
    GaEval_RaiseException(vm, GaErr_NewOperatorError("__len__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline GaObject *
GaObj_LOGICAL_NOT(GaObject *self, GaContext *vm)
{
    for (GaObject *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->logical_not) {
            return i->obj_ops->logical_not(i, vm);
        }
        GaObject *op = GaObj_XINC_REF(GaObj_GETATTR(self, vm, "__not__"));
        if (op) {
            GaObject *ret = GaObj_INVOKE(op, vm, 0, NULL);
            GaObj_DEC_REF(op);
            return ret;
        }
    }
    GaEval_RaiseException(vm, GaErr_NewOperatorError("__not__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline GaObject *
GaObj_NEGATE(GaObject *self, GaContext *vm)
{
    for (GaObject *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->negate) {
            return i->obj_ops->negate(i, vm);
        }
        GaObject *op = GaObj_XINC_REF(GaObj_GETATTR(self, vm, "__negate__"));
        if (op) {
            GaObject *ret = GaObj_INVOKE(op, vm, 0, NULL);
            GaObj_DEC_REF(op);
            return ret;
        }
    }
    GaEval_RaiseException(vm, GaErr_NewOperatorError("__negate__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline GaObject *
GaObj_NOT(GaObject *self, GaContext *vm)
{
    for (GaObject *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->inverse) {
            return i->obj_ops->inverse(i, vm);
        }
        GaObject *op = GaObj_XINC_REF(GaObj_GETATTR(self, vm, "__inverse__"));
        if (op) {
            GaObject *ret = GaObj_INVOKE(op, vm, 0, NULL);
            GaObj_DEC_REF(op);
            return ret;
        }
    }
    GaEval_RaiseException(vm, GaErr_NewOperatorError("__inverse__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline bool
GaObj_GT(GaObject *self, GaContext *vm, GaObject *right)
{
    for (GaObject *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->gt) {
            return i->obj_ops->gt(i, vm, right);
        }
        GaObject *op = GaObj_XINC_REF(GaObj_GETATTR(self, vm, "__gt__"));
        if (op) {
            GaObject *ret = GaObj_INVOKE(op, vm, 1, &right);
            GaObj_DEC_REF(op);
            return GaObj_IS_TRUE(ret, vm);
        }
    }
    GaEval_RaiseException(vm, GaErr_NewOperatorError("__gt__ is not implemented"));
    return false;
}

__attribute__((always_inline))
static inline bool
GaObj_GE(GaObject *self, GaContext *vm, GaObject *right)
{
    for (GaObject *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->ge) {
            return i->obj_ops->ge(i, vm, right);
        }
        GaObject *op = GaObj_XINC_REF(GaObj_GETATTR(self, vm, "__ge__"));
        if (op) {
            GaObject *ret = GaObj_INVOKE(op, vm, 1, &right);
            GaObj_DEC_REF(op);
            return GaObj_IS_TRUE(ret, vm);
        }
    }
    GaEval_RaiseException(vm, GaErr_NewOperatorError("__ge__ is not implemented"));
    return false;
}

__attribute__((always_inline))
static inline bool
GaObj_LT(GaObject *self, GaContext *vm, GaObject *right)
{
    for (GaObject *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->lt) {
            return i->obj_ops->lt(i, vm, right);
        }
        GaObject *op = GaObj_XINC_REF(GaObj_GETATTR(self, vm, "__lt__"));
        if (op) {
            GaObject *ret = GaObj_INVOKE(op, vm, 1, &right);
            GaObj_DEC_REF(op);
            return GaObj_IS_TRUE(ret, vm);
        }
    }
    GaEval_RaiseException(vm, GaErr_NewOperatorError("__lt__ is not implemented"));
    return false;
}

__attribute__((always_inline))
static inline bool
GaObj_LE(GaObject *self, GaContext *vm, GaObject *right)
{
    for (GaObject *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->le) {
            return i->obj_ops->le(i, vm, right);
        }
        GaObject *op = GaObj_XINC_REF(GaObj_GETATTR(self, vm, "__le__"));
        if (op) {
            GaObject *ret = GaObj_INVOKE(op, vm, 1, &right);
            GaObj_DEC_REF(op);
            return GaObj_IS_TRUE(ret, vm);
        }
    }
    GaEval_RaiseException(vm, GaErr_NewOperatorError("__le__ is not implemented"));
    return false;
}

__attribute__((always_inline))
static inline GaObject *
GaObj_ADD(GaObject *self, GaContext *vm, GaObject *right)
{
    for (GaObject *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->add) {
            return i->obj_ops->add(i, vm, right);
        }

        GaObject *operator = GaObj_XINC_REF(GaObj_GETATTR(self, vm, "__add__"));

        if (operator) {
            GaObject *ret = GaObj_INVOKE(operator, vm, 1, &right);
            GaObj_DEC_REF(operator);
            return ret;
        }
    }
    GaEval_RaiseException(vm, GaErr_NewOperatorError("__add__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline GaObject *
GaObj_SUB(GaObject *self, GaContext *vm, GaObject *right)
{
    for (GaObject *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->sub) {
            return i->obj_ops->sub(i, vm, right);
        }
        GaObject *op = GaObj_XINC_REF(GaObj_GETATTR(self, vm, "__sub__"));
        if (op) {
            GaObject *ret = GaObj_INVOKE(op, vm, 1, &right);
            GaObj_DEC_REF(op);
            return ret;
        }
    }
    GaEval_RaiseException(vm, GaErr_NewOperatorError("__sub__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline GaObject *
GaObj_MUL(GaObject *self, GaContext *vm, GaObject *right)
{
    for (GaObject *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->mul) {
            return i->obj_ops->mul(i, vm, right);
        }
        GaObject *op = GaObj_XINC_REF(GaObj_GETATTR(self, vm, "__mul__"));
        if (op) {
            GaObject *ret = GaObj_INVOKE(op, vm, 1, &right);
            GaObj_DEC_REF(op);
            return ret;
        }
    }
    GaEval_RaiseException(vm, GaErr_NewOperatorError("__mul__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline GaObject *
GaObj_DIV(GaObject *self, GaContext *vm, GaObject *right)
{
    for (GaObject *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->div) {
            return i->obj_ops->div(i, vm, right);
        }
        GaObject *op = GaObj_XINC_REF(GaObj_GETATTR(self, vm, "__div__"));
        if (op) {
            GaObject *ret = GaObj_INVOKE(op, vm, 1, &right);
            GaObj_DEC_REF(op);
            return ret;
        }
    }
    GaEval_RaiseException(vm, GaErr_NewOperatorError("__div__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline GaObject *
GaObj_MOD(GaObject *self, GaContext *vm, GaObject *right)
{
    for (GaObject *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->mod) {
            return i->obj_ops->mod(i, vm, right);
        }
        GaObject *op = GaObj_XINC_REF(GaObj_GETATTR(self, vm, "__mod__"));
        if (op) {
            GaObject *ret = GaObj_INVOKE(op, vm, 1, &right);
            GaObj_DEC_REF(op);
            return ret;
        }
    }
    GaEval_RaiseException(vm, GaErr_NewOperatorError("__mod__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline GaObject *
GaObj_AND(GaObject *self, GaContext *vm, GaObject *right)
{
    for (GaObject *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->band) {
            return i->obj_ops->band(i, vm, right);
        }
        GaObject *op = GaObj_XINC_REF(GaObj_GETATTR(self, vm, "__and__"));
        if (op) {
            GaObject *ret = GaObj_INVOKE(op, vm, 1, &right);
            GaObj_DEC_REF(op);
            return ret;
        }
    }
    GaEval_RaiseException(vm, GaErr_NewOperatorError("__and__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline GaObject *
GaObj_OR(GaObject *self, GaContext *vm, GaObject *right)
{
    for (GaObject *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->bor) {
            return i->obj_ops->bor(i, vm, right);
        }
        GaObject *op = GaObj_XINC_REF(GaObj_GETATTR(self, vm, "__or__"));
        if (op) {
            GaObject *ret = GaObj_INVOKE(op, vm, 1, &right);
            GaObj_DEC_REF(op);
            return ret;
        }
    }
    GaEval_RaiseException(vm, GaErr_NewOperatorError("__or__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline GaObject *
GaObj_XOR(GaObject *self, GaContext *vm, GaObject *right)
{
    for (GaObject *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->bxor) {
            return i->obj_ops->bxor(i, vm, right);
        }
        GaObject *op = GaObj_XINC_REF(GaObj_GETATTR(self, vm, "__xor__"));
        if (op) {
            GaObject *ret = GaObj_INVOKE(op, vm, 1, &right);
            GaObj_DEC_REF(op);
            return ret;
        }
    }
    GaEval_RaiseException(vm, GaErr_NewOperatorError("__xor__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline GaObject *
GaObj_SHL(GaObject *self, GaContext *vm, GaObject *right)
{
    for (GaObject *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->shl) {
            return i->obj_ops->shl(i, vm, right);
        }
        GaObject *op = GaObj_XINC_REF(GaObj_GETATTR(self, vm, "__shl__"));
        if (op) {
            GaObject *ret = GaObj_INVOKE(op, vm, 1, &right);
            GaObj_DEC_REF(op);
            return ret;
        }
    }
    GaEval_RaiseException(vm, GaErr_NewOperatorError("__shl__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline GaObject *
GaObj_SHR(GaObject *self, GaContext *vm, GaObject *right)
{
    for (GaObject *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->shr) {
            return i->obj_ops->shr(i, vm, right);
        }
        GaObject *op = GaObj_XINC_REF(GaObj_GETATTR(self, vm, "__shr__"));
        if (op) {
            GaObject *ret = GaObj_INVOKE(op, vm, 1, &right);
            GaObj_DEC_REF(op);
            return ret;
        }
    }
    GaEval_RaiseException(vm, GaErr_NewOperatorError("__shr__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline GaObject *
GaObj_CLOSED_RANGE(GaObject *self, GaContext *vm, GaObject *right)
{
    for (GaObject *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->closed_range) {
            return i->obj_ops->closed_range(i, vm, right);
        }
        GaObject *op = GaObj_XINC_REF(GaObj_GETATTR(self, vm, "__closed_range__"));
        if (op) {
            GaObject *ret = GaObj_INVOKE(op, vm, 1, &right);
            GaObj_DEC_REF(op);
            return ret;
        }
    }
    GaEval_RaiseException(vm, GaErr_NewOperatorError("__closed_range__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline GaObject *
GaObj_HALF_RANGE(GaObject *self, GaContext *vm, GaObject *right)
{
    for (GaObject *i = self; i; i = i->super) {
        if (i->obj_ops && i->obj_ops->half_range) {
            return i->obj_ops->half_range(i, vm, right);
        }
        GaObject *op = GaObj_XINC_REF(GaObj_GETATTR(self, vm, "__half_range__"));
        if (op) {
            GaObject *ret = GaObj_INVOKE(op, vm, 1, &right);
            GaObj_DEC_REF(op);
            return ret;
        }
    }
    GaEval_RaiseException(vm, GaErr_NewOperatorError("__half_range__ is not implemented"));
    return NULL;
}

__attribute__((always_inline))
static inline void
GaObj_WEAKREF_ADD(GaObject *obj, GaObject **ref)
{
    if (!obj->weak_refs) {
        obj->weak_refs = _Ga_list_new();
    }
    *ref = obj;
    _Ga_list_push(obj->weak_refs, ref);
}

__attribute__((always_inline))
static inline void
GaObj_WEAKREF_DEL(GaObject **ref)
{
    extern GaObject _GaNull;
    GaObject *obj = *ref;

    if (obj == &_GaNull) {
        return;
    }
    
    _Ga_list_remove(obj->weak_refs, ref, NULL, NULL);
}
#endif