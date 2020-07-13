#ifndef _RUNTIME_VM_H
#define _RUNTIME_VM_H

#include <stdbool.h>
#include <runtime/bytecode.h>
#include <gallium/dict.h>
#include <gallium/pool.h>

#define VM_UNHANDLED_EXCEPTION  1
#define VM_STACK_CORRUPTION     2
#define VM_STACK_OVERFLOW       3
#define VM_STACK_UNDERFLOW      4

#define VM_STACK_FASTCELL_MAX       128
#define VM_STACK_MAX                255
#define VM_EXCEPTION_HANDLER_MAX    32

struct vm;
struct dict;
struct ga_proc;
struct ga_ins;

struct stackframe {
    struct ga_obj       *   stack[VM_STACK_MAX];
    struct ga_obj       *   fast_cells[VM_STACK_FASTCELL_MAX];  /* local variables */
    struct ga_obj       *   mod; /* calling module */
    struct ga_proc      *   code;
    struct vm           *   vm;
    struct stackframe   *   parent;     /* the caller's stackframe */
    struct stackframe   *   captive;    /* stackframe we captured with a closure */
    
    /* stack of exception handlers */
    struct ga_ins       *   exception_stack[VM_EXCEPTION_HANDLER_MAX];
    int                     exception_stack_top;
    
    /*
     * pointers to local variables inside of vm_exec_code. A hack to be sure, 
     * but these should only be touched in circumstances where vm_exec_code()'s
     * stackframe still exists. The reason for this approach deals with hacks
     * made to optimize performance of vm_exec_code()'s dispatch and reliance
     * on local variables to minimize instructions
     */
    bool                *   sentinel_ptr;
    struct ga_ins       **  ins_ptr;
    struct ga_obj      ***  stack_ptr;
   
    /*
     * We use reference counting to prevent the stackframe from being de-allocated
     * when its referenced by a closure
     */
    int                     ref_count;
};

struct vm {
    struct stackframe   *   top;
    int                     vm_errno;
};


extern struct pool ga_vm_stackframe_pool;

struct ga_obj       *   vm_exec_code(struct vm *, struct ga_obj *mod, struct ga_proc *, struct stackframe *, int argc, struct ga_obj **);
void                    vm_raise_exception(struct vm *, struct ga_obj *);

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
STACKFRAME_NEW(struct vm *vm, struct ga_proc *code, struct stackframe *captive)
{
    struct stackframe *frame = POOL_GET(&ga_vm_stackframe_pool);
    frame->code = code;
    frame->vm = vm;
    frame->parent = vm->top;
    frame->ref_count = 1;

    if (captive) {
        captive->ref_count++;
        frame->captive = captive;
    }

    return frame;
}

#endif
