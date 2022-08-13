#ifndef _GALLIUM_BUILTINS_H
#define _GALLIUM_BUILTINS_H

#include <stdint.h>
#include <gallium/list.h>
#include <gallium/object.h>
#include <gallium/vm.h>
#include <sys/types.h>

#define GA_BUILTIN_TYPE_DECL(var_name,type_name,ctr) GaObject var_name = { \
    .ref_count = 1, \
    .type      = &ga_type_type_inst, \
    .un.statep = type_name, \
    .obj_ops   = &(struct Ga_Operators) { .invoke = ctr, .match = _GaType_Match } \
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

typedef GaObject * (*GaCFunc) (GaContext *, int, GaObject **);

/* builtin constants */
extern GaObject    _GaTrue;
extern GaObject    _GaFalse;
extern GaObject    _GaNull;

#define Ga_TRUE    (&_GaTrue)
#define Ga_FALSE   (&_GaFalse)
#define Ga_NULL    (&_GaNull)

/*
 * builtin type definitions. These symbols are subject to change and should
 * not be used! Use the macros below instead
 */
extern GaObject    _GaObj_Type;
extern GaObject    _GaBuiltin_Type;
extern GaObject    _GaClass_Type;
extern GaObject    _GaDict_Type;
extern GaObject    _GaFunc_Type;
extern GaObject    _GaInt_Type;
extern GaObject *   _GaList_type;
extern GaObject *   _GaMutStr_type;
extern GaObject    _GaRange_Type;
extern GaObject *   _GaStr_type;
extern GaObject    _GaWeakRef_Type;
extern GaObject *   _GaAst_type;
extern GaObject *   _GaCode_type;
extern GaObject *   _GaFile_type;
extern GaObject *   _GaParser_type;
extern GaObject     _GaFloat_Type;

/* Constants for GaFunc */
#define             GaFunc_VARIADIC     0x01
#define             GaFunc_HAS_KWARGS   0x02

/* builtin-type macros (The symbol definitions are subject to change) */
#define GA_OBJECT_TYPE      (&_GaObj_Type)
#define GA_BUILTIN_TYPE     (&_GaBuiltin_Type)
#define GA_CLASS_TYPE       (&_GaClass_Type)
#define GA_DICT_TYPE        (&_GaDict_Type)
#define GA_FUNC_TYPE        (&_GaFunc_Type)
#define GA_INT_TYPE         (&_GaInt_Type)
#define GA_RANGE_TYPE       (&_GaRange_Type)
#define GA_WEAKREF_TYPE     (&_GaWeakRef_Type)
#define Ga_FLOAT_TYPE       (&_GaFloat_Type)
#define GA_AST_TYPE         (_GaAst_type)
#define GA_CODE_TYPE        (_GaCode_type)
#define GA_FILE_TYPE        (_GaFile_type)
#define GA_PARSER_TYPE      (_GaParser_type)
#define GA_LIST_TYPE        (_GaList_type)
#define GA_MUTSTR_TYPE      (_GaMutStr_type)
#define GA_STR_TYPE         (_GaStr_type)

/* builtin modules */
GaObject        *   GaMod_OpenAst();
struct Ga_Object	*   GaMod_OpenBuiltins();
GaObject        *   GaMod_OpenOS();
GaObject        *   GaMod_OpenParser();

/* builtin exception constructors */
GaObject        *   GaErr_NewArgumentError(const char *, ...);
GaObject        *   GaErr_NewAttributeError(const char *);
GaObject        *   GaErr_NewIOError(const char *);
GaObject        *   GaErr_NewImportError(const char *);
GaObject        *   GaErr_NewIndexError(const char *);
GaObject        *   GaErr_NewInternalError(const char*);
GaObject        *   GaErr_NewKeyError();
GaObject        *   GaErr_NewNameError(const char *);
GaObject        *   GaErr_NewOperatorError(const char *);
GaObject        *   GaErr_NewTypeError(const char *, ...);
GaObject        *   GaErr_NewValueError(const char *);
GaObject        *   GaErr_NewSyntaxError(const char *, ...);

GaObject        *   GaAstNode_CompileInline(GaContext *, GaObject *, struct ga_proc *);
GaObject        *   GaAstNode_New(struct ast_node *, _Ga_list_t *);

GaObject        *   GaBuiltin_New(GaCFunc, GaObject *);

GaObject        *   GaClass_Base(GaObject *);
GaObject        *   GaClass_New(const char *, GaObject *, GaObject *, GaObject *);

GaObject        *   GaClosure_New(struct stackframe *, GaObject *,
                                  struct ga_proc *, struct ga_proc *);

GaObject        *   GaCode_Eval(GaContext *, GaObject *, struct stackframe *);
GaObject        *   GaCode_New(const char *);
struct ga_proc  *   GaCode_GetProc(GaObject *);
GaObject        *   GaEnum_New(const char *, GaObject *);
GaObject        *   GaEnumerable_New();

GaObject        *   GaMixin_New(GaObject *);

GaObject        *   GaFile_New(int, mode_t);

GaObject        *   GaFunc_New(GaObject *, struct ga_proc *, struct ga_proc *);
void                GaFunc_AddParam(GaObject *, const char *, int);

GaObject        *   GaList_New();
void                GaList_Append(GaObject *, GaObject *);
void                GaList_Remove(GaObject *, GaContext *, GaObject *);
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
void                GaDict_GetITer(GaObject *, _Ga_iter_t *);

GaObject        *   ga_tokenstream_new(_Ga_list_t *);

GaObject        *   GaTuple_New(int nelems);
GaObject        *   GaTuple_GetElem(GaObject *self, int elem);
int                 GaTuple_GetSize(GaObject *self);
void                GaTuple_InitElem(GaObject *self, int elem, GaObject *obj);

GaObject        *   GaModule_New(const char *, GaObject *, const char *);
GaObject        *   GaModule_Open(GaObject *, GaContext *, const char *);
void                GaModule_Import(GaObject *, GaContext *, GaObject *);
void                GaModule_SetConstructor(GaObject *, GaObject *);

GaObject        *   GaWeakRef_New(GaObject *);
GaObject        *   GaWeakRef_Val(GaObject *);

/* These functions are for allocating type objects internally and should not be used! */
GaObject        *   _GaStr_init();
void                _GaStr_fini();
GaObject        *   _GaMutStr_init();
void                _GaMutStr_fini();
GaObject        *   _GaFile_init();
void                _GaFile_fini();
GaObject        *   _GaList_init();
void                _GaList_fini();
GaObject        *   _GaCode_init();
void                _GaCode_fini();
GaObject        *   _GaAst_init();
void                _GaAst_fini();
GaObject        *   _GaParser_init();
void                _GaParser_fini();

static inline GaObject *
Ga_ENSURE_HAS_ITER(GaContext *ctx, GaObject *obj)
{
    GaObject *iter = GaObj_ITER(obj, ctx);

    if (!iter && !GaEval_HAS_THROWN_EXCEPTION(ctx)) {
        GaObject *e = GaErr_NewTypeError("Type '%s' is not iterable",
                                         GaObj_TypeName(obj->type));
        GaEval_RaiseException(ctx, e);
    }

    return iter;
}

static inline GaObject *
Ga_ENSURE_HAS_CUR(GaContext *ctx, GaObject *obj)
{
    GaObject *iter = GaObj_ITER_CUR(obj, ctx);

    if (!iter && !GaEval_HAS_THROWN_EXCEPTION(ctx)) {
        GaObject *e = GaErr_NewTypeError("Type '%s' is not iterable",
                                         GaObj_TypeName(obj->type));
        GaEval_RaiseException(ctx, e);
    }

    return iter;
}

static inline GaObject *
Ga_ENSURE_TYPE(GaContext *ctx, GaObject *obj, GaObject *type)
{
    GaObject *ret = GaObj_Super(obj, type);
    if (!ret) {
        GaObject *e = GaErr_NewTypeError("Expected argument of type %s, "\
                                         "but is %s", GaObj_TypeName(type),
                                          GaObj_TypeName(obj->type));
        GaEval_RaiseException(ctx, e);
    }
    return ret;
}

static inline bool
Ga_CHECK_ARG_COUNT_EXACT(GaContext *ctx, int required_argc, int argc)
{
    if (required_argc != argc) {
        GaObject *e = GaErr_NewArgumentError("Expected %d argument(s), but "\
                                             "got %d.", required_argc, argc);
        GaEval_RaiseException(ctx, e);
        return false;
    }
    return true;
}

static inline bool
Ga_CHECK_ARG_COUNT_MIN(GaContext *ctx, int min_args, int argc)
{
    if (argc < min_args) {
        GaObject *e = GaErr_NewArgumentError("Expected at least %d "\
                                             "argument(s), but got %d",
                                             min_args, argc);
        GaEval_RaiseException(ctx, e);
        return false;
    }
    return true;
}

static inline bool
Ga_CHECK_ARGS_OPTIONAL(GaContext *ctx, int min_args, GaObject **types,
                       int argc, GaObject **args)
{
    if (argc < min_args) {
        GaEval_RaiseException(ctx, GaErr_NewArgumentError("Expected at least %d argument(s), but got %d.", min_args, argc));
        return false;
    }

    for (int i = 0; i < argc && types[i]; i++) {
        if (!Ga_ENSURE_TYPE(ctx, args[i], types[i])) return false;
    }

    return true;
}

static inline bool
Ga_CHECK_ARGS_EXACT(GaContext *ctx, int required_argc, GaObject **types,
                    int argc, GaObject **args)
{
    if (required_argc != argc) {
        GaObject *e = GaErr_NewArgumentError("Expected %d argument(s), but "\
                                             "got %d.", required_argc, argc);
        GaEval_RaiseException(ctx, e);
        return false;
    }

    for (int i = 0; i < argc; i++) {
        if (!Ga_ENSURE_TYPE(ctx, args[i], types[i])) return false;
    }

    return true;
}

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
    extern struct Ga_Operators Ga_IntOps;
    GaObject *obj = GaObj_New(&_GaInt_Type, &Ga_IntOps);
    obj->un.state_i64 = val;
    return obj;
}

static inline int64_t
GaInt_TO_I64(GaObject *obj)
{
    return obj->un.state_i64;
}

static inline GaObject *
GaFloat_FROM_DOUBLE(double val)
{
    extern struct Ga_Operators Ga_FloatOps;
    GaObject *obj = GaObj_New(&_GaFloat_Type, &Ga_FloatOps);
    obj->un.state_f64 = val;
    return obj;
}

static inline double
GaFloat_TO_DOUBLE(GaObject *obj)
{
    return obj->un.state_f64;
}


#endif