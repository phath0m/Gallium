#ifndef _GALLIUM_CODE_H
#define _GALLIUM_CODE_H

#include <stdbool.h>
#include <gallium/dict.h>
#include <gallium/pool.h>
#include <gallium/object.h>
#include <gallium/vec.h>

typedef uint64_t ga_ins_t;

#define GA_INS_MAKE(opcode, imm)    ((opcode) | ((imm) << 8))
#define GA_INS_OPCODE(ins)          ((ins) & 0x7F)
#define GA_INS_OPARG(ins)           (((ins) & 0x80) >> 7)
#define GA_INS_IMMEDIATE(ins)       ((ins) >> 8)

typedef struct _GaCodeObject {
    GaObject                _header;
    ga_ins_t            *   bytecode;
    int                     bytecode_len;
    void                *   compiler_private;
    struct vec              object_pool;
    struct vec              string_pool;
    int                     locals_start;
    int                     locals_end;
    int                     size;
    char                    name[];
} GaCodeObject;

/* string pool entry */
struct GaCodeStringEntry {
    uint32_t    hash;
    char        value[];
};

#endif