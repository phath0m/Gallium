#ifndef _RUNTIME_VM_H
#define _RUNTIME_VM_H

#include <stdbool.h>
#include <gallium/dict.h>
#include <gallium/pool.h>
#include <gallium/vec.h>

#define VM_UNHANDLED_EXCEPTION  1
#define VM_STACK_CORRUPTION     2
#define VM_STACK_OVERFLOW       3
#define VM_STACK_UNDERFLOW      4

#define VM_STACK_FASTCELL_MAX       128
#define VM_STACK_MAX                255
#define VM_EXCEPTION_HANDLER_MAX    32

typedef uint64_t ga_ins_t;

#define GA_INS_MAKE(opcode, imm) ((opcode) | (imm << 8))
#define GA_INS_OPCODE(ins) ((ins) & 0xFF)
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
    struct ga_obj       *   obj; /* The actual code object (This is a temporary hack, I will refactor this... I hope) */
    //struct ga_obj   *   mod;
    int                         locals_start;
    int                         size;
    char                        name[];
};

void    ga_proc_destroy(struct ga_proc *);

struct stackframe {
    struct ga_obj       *   stack[VM_STACK_MAX];
    struct ga_obj       *   fast_cells[VM_STACK_FASTCELL_MAX];  /* local variables */
    struct ga_obj       *   mod; /* calling module */
    struct ga_proc      *   code;
    struct vm           *   vm;
    struct stackframe   *   parent;     /* the caller's stackframe */
    struct stackframe   *   captive;    /* stackframe we captured with a closure */

    /* stack of exception handlers */
    int                     exception_stack[VM_EXCEPTION_HANDLER_MAX];
    int                     exception_stack_top;

    int                     pending_exception_handler;
    struct ga_obj       *   exception_obj;

    volatile bool       *   interrupt_flag_ptr;

    /*
     * We use reference counting to prevent the stackframe from being de-allocated
     * when its referenced by a closure
     */
    int                     ref_count;
};


#define VM_SET_INTERRUPT(s)    *(s)->interrupt_flag_ptr=1;

struct vm {
    struct stackframe   *   top;
    bool                    unhandled_exception;
    int                     vm_errno;
};


extern struct pool ga_vm_stackframe_pool;

void                    vm_raise_exception(struct vm *, struct ga_obj *);
struct ga_obj       *   vm_eval_frame(struct vm *, struct stackframe *, int argc, struct ga_obj **);

__attribute__((always_inline))
static inline void
STACKFRAME_DESTROY(struct stackframe *frame)
{
    while (frame) {
        frame->ref_count--;

        if (frame->ref_count > 0) break;

        for (int i = frame->code->locals_start; frame->fast_cells[i]; i++) {
            GAOBJ_DEC_REF(frame->fast_cells[i]);
        }

        POOL_PUT(&ga_vm_stackframe_pool, frame);
        frame = frame->captive;
    }
}

__attribute__((always_inline))
static inline struct stackframe *
STACKFRAME_NEW(struct ga_obj *mod, struct ga_proc *code, struct stackframe *captive)
{
    struct stackframe *frame = POOL_GET(&ga_vm_stackframe_pool);
    frame->code = code;
    frame->mod = mod;
    //frame->parent = vm->top;
    frame->ref_count = 1;

    if (captive) {
        captive->ref_count++;
        frame->captive = captive;
    }

    return frame;
}

#endif
