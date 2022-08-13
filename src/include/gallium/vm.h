#ifndef _RUNTIME_VM_H
#define _RUNTIME_VM_H

#include <stdbool.h>
#include <gallium/code.h>
#include <gallium/dict.h>
#include <gallium/pool.h>
#include <gallium/object.h>
#include <gallium/vec.h>

#define Ga_ARGUMENT_MAX         16
#define VM_UNHANDLED_EXCEPTION  1
#define VM_STACK_CORRUPTION     2
#define VM_STACK_OVERFLOW       3
#define VM_STACK_UNDERFLOW      4

#define VM_STACK_FASTCELL_MAX       128
#define VM_STACK_MAX                255
#define VM_EXCEPTION_HANDLER_MAX    32
#define VM_DISPOSABLE_MAX           32

struct stackframe {
    GaObject            *   stack[VM_STACK_MAX];
    GaObject            *   fast_cells[VM_STACK_FASTCELL_MAX];  /* local variables */
    GaObject            *   mod; /* calling module */
    GaCodeObject        *   code;
    GaContext           *   vm;
    struct stackframe   *   parent;     /* the caller's stackframe */
    struct stackframe   *   captive;    /* stackframe we captured with a closure */

    int                     locals_count;   /* how many locals have been set */

    /* stack of exception handlers */
    int                     exception_stack[VM_EXCEPTION_HANDLER_MAX];
    int                     exception_stack_top;

    /* Objects to be disposed at the end of a "with" statement */
    GaObject            *   disposable_stack[VM_DISPOSABLE_MAX];
    int                     disposable_stack_top;

    int                     pending_exception_handler;
    GaObject            *   exception_obj;

    volatile bool       *   interrupt_flag_ptr;

    /*
     * We use reference counting to prevent the stackframe from being de-allocated
     * when its referenced by a closure
     */
    int                     ref_count;
};


#define GaEval_HAS_THROWN_EXCEPTION(v)     ((v)->unhandled_exception)

#define VM_SET_INTERRUPT(s)    *(s)->interrupt_flag_ptr=1;

typedef struct ga_context {
    GaObject            *   default_module;
    struct stackframe   *   top;
    bool                    unhandled_exception;
    int                     vm_errno;
    GaObject            *   error;
    GaObject            *   globals;            /* The "builtins" module */
    _Ga_dict_t              import_cache;       /* All imported modules */
} GaContext;


extern struct pool ga_vm_stackframe_pool;

void            GaEval_RaiseException(GaContext *, GaObject *);
GaObject    *   GaEval_ExecFrame(GaContext *, struct stackframe *, int argc,
                                 GaObject **);

__attribute__((always_inline))
static inline void
GaFrame_DESTROY(struct stackframe *frame)
{
    while (frame) {
        frame->ref_count--;
        if (frame->ref_count > 0) break;
        if (frame->code) {
            for (int i = frame->code->locals_start;
                i < frame->code->locals_end;
                i++)
            {
                GaObj_XDEC_REF(frame->fast_cells[i]);
            }
        }
        GaPool_PUT(&ga_vm_stackframe_pool, frame);
        frame = frame->captive;
    }
}

__attribute__((always_inline))
static inline struct stackframe *
GaFrame_NEW(GaObject *mod, GaCodeObject *code, struct stackframe *captive)
{
    struct stackframe *frame = GaPool_GET(&ga_vm_stackframe_pool);

    frame->code = code;
    frame->mod = mod;
    frame->ref_count = 1;
    frame->vm = NULL;
    frame->parent = NULL;
    frame->captive = NULL;
    frame->exception_stack_top = 0; 
    frame->pending_exception_handler = 0;
    frame->disposable_stack_top = 0;

    /*
     * This memset can be incredibly expensive with the interpreter speed so
     * we'll try to optimize it by only initializing the slots that wil
     * actually be used. For situations where that is unknown (IE: REPL) I'm
     * going to clear the entire thing.
     */
    if (frame->code)
        memset(frame->fast_cells, 0,
               sizeof(struct Ga_Object*) * frame->code->locals_end);
    else
        memset(frame->fast_cells, 0,
               sizeof(struct Ga_Object*) * VM_STACK_FASTCELL_MAX);

    if (captive) {
        captive->ref_count++;
        frame->captive = captive;
    } else {
        frame->captive = NULL;
    }
    return frame;
}

#endif