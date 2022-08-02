#ifndef _RUNTIME_VM_H
#define _RUNTIME_VM_H

#include <stdbool.h>
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

typedef uint64_t ga_ins_t;

#define GA_INS_MAKE(opcode, imm) ((opcode) | (imm << 8))
#define GA_INS_OPCODE(ins) ((ins) & 0x7F)
#define GA_INS_OPARG(ins)   ((ins & 0x80) >> 7)
#define GA_INS_IMMEDIATE(ins) ((ins) >> 8)

/* data associated with a module (Constants, procs, ect) */
struct ga_mod_data {
    struct vec          object_pool;
    struct vec          string_pool;
    struct vec          proc_pool;
};


/* bytecode "procdure" (series of bytecode instructions*/
struct ga_proc {
    ga_ins_t            *   bytecode;
    void                *   compiler_private;
    struct ga_mod_data  *   data;
    GaObject            *   obj; /* The actual code object (This is a temporary hack, I will refactor this... I hope) */
    int                     bytecode_len;
    int                     locals_start;
    int                     locals_end;
    int                     size;
    char                    name[];
};

/* string pool entry */
struct ga_string_pool_entry {
    uint32_t    hash;
    char        value[];
};

void    ga_proc_destroy(struct ga_proc *);

struct stackframe {
    GaObject            *   stack[VM_STACK_MAX];
    GaObject            *   fast_cells[VM_STACK_FASTCELL_MAX];  /* local variables */
    GaObject            *   mod; /* calling module */
    struct ga_proc      *   code;
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
                i < frame->code->locals_end && frame->fast_cells[i];
                i++)
            {
                GaObj_DEC_REF(frame->fast_cells[i]);
            }
        }
        GaPool_PUT(&ga_vm_stackframe_pool, frame);
        frame = frame->captive;
    }
}

__attribute__((always_inline))
static inline struct stackframe *
GaFrame_NEW(GaObject *mod, struct ga_proc *code, struct stackframe *captive)
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

    /*
     * This memset can be incredibly expensive with the interpreter speed so
     * we'll try to optimize it by only initializing the slots that wil
     * actually be used. For situations where that is unknown (IE: REPL) I'm
     * going to clear the entire thing.
     */
    if (frame->code)
        memset(frame->fast_cells, 0,
               sizeof(struct ga_obj*) * frame->code->locals_end);
    else
        memset(frame->fast_cells, 0,
               sizeof(struct ga_obj*) * VM_STACK_FASTCELL_MAX);

    if (captive) {
        captive->ref_count++;
        frame->captive = captive;
    } else {
        frame->captive = NULL;
    }
    return frame;
}

#endif