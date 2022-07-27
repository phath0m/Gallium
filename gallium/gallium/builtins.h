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
extern GaObject    ga_bool_true_inst;
extern GaObject    ga_bool_false_inst;
extern GaObject    ga_null_inst;

#define GA_TRUE         (&ga_bool_true_inst)
#define GA_FALSE        (&ga_bool_false_inst)
#define GA_NULL         (&ga_null_inst)

/* builtin type definitions */
extern GaObject    ga_obj_type_inst;
extern GaObject    ga_cfunc_type_inst;
extern GaObject    ga_class_type_inst;
extern GaObject    ga_dict_type_inst;
extern GaObject    ga_func_type_inst;
extern GaObject    ga_int_type_inst;
extern GaObject    ga_list_type_inst;
extern GaObject    ga_mutstr_type_inst;
extern GaObject    ga_range_type_inst;
extern GaObject    ga_str_type_inst;
extern GaObject    ga_weakref_type_inst;
extern GaObject    ga_astnode_type_inst;

/* builtin-type macros */
#define GA_OBJECT_TYPE      (&ga_obj_type_inst)
#define GA_BUILTIN_TYPE     (&ga_cfunc_type_inst)
#define GA_CLASS_TYPE       (&ga_class_type_inst)
#define GA_DICT_TYPE        (&ga_dict_type_inst)
#define GA_FUNC_TYPE        (&ga_func_type_inst)
#define GA_INT_TYPE         (&ga_int_type_inst)
#define GA_LIST_TYPE        (&ga_list_type_inst)
#define GA_MUTSTR_TYPE      (&ga_mutstr_type_inst)
#define GA_RANGE_TYPE       (&ga_range_type_inst)
#define GA_STR_TYPE         (&ga_str_type_inst)
#define GA_WEAKREF_TYPE     (&ga_weakref_type_inst)
#define GA_AST_TYPE         (&ga_astnode_type_inst)

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

GaObject        *   ga_ast_node_compile_inline(GaObject *, struct ga_proc *);
struct ast_node *   ga_ast_node_val(GaObject *);
GaObject        *   ga_ast_node_new(struct ast_node *, struct list *);

GaObject        *   GaBuiltin_New(GaCFunc, GaObject *);

GaObject        *   GaClass_Base(GaObject *);
GaObject        *   GaClass_New(const char *, GaObject *, GaObject *, GaObject *);

GaObject        *   GaClosure_New(struct stackframe *, GaObject *, struct ga_proc *, struct ga_proc *);

GaObject        *   GaCode_InvokeInline(struct vm *, GaObject *, struct stackframe *);
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
    return b ? &ga_bool_true_inst : &ga_bool_false_inst;
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
    GaObject *obj = GaObj_New(&ga_int_type_inst, &Ga_IntOps);
    obj->un.state_i64 = val;
    return obj;
}

static inline int64_t
GaInt_TO_I64(GaObject *obj)
{
    return obj->un.state_i64;
}

#endif
