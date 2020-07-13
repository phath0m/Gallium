#ifndef _RUNTIME_BYTECODE_H
#define _RUNTIME_BYTECODE_H

#include <stdint.h>


/* opcodes */
#define NOOP            0x00
#define LOAD_CONST      0x01
#define LOAD_LOCAL      0x02
#define LOAD_GLOBAL     0x03
#define STORE_LOCAL     0x04
#define STORE_GLOBAL    0x05
#define POP             0x06
#define ADD             0x07
#define SUB             0x08
#define MUL             0x09
#define DIV             0x0A
#define RET             0x0B
#define LOAD_TRUE       0x0C
#define LOAD_FALSE      0x0D
#define JUMP_IF_TRUE    0x0E
#define JUMP_IF_FALSE   0x0F
#define INVOKE          0x10
#define DUP             0x11
#define BUILD_TUPLE     0x12
#define BUILD_LIST      0x13
#define BUILD_DICT      0x14
#define BUILD_FUNC      0x15
#define EQUALS          0x16
#define NOT_EQUALS      0x17
#define GREATER_THAN    0x18
#define LESS_THAN       0x19
#define GREATER_THAN_OR_EQU 0x1A
#define LESS_THAN_OR_EQU    0x1B
#define JUMP			0x1C
#define JUMP_DUP_IF_TRUE    0x1D
#define JUMP_DUP_IF_FALSE   0x1E
#define SET_ATTR            0x1F
#define GET_ATTR            0x20
#define PUSH_EXCEPTION_HANDLER  0x21
#define POP_EXCEPTION_HANDLER   0x22
#define LOAD_INDEX      0x23
#define STORE_INDEX     0x24
#define BUILD_CLASS     0x25
#define MOD             0x26
#define AND             0x27
#define OR              0x28
#define XOR             0x29
#define SHL             0x2A
#define SHR             0x2B
#define GET_ITER        0x2C
#define ITER_NEXT       0x2D
#define ITER_CUR        0x2E
#define STORE_FAST      0x2F
#define LOAD_FAST       0x30
#define BUILD_RANGE_CLOSED  0x31
#define BUILD_RANGE_HALF    0x32
#define BUILD_CLOSURE       0x33
#define NEGATE              0x34
#define NOT                 0x35
#define LOGICAL_NOT         0x36
#define COMPILE_MACRO       0x37
#define INLINE_INVOKE       0x38
#define JUMP_IF_COMPILED    0x39

/* gallium VM bytecode instruction */
struct ga_ins {
    uint8_t opcode;
    union {
        int32_t             imm_i32;
        int64_t             imm_i64;
        uint64_t            imm_u64;
        uint32_t            imm_u32;
        const char      *   imm_str;
        void            *   imm_ptr;
        struct ga_obj   *   imm_obj;
    } un;
};

/* bytecode "procdure" (series of bytecode instructions*/
struct ga_proc {
    struct ga_ins   *   bytecode;
    void            *   compiler_private;
    //struct ga_obj   *   mod;
    int                 locals_start;
    int                 size;
};

/* data associated with a module (Constants, procs, ect) */
struct ga_mod_data {
    struct list     *   constants;
    struct list     *   strings;
    struct list     *   procs;
};


void    ga_proc_destroy(struct ga_proc *);

#endif
