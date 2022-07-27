#ifndef _GALLIUM_BUILTINS_H
#define _GALLIUM_BUILTINS_H

#include <stdint.h>
#include <gallium/list.h>
#include <gallium/object.h>
#include <sys/types.h>

#define GA_BUILTIN_TYPE_DECL(var_name,type_name,ctr) struct ga_obj var_name = { \
    .ref_count = 1, \
    .type      = &ga_type_type_inst, \
    .un.statep = type_name, \
    .obj_ops = &(struct ga_obj_ops) { .invoke = ctr, .match = ga_type_match } \
};

struct stackframe;
struct stringbuf;
struct ga_proc;
struct ga_mod_data;
struct ast_node;

struct ga_dict_kvp {
    struct ga_obj   *   key;
    struct ga_obj   *   val;
};

typedef struct ga_obj * (*ga_cfunc_t) (struct ga_obj *, struct vm *, int, struct ga_obj **);

/* builtin constants */
extern struct ga_obj    ga_bool_true_inst;
extern struct ga_obj    ga_bool_false_inst;
extern struct ga_obj    ga_null_inst;

#define GA_TRUE         (&ga_bool_true_inst)
#define GA_FALSE        (&ga_bool_false_inst)
#define GA_NULL         (&ga_null_inst)

/* builtin type definitions */
extern struct ga_obj    ga_obj_type_inst;
extern struct ga_obj    ga_cfunc_type_inst;
extern struct ga_obj    ga_class_type_inst;
extern struct ga_obj    ga_dict_type_inst;
extern struct ga_obj    ga_func_type_inst;
extern struct ga_obj    ga_int_type_inst;
extern struct ga_obj    ga_list_type_inst;
extern struct ga_obj    ga_mutstr_type_inst;
extern struct ga_obj    ga_range_type_inst;
extern struct ga_obj    ga_str_type_inst;
extern struct ga_obj    ga_weakref_type_inst;
extern struct ga_obj    ga_astnode_type_inst;

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
struct ga_obj   *       GaMod_OpenAst();
struct ga_obj	*		GaMod_OpenBuiltins();
struct ga_obj   *       GaMod_OpenOS();
struct ga_obj   *       GaMod_OpenParser();

/* builtin exception constructors */
struct ga_obj   *       GaErr_NewArgumentError(const char *);
struct ga_obj   *       GaErr_NewAttributeError(const char *);
struct ga_obj   *       GaErr_NewIOError(const char *);
struct ga_obj   *       GaErr_NewImportError(const char *);
struct ga_obj   *       GaErr_NewIndexError(const char *);
struct ga_obj   *       GaErr_NewInternalError(const char*);
struct ga_obj   *       GaErr_NewKeyError();
struct ga_obj   *       GaErr_NewNameError(const char *);
struct ga_obj   *       GaErr_NewOperatorError(const char *);
struct ga_obj   *       GaErr_NewTypeError(const char *);
struct ga_obj   *       GaErr_NewValueError(const char *);
struct ga_obj   *       GaErr_NewSyntaxError(const char *);

struct ga_obj   *       ga_ast_node_compile_inline(struct ga_obj *, struct ga_proc *);
struct ast_node *       ga_ast_node_val(struct ga_obj *);
struct ga_obj   *       ga_ast_node_new(struct ast_node *, struct list *);

struct ga_obj   *       GaBuiltin_New(ga_cfunc_t, struct ga_obj *);

struct ga_obj   *       GaClass_Base(struct ga_obj *);
struct ga_obj   *       GaClass_New(const char *, struct ga_obj *, struct ga_obj *, struct ga_obj *);

struct ga_obj   *       GaClosure_New(struct stackframe *, struct ga_obj *, struct ga_proc *, struct ga_proc *);

struct ga_obj   *       GaCode_InvokeInline(struct vm *, struct ga_obj *, struct stackframe *);
struct ga_obj   *       GaCode_New(struct ga_proc *, struct ga_mod_data *);
struct ga_proc  *       GaCode_GetProc(struct ga_obj *);

struct ga_obj   *       GaEnum_New(const char *, struct ga_obj *);
struct ga_obj   *       GaEnumerable_New();

struct ga_obj   *       GaMixin_New(struct ga_obj *);

struct ga_obj   *       GaFile_New(int, mode_t);

struct ga_obj   *       GaFunc_New(struct ga_obj *, struct ga_proc *, struct ga_proc *);
void                    GaFunc_AddParam(struct ga_obj *, const char *, int);

struct ga_obj   *       GaList_New();
void                    GaList_Append(struct ga_obj *, struct ga_obj *);
void                    GaList_Remove(struct ga_obj *, struct vm *, struct ga_obj *);
int                     GaList_Size(struct ga_obj *);

struct ga_obj   *       GaMethod_New(struct ga_obj *, struct ga_obj *);

struct ga_obj   *       GaRange_New(int64_t, int64_t, int64_t);

struct ga_obj   *       GaStr_FromCString(const char *);
struct ga_obj   *       GaStr_FromCStringEx(const char *, size_t);
struct ga_obj   *       GaStr_FromStringBuilder(struct stringbuf *);
size_t                  GaStr_Len(struct ga_obj *);
const char      *       GaStr_ToCString(struct ga_obj *);
struct stringbuf    *   GaStr_ToStringBuilder(struct ga_obj *);

struct ga_obj   *       GaDict_New();
void                    GaDict_GetITer(struct ga_obj *, list_iter_t *);

struct ga_obj   *       ga_tokenstream_new(struct list *);

struct ga_obj   *       GaTuple_New(int nelems);
struct ga_obj   *       GaTuple_GetElem(struct ga_obj *self, int elem);
int                     GaTuple_GetSize(struct ga_obj *self);
void                    GaTuple_InitElem(struct ga_obj *self, int elem, struct ga_obj *obj);

struct ga_obj   *       GaModule_New(const char *, struct ga_obj *, const char *);
struct ga_obj   *       GaModule_Open(struct ga_obj *, struct vm *, const char *);
void                    GaModule_Import(struct ga_obj *, struct vm *, struct ga_obj *);

struct ga_obj   *       GaWeakRef_New(struct ga_obj *);
struct ga_obj   *       GaWeakRef_Val(struct ga_obj *);

static inline struct ga_obj *
GaBool_FROM_BOOL(bool b)
{
    return b ? &ga_bool_true_inst : &ga_bool_false_inst;
}

static inline bool
GaBool_TO_BOOL(struct ga_obj *obj)
{
    return obj->un.state_i8 != 0;
}

static inline struct ga_obj *
GaInt_FROM_I64(int64_t val)
{
    extern struct ga_obj_ops int_obj_ops;
    struct ga_obj *obj = GaObj_New(&ga_int_type_inst, &int_obj_ops);
    obj->un.state_i64 = val;
    return obj;
}

static inline int64_t
GaInt_TO_I64(struct ga_obj *obj)
{
    return obj->un.state_i64;
}

#endif
