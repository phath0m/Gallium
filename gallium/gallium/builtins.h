#ifndef _GALLIUM_BUILTINS_H
#define _GALLIUM_BUILTINS_H

#include <stdint.h>
#include <gallium/list.h>
#include <gallium/object.h>
#include <sys/types.h>

#define GA_BUILTIN_TYPE_DECL(var_name,type_name,ctr) GaObject var_name = { \
    .ref_count = 1, \
    .type      = &ga_type_type_inst, \
    .un.statep = type_name, \
    .obj_ops = &(struct ga_obj_ops) { .invoke = ctr, .match = _GaType_Match } \
};

struct stackframe;
struct stringbuf;
struct ga_proc;
struct ga_mod_data;
struct ast_node;

struct ga_dict_kvp {
    GaObject   *   key;
    GaObject   *   val;
};

typedef GaObject * (*GaCFunc) (GaObject *, struct vm *, int, GaObject **);

/* builtin constants */
extern GaObject    _GaTrue;
extern GaObject    _GaFalse;
extern GaObject    _GaNull;

#define Ga_TRUE    (&_GaTrue)
#define Ga_FALSE   (&_GaFalse)
#define Ga_NULL    (&_GaNull)

/* builtin type definitions */
extern GaObject    _GaObj_Type;
extern GaObject    _GaBuiltin_Type;
extern GaObject    _GaClass_Type;
extern GaObject    _GaDict_Type;
extern GaObject    _GaFunc_Type;
extern GaObject    _GaInt_Type;
extern GaObject    _GaList_Type;
extern GaObject    _GaMutStr_Type;
extern GaObject    _GaRange_Type;
extern GaObject    _GaStr_Type;
extern GaObject    _GaWeakRef_Type;
extern GaObject    _GaAstNode_Type;

/* builtin-type macros */
#define GA_OBJECT_TYPE      (&_GaObj_Type)
#define GA_BUILTIN_TYPE     (&_GaBuiltin_Type)
#define GA_CLASS_TYPE       (&_GaClass_Type)
#define GA_DICT_TYPE        (&_GaDict_Type)
#define GA_FUNC_TYPE        (&_GaFunc_Type)
#define GA_INT_TYPE         (&_GaInt_Type)
#define GA_LIST_TYPE        (&_GaList_Type)
#define GA_MUTSTR_TYPE      (&_GaMutStr_Type)
#define GA_RANGE_TYPE       (&_GaRange_Type)
#define GA_STR_TYPE         (&_GaStr_Type)
#define GA_WEAKREF_TYPE     (&_GaWeakRef_Type)
#define GA_AST_TYPE         (&_GaAstNode_Type)

/* builtin modules */
GaObject        *   GaMod_OpenAst();
struct ga_obj	*   GaMod_OpenBuiltins();
GaObject        *   GaMod_OpenOS();
GaObject        *   GaMod_OpenParser();

/* builtin exception constructors */
GaObject        *   GaErr_NewArgumentError(const char *);
GaObject        *   GaErr_NewAttributeError(const char *);
GaObject        *   GaErr_NewIOError(const char *);
GaObject        *   GaErr_NewImportError(const char *);
GaObject        *   GaErr_NewIndexError(const char *);
GaObject        *   GaErr_NewInternalError(const char*);
GaObject        *   GaErr_NewKeyError();
GaObject        *   GaErr_NewNameError(const char *);
GaObject        *   GaErr_NewOperatorError(const char *);
GaObject        *   GaErr_NewTypeError(const char *);
GaObject        *   GaErr_NewValueError(const char *);
GaObject        *   GaErr_NewSyntaxError(const char *);

GaObject        *   GaAstNode_CompileInline(GaObject *, struct ga_proc *);
struct ast_node *   GaAstNode_Val(GaObject *);
GaObject        *   GaAstNode_New(struct ast_node *, struct list *);

GaObject        *   GaBuiltin_New(GaCFunc, GaObject *);

GaObject        *   GaClass_Base(GaObject *);
GaObject        *   GaClass_New(const char *, GaObject *, GaObject *, GaObject *);

GaObject        *   GaClosure_New(struct stackframe *, GaObject *,
                                  struct ga_proc *, struct ga_proc *);

GaObject        *   GaCode_InvokeInline(struct vm *, GaObject *,
                                        struct stackframe *);
GaObject        *   GaCode_New(struct ga_proc *, struct ga_mod_data *);
struct ga_proc  *   GaCode_GetProc(GaObject *);
GaObject        *   GaEnum_New(const char *, GaObject *);
GaObject        *   GaEnumerable_New();

GaObject        *   GaMixin_New(GaObject *);

GaObject        *   GaFile_New(int, mode_t);

GaObject        *   GaFunc_New(GaObject *, struct ga_proc *, struct ga_proc *);
void                GaFunc_AddParam(GaObject *, const char *, int);

GaObject        *   GaList_New();
void                GaList_Append(GaObject *, GaObject *);
void                GaList_Remove(GaObject *, struct vm *, GaObject *);
int                 GaList_Size(GaObject *);

GaObject        *   GaMethod_New(GaObject *, GaObject *);

GaObject        *   GaRange_New(int64_t, int64_t, int64_t);

GaObject        *   GaStr_FromCString(const char *);
GaObject        *   GaStr_FromCStringEx(const char *, size_t);
GaObject        *   GaStr_FromStringBuilder(struct stringbuf *);
size_t              GaStr_Len(GaObject *);
const char      *   GaStr_ToCString(GaObject *);
struct stringbuf*   GaStr_ToStringBuilder(GaObject *);

GaObject        *   GaDict_New();
void                GaDict_GetITer(GaObject *, list_iter_t *);

GaObject        *   ga_tokenstream_new(struct list *);

GaObject        *   GaTuple_New(int nelems);
GaObject        *   GaTuple_GetElem(GaObject *self, int elem);
int                 GaTuple_GetSize(GaObject *self);
void                GaTuple_InitElem(GaObject *self, int elem, GaObject *obj);

GaObject        *   GaModule_New(const char *, GaObject *, const char *);
GaObject        *   GaModule_Open(GaObject *, struct vm *, const char *);
void                GaModule_Import(GaObject *, struct vm *, GaObject *);

GaObject        *   GaWeakRef_New(GaObject *);
GaObject        *   GaWeakRef_Val(GaObject *);

static inline GaObject *
GaBool_FROM_BOOL(bool b)
{
    return b ? &_GaTrue : &_GaFalse;
}

static inline bool
GaBool_TO_BOOL(GaObject *obj)
{
    return obj->un.state_i8 != 0;
}

static inline GaObject *
GaInt_FROM_I64(int64_t val)
{
    extern struct ga_obj_ops Ga_IntOps;
    GaObject *obj = GaObj_New(&_GaInt_Type, &Ga_IntOps);
    obj->un.state_i64 = val;
    return obj;
}

static inline int64_t
GaInt_TO_I64(GaObject *obj)
{
    return obj->un.state_i64;
}
#endif
