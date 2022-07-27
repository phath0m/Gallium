/*
 * vm.c - Gallium bytecode interpreter
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <gallium/builtins.h>
#include <gallium/dict.h>
#include <gallium/list.h>
#include <gallium/object.h>
#include <gallium/opcode.h>
#include <gallium/pool.h>
#include <gallium/vm.h>

struct pool ga_vm_stackframe_pool = {
    .size = sizeof(struct stackframe),
};

static void
vm_panic(struct stackframe *frame, int err, const char *fmt, ...)
{
    va_list vlist;
    struct vm *vm = frame->vm;
    *frame->interrupt_flag_ptr = true;
    vm->vm_errno = err;
    va_start(vlist, fmt);
    vfprintf(stderr, fmt, vlist);
    va_end(vlist);
}

static void
push_exception_handler(struct stackframe *frame, int ip)
{
    if (frame->exception_stack_top >= VM_EXCEPTION_HANDLER_MAX) {
        vm_panic(frame, VM_STACK_OVERFLOW, "exception stack overflow\n");
        return;
    }

    frame->exception_stack[frame->exception_stack_top++] = ip;
}

static int
pop_exception_handler(struct stackframe *frame)
{
    return frame->exception_stack[--frame->exception_stack_top];
}

__attribute__((hot))
struct ga_obj *
GaEval_ExecFrame(struct vm *vm, struct stackframe *frame, int argc, 
                 struct ga_obj **args)
{

#define JUMP_TARGET(o) o: _opcode##o
#define JUMP_LABEL(o) [o]=&&_opcode##o

    /*
     * pre-calculated jump table. This same technique is used by Python and can take advantage of 
     * CPU branch prediction and eliminate some instructions related to looping and dispatching 
     * of the switch statement. 
     */
    static void *jump_table[] = {
        JUMP_LABEL(LOAD_CONST), JUMP_LABEL(LOAD_GLOBAL),
        JUMP_LABEL(STORE_GLOBAL), JUMP_LABEL(POP), JUMP_LABEL(ADD),
        JUMP_LABEL(SUB), JUMP_LABEL(MUL), JUMP_LABEL(DIV), JUMP_LABEL(RET),
        JUMP_LABEL(LOAD_TRUE), JUMP_LABEL(LOAD_FALSE),
        JUMP_LABEL(JUMP_IF_TRUE), JUMP_LABEL(JUMP_IF_FALSE),
        JUMP_LABEL(INVOKE), JUMP_LABEL(DUP), JUMP_LABEL(BUILD_TUPLE),
        JUMP_LABEL(BUILD_LIST), JUMP_LABEL(BUILD_DICT), JUMP_LABEL(BUILD_FUNC),
        JUMP_LABEL(EQUALS), JUMP_LABEL(NOT_EQUALS), JUMP_LABEL(GREATER_THAN),
        JUMP_LABEL(LESS_THAN), JUMP_LABEL(GREATER_THAN_OR_EQU),
        JUMP_LABEL(LESS_THAN_OR_EQU), JUMP_LABEL(JUMP),
        JUMP_LABEL(JUMP_DUP_IF_TRUE), JUMP_LABEL(JUMP_DUP_IF_FALSE),
        JUMP_LABEL(SET_ATTR), JUMP_LABEL(GET_ATTR),
        JUMP_LABEL(PUSH_EXCEPTION_HANDLER), JUMP_LABEL(POP_EXCEPTION_HANDLER),
        JUMP_LABEL(LOAD_INDEX), JUMP_LABEL(STORE_INDEX),
        JUMP_LABEL(BUILD_CLASS), JUMP_LABEL(MOD), JUMP_LABEL(AND),
        JUMP_LABEL(OR), JUMP_LABEL(XOR), JUMP_LABEL(SHL), JUMP_LABEL(SHR),
        JUMP_LABEL(GET_ITER), JUMP_LABEL(ITER_NEXT), JUMP_LABEL(ITER_CUR),
        JUMP_LABEL(STORE_FAST), JUMP_LABEL(LOAD_FAST),
        JUMP_LABEL(BUILD_RANGE_CLOSED), JUMP_LABEL(BUILD_RANGE_HALF),
        JUMP_LABEL(BUILD_CLOSURE), JUMP_LABEL(NEGATE), JUMP_LABEL(NOT),
        JUMP_LABEL(LOGICAL_NOT), JUMP_LABEL(COMPILE_MACRO),
        JUMP_LABEL(INLINE_INVOKE), JUMP_LABEL(JUMP_IF_COMPILED),
        JUMP_LABEL(LOAD_EXCEPTION), JUMP_LABEL(OPEN_MODULE), JUMP_LABEL(DUPX),
        JUMP_LABEL(MATCH), JUMP_LABEL(BUILD_ENUM), JUMP_LABEL(BUILD_MIXIN)
    };

    /* Store references here for faster access */
    ga_ins_t *bytecode = frame->code->bytecode;

    struct ga_obj *return_val = NULL;
    struct ga_mod_data *data = frame->code->data;

    /* regularly accessed structures */
    struct vec *strings_vec = &data->string_pool;
    struct vec *objects_vec = &data->object_pool;

    struct ga_obj **locals = frame->fast_cells;

    struct ga_obj *mod = GaObj_INC_REF(frame->mod);

    /* interrupt_flag; if set, can force break from loop*/ 
    volatile bool interrupt_flag = false;

    /*
     * allows external functions to set the interrupt_flag. This is used for exception
     * handling
     */
    frame->interrupt_flag_ptr = &interrupt_flag;

    /*
     * instruction and stack pointer.
     *
     * Note: Use of register keyword has yielded optimal 
     * performance gains
     *
     */
    register ga_ins_t *ins = &bytecode[0];
    register struct ga_obj **stackpointer = frame->stack;

/* instruction pointer helper macros */
#define JUMP_TO(target) \
    ins = &bytecode[target]; \
    if (!interrupt_flag) { \
        goto *jump_table[GA_INS_OPCODE(*ins)] ; \
    } else \
        break;

#define NEXT_INSTRUCTION() if (!interrupt_flag) { \
        goto *jump_table[GA_INS_OPCODE(*(++ins))]; \
    } else { \
        break; \
    }

/* 
 * This will not check to see if the interrupt_flag was sent. Should only be used by
 * instructions that will not raise an exception
 */
#define NEXT_INSTRUCTION_FAST() goto *jump_table[GA_INS_OPCODE(*(++ins))];

/* Stack helper macros */
#define STACK_PUSH(o) *(stackpointer++) = (o)
#define STACK_POP() *(--stackpointer)
#define STACK_TOP() (stackpointer[-1])
#define STACK_SECOND() (stackpointer[-2])
#define STACK_THIRD() (stackpointer[-3])
#define STACK_SET_TOP(o) stackpointer[-1] = (o)
#define STACK_SHRINK(n) stackpointer -= n;

/* Retrieve immediate string value */
#define IMMEDIATE_STRING() (struct ga_string_pool_entry*)VEC_FAST_GET(strings_vec, GA_INS_IMMEDIATE(*ins));

/* Integer optimization */
#define IS_INTEGER(o) (((o)->type) == (GA_INT_TYPE))
#define AS_INTEGER(o) ((o)->un.state_i64)

    frame->parent = vm->top;
    vm->top = frame;

    for (int i = 0; i < argc; i++) {
        locals[frame->code->locals_start + i] = GaObj_INC_REF(args[i]);
    }

    while (!interrupt_flag) {
        switch (GA_INS_OPCODE(*ins)) {
            case JUMP_TARGET(BUILD_CLASS): {
                struct ga_string_pool_entry *imm_str = IMMEDIATE_STRING();
                struct ga_obj *base = STACK_POP();
                struct ga_obj *mixins = STACK_POP();
                struct ga_obj *dict = STACK_TOP();
                struct ga_obj *clazz = GaClass_New(imm_str->value, base, mixins, dict);

                STACK_SET_TOP(GaObj_INC_REF(clazz));

                GaObj_DEC_REF(dict);
                GaObj_DEC_REF(base);
                GaObj_DEC_REF(mixins);

                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(BUILD_ENUM): {
                struct ga_string_pool_entry *imm_str = IMMEDIATE_STRING();
                struct ga_obj *values = STACK_TOP();
                struct ga_obj *enum_type = GaEnum_New(imm_str->value, values);

                STACK_SET_TOP(GaObj_INC_REF(enum_type));

                GaObj_DEC_REF(values);

                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(BUILD_MIXIN): {
                struct ga_obj *dict = STACK_TOP();

                struct ga_obj *mixin = GaMixin_New(dict);

                STACK_SET_TOP(GaObj_INC_REF(mixin));

                GaObj_DEC_REF(dict);

                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(BUILD_DICT): {
                struct ga_obj *dict = GaDict_New();
                for (int i = 0; i < GA_INS_IMMEDIATE(*ins); i++) {
                    struct ga_obj *key = STACK_POP();
                    struct ga_obj *val = STACK_POP();
                    GaObj_SETINDEX(dict, vm, key, val);
                    GaObj_DEC_REF(key);
                    GaObj_DEC_REF(val);
                }

                STACK_PUSH(GaObj_INC_REF(dict));

                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(BUILD_CLOSURE):
            case JUMP_TARGET(BUILD_FUNC): {
                struct ga_proc *func_code = VEC_FAST_GET(&data->proc_pool, GA_INS_IMMEDIATE(*ins));
                struct ga_obj *arglist = STACK_POP();
                struct ga_obj *func;

                if (GA_INS_OPCODE(*ins) == BUILD_FUNC)
                    func = GaFunc_New(mod, func_code, frame->code);
                else
                    func = GaClosure_New(frame, mod, func_code, frame->code);

                for (int i = 0; i < GaTuple_GetSize(arglist); i++) {
                    struct ga_obj *param_name = GaTuple_GetElem(arglist, i);
                    GaFunc_AddParam(func, GaStr_ToCString(GaObj_STR(param_name, vm)), i); 
                }

                GaObj_DEC_REF(arglist);
                STACK_PUSH(GaObj_INC_REF(func));

                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(BUILD_LIST): {
                struct ga_obj *listp = GaList_New(); 

                for (int i = GA_INS_IMMEDIATE(*ins) - 1; i >= 0; i--) {
                    struct ga_obj *elem = STACK_POP();
                    GaList_Append(listp, elem);
                    GaObj_DEC_REF(elem);
                }

                STACK_PUSH(GaObj_INC_REF(listp));

                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(BUILD_TUPLE): {
                struct ga_obj *tuple = GaTuple_New(GA_INS_IMMEDIATE(*ins));

                for (int i = GA_INS_IMMEDIATE(*ins) - 1; i >= 0; i--) {
                    struct ga_obj *elem = STACK_POP();
                    GaTuple_InitElem(tuple, i, elem); 
                    GaObj_DEC_REF(elem);
                }

                STACK_PUSH(GaObj_INC_REF(tuple));

                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(COMPILE_MACRO): {
                struct ga_obj *macro = STACK_POP();
                struct ga_obj *token_list = STACK_POP();
                
                struct ga_obj *macro_args[] = {
                    token_list
                };

                struct ga_obj *res = GaObj_XINC_REF(GaObj_INVOKE(macro, vm, 1, macro_args));

                if (res && !interrupt_flag) {
                    struct ga_obj *inline_code = GaObj_INC_REF(ga_ast_node_compile_inline(res, frame->code));
                    struct ga_obj *ret = GaCode_InvokeInline(vm, inline_code, frame);
                    GaObj_DEC_REF(res);
                    STACK_PUSH(GaObj_XINC_REF(ret));
                    *ins = GA_INS_MAKE(INLINE_INVOKE, GaVec_Append(objects_vec, inline_code));
                }
                GaObj_DEC_REF(macro);
                GaObj_DEC_REF(token_list);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(DUP): {
                struct ga_obj *obj = STACK_TOP();
                STACK_PUSH(GaObj_INC_REF(obj));
                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(DUPX): {
                struct ga_obj *obj = STACK_TOP();
                for (int i = 0; i < GA_INS_IMMEDIATE(*ins); i++) {
                    STACK_PUSH(GaObj_INC_REF(obj));
                }
                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(INLINE_INVOKE): {
                struct ga_obj *ret = GaCode_InvokeInline(vm, VEC_FAST_GET(&data->proc_pool, GA_INS_IMMEDIATE(*ins)), frame);
                STACK_PUSH(GaObj_INC_REF(ret));
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(INVOKE): {
                struct ga_obj *obj = STACK_POP();
                struct ga_obj *args[128];

                for (int i = GA_INS_IMMEDIATE(*ins) - 1; i >= 0; i--) {
                    args[i] = STACK_POP();
                }

                struct ga_obj *res = GaObj_INVOKE(obj, vm, GA_INS_IMMEDIATE(*ins), args);
               
                if (res) {
                    STACK_PUSH(GaObj_INC_REF(res));
                }

                for (int i = 0; i < GA_INS_IMMEDIATE(*ins); i++) {
                    GaObj_DEC_REF(args[i]);
                }

                GaObj_DEC_REF(obj);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(GET_ATTR): {
                struct ga_string_pool_entry *imm_str = IMMEDIATE_STRING();
                struct ga_obj *obj = STACK_TOP();
                struct ga_obj *attr = _GaObj_GETATTR_FAST(obj, vm, imm_str->hash, imm_str->value);
                
                if (attr) {
                    STACK_SET_TOP(GaObj_INC_REF(attr));
                } else {
                    STACK_SHRINK(1)
                    GaEval_RaiseException(vm, GaErr_NewAttributeError(imm_str->value));
                }
                GaObj_DEC_REF(obj);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(SET_ATTR): {
                struct ga_string_pool_entry *imm_str = IMMEDIATE_STRING();
                struct ga_obj *obj = STACK_TOP();
                struct ga_obj *val = STACK_SECOND();

                STACK_SHRINK(2);

                GaObj_SETATTR(obj, vm, imm_str->value, val);

                GaObj_DEC_REF(obj);
                GaObj_DEC_REF(val);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(LOAD_CONST): {
                STACK_PUSH(GaObj_INC_REF(VEC_FAST_GET(objects_vec, GA_INS_IMMEDIATE(*ins))));

                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(LOAD_EXCEPTION): {
                STACK_PUSH(GaObj_INC_REF(frame->exception_obj));

                GaObj_CLEAR_REF(&frame->exception_obj);

                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(LOAD_FAST): {
                struct stackframe *cur = frame;

                while (cur && !cur->fast_cells[GA_INS_IMMEDIATE(*ins)]) {
                    cur = cur->captive;
                }

                STACK_PUSH(GaObj_INC_REF(cur->fast_cells[GA_INS_IMMEDIATE(*ins)]));

                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(LOAD_GLOBAL): {
                struct ga_string_pool_entry *imm_str = IMMEDIATE_STRING();
                struct ga_obj *obj = _GaObj_GETATTR_FAST(mod, vm, imm_str->hash, imm_str->value);

                if (!obj) {
                    GaEval_RaiseException(vm, GaErr_NewNameError(imm_str->value));
                } else {
                    STACK_PUSH(GaObj_INC_REF(obj));
                }

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(LOAD_INDEX): {
                struct ga_obj *obj = STACK_TOP();
                struct ga_obj *key = STACK_SECOND();
                struct ga_obj *val = GaObj_GETINDEX(obj, vm, key);

                if (val) {
                    STACK_SHRINK(1)
                    STACK_SET_TOP(GaObj_INC_REF(val));
                } else {
                    STACK_SHRINK(2);
                }

                GaObj_DEC_REF(obj);
                GaObj_DEC_REF(key);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(LOAD_TRUE): {
                STACK_PUSH(GaObj_INC_REF(&ga_bool_true_inst));
                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(LOAD_FALSE): {
                STACK_PUSH(GaObj_INC_REF(&ga_bool_false_inst));
                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(STORE_FAST): {
                struct ga_obj *obj = STACK_POP();

                if (locals[GA_INS_IMMEDIATE(*ins)]) {
                    GaObj_DEC_REF(locals[GA_INS_IMMEDIATE(*ins)]);
                }

                locals[GA_INS_IMMEDIATE(*ins)] = obj;

                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(STORE_GLOBAL): {
                struct ga_string_pool_entry *imm_str = IMMEDIATE_STRING();
                struct ga_obj *obj = STACK_POP();

                GaObj_SETATTR(mod, vm, imm_str->value, obj);
                GaObj_DEC_REF(obj);
                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(STORE_INDEX): {
                struct ga_obj *obj = STACK_TOP();
                struct ga_obj *key = STACK_SECOND();
                struct ga_obj *val = STACK_THIRD();

                STACK_SHRINK(3);
                GaObj_SETINDEX(obj, vm, key, val);

                GaObj_DEC_REF(obj);
                GaObj_DEC_REF(key);
                GaObj_DEC_REF(val);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(OPEN_MODULE): {
                struct ga_string_pool_entry *imm_str = IMMEDIATE_STRING();
                struct ga_obj *imported_mod = GaModule_Open(mod, vm, imm_str->value);

                if (imported_mod) {
                    STACK_PUSH(GaObj_INC_REF(imported_mod));
                }
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(POP): {
                if (return_val) {
                    GaObj_DEC_REF(return_val);
                }
                return_val = STACK_POP();

                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(RET): {
                /* stack empty */
                if (stackpointer != frame->stack) {
                    if (return_val) GaObj_DEC_REF(return_val);
                    return_val = STACK_POP();
                } else {
                    if (!return_val) return_val = GaObj_INC_REF(&ga_null_inst);
                }
                interrupt_flag = true;
                break;
            }
            case JUMP_TARGET(LOGICAL_NOT): {
                struct ga_obj *top = STACK_TOP();
                STACK_SET_TOP(GaObj_INC_REF(GaBool_FROM_BOOL(!GaObj_IS_TRUE(top, vm))));
                GaObj_DEC_REF(top);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(NEGATE): {
                struct ga_obj *top = STACK_TOP();
                struct ga_obj *res = GaObj_NEGATE(top, vm);
                
                if (res) {
                    STACK_SET_TOP(GaObj_XINC_REF(res));
                }

                GaObj_DEC_REF(top);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(NOT): {
                struct ga_obj *top = STACK_TOP();
                struct ga_obj *res = GaObj_NOT(top, vm);

                if (res) {
                    STACK_SET_TOP(GaObj_XINC_REF(res));
                }

                GaObj_DEC_REF(top);
                NEXT_INSTRUCTION();
            } 
            case JUMP_TARGET(GREATER_THAN): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GaBool_FROM_BOOL(GaObj_GT(left, vm, right));

                STACK_SET_TOP(GaObj_INC_REF(res));

                GaObj_DEC_REF(right);
                GaObj_DEC_REF(left);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(GREATER_THAN_OR_EQU): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GaBool_FROM_BOOL(GaObj_GE(left, vm, right));

                STACK_SET_TOP(GaObj_INC_REF(res));

                GaObj_DEC_REF(right);
                GaObj_DEC_REF(left);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(LESS_THAN): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res;

                if (IS_INTEGER(right) && IS_INTEGER(left)) res = GaBool_FROM_BOOL(AS_INTEGER(left) < AS_INTEGER(right));
                else res = GaBool_FROM_BOOL(GaObj_LT(left, vm, right));

                STACK_SET_TOP(GaObj_INC_REF(res));

                GaObj_DEC_REF(right);
                GaObj_DEC_REF(left);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(LESS_THAN_OR_EQU): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GaBool_FROM_BOOL(GaObj_LE(left, vm, right));

                STACK_SET_TOP(GaObj_INC_REF(res));

                GaObj_DEC_REF(right);
                GaObj_DEC_REF(left);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(EQUALS): {
                struct ga_obj *right = STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GaBool_FROM_BOOL(GaObj_EQUALS(left, vm, right));

                STACK_SET_TOP(GaObj_INC_REF(res));

                GaObj_DEC_REF(right);
                GaObj_DEC_REF(left);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(NOT_EQUALS): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GaBool_FROM_BOOL(!GaObj_EQUALS(left, vm, right));

                STACK_SET_TOP(GaObj_INC_REF(res));

                GaObj_DEC_REF(right);
                GaObj_DEC_REF(left);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(MATCH): {
                struct ga_obj *right = STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GaBool_FROM_BOOL(GaObj_MATCH(left, vm, right));

                STACK_SET_TOP(GaObj_INC_REF(res));

                GaObj_DEC_REF(right);
                GaObj_DEC_REF(left);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(ADD): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();

                if (IS_INTEGER(right) && IS_INTEGER(left))
                    STACK_SET_TOP(GaObj_INC_REF(GaInt_FROM_I64(AS_INTEGER(left) + AS_INTEGER(right))));
                else
                    STACK_SET_TOP(GaObj_XINC_REF(GaObj_ADD(left, vm, right)));

                GaObj_DEC_REF(right);
                GaObj_DEC_REF(left);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(SUB): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();

                if (IS_INTEGER(right) && IS_INTEGER(left))
                    STACK_SET_TOP(GaObj_INC_REF(GaInt_FROM_I64(AS_INTEGER(left) - AS_INTEGER(right))));
                else
                    STACK_SET_TOP(GaObj_XINC_REF(GaObj_SUB(left, vm, right)));

                GaObj_DEC_REF(right);
                GaObj_DEC_REF(left);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(MUL): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GaObj_MUL(left, vm, right);

                STACK_SET_TOP(GaObj_XINC_REF(res));

                GaObj_DEC_REF(right);
                GaObj_DEC_REF(left);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(DIV): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GaObj_DIV(left, vm, right);

                STACK_SET_TOP(GaObj_XINC_REF(res));

                GaObj_DEC_REF(right);
                GaObj_DEC_REF(left);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(MOD): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GaObj_MOD(left, vm, right);

                STACK_SET_TOP(GaObj_XINC_REF(res));

                GaObj_DEC_REF(right);
                GaObj_DEC_REF(left);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(AND): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GaObj_AND(left, vm, right);

                STACK_SET_TOP(GaObj_XINC_REF(res));

                GaObj_DEC_REF(right);
                GaObj_DEC_REF(left);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(OR): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GaObj_OR(left, vm, right);

                STACK_SET_TOP(GaObj_XINC_REF(res));

                GaObj_DEC_REF(right);
                GaObj_DEC_REF(left);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(XOR): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GaObj_XOR(left, vm, right);

                STACK_SET_TOP(GaObj_XINC_REF(res));

                GaObj_DEC_REF(right);
                GaObj_DEC_REF(left);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(SHL): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GaObj_SHL(left, vm, right);

                STACK_SET_TOP(GaObj_XINC_REF(res));

                GaObj_DEC_REF(right);
                GaObj_DEC_REF(left);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(SHR): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GaObj_SHR(left, vm, right);

                STACK_SET_TOP(GaObj_XINC_REF(res));

                GaObj_DEC_REF(right);
                GaObj_DEC_REF(left);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(BUILD_RANGE_CLOSED): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GaObj_CLOSED_RANGE(left, vm, right);

                STACK_SET_TOP(GaObj_XINC_REF(res));

                GaObj_DEC_REF(right);
                GaObj_DEC_REF(left);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(BUILD_RANGE_HALF): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GaObj_HALF_RANGE(left, vm, right);

                STACK_SET_TOP(GaObj_XINC_REF(res));

                GaObj_DEC_REF(right);
                GaObj_DEC_REF(left);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(GET_ITER): {
                struct ga_obj *obj = STACK_TOP();
                struct ga_obj *iter = GaObj_ITER(obj, vm);

                if (iter) {
                    STACK_SET_TOP(GaObj_INC_REF(iter));
                } else {
                    STACK_SHRINK(1);
                }

                GaObj_DEC_REF(obj);
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(ITER_NEXT): {
                struct ga_obj *obj = STACK_TOP();
                
                STACK_SET_TOP(GaObj_INC_REF(GaBool_FROM_BOOL(GaObj_ITER_NEXT(obj, vm))));
                GaObj_DEC_REF(obj);
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(ITER_CUR): {
                struct ga_obj *obj = STACK_TOP();
                struct ga_obj *cur = GaObj_ITER_CUR(obj, vm);

                if (cur) {
                    STACK_SET_TOP(GaObj_INC_REF(cur));
                } else {
                    STACK_SHRINK(1);
                }
                GaObj_DEC_REF(obj);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(JUMP): {
                JUMP_TO(GA_INS_IMMEDIATE(*ins));
            }
            case JUMP_TARGET(JUMP_DUP_IF_FALSE): {
                struct ga_obj *obj = STACK_POP();

                if (!GaObj_IS_TRUE(obj, vm)) {
                    STACK_PUSH(obj);
                    JUMP_TO(GA_INS_IMMEDIATE(*ins));
                }
                GaObj_DEC_REF(obj);

                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(JUMP_DUP_IF_TRUE): {
                struct ga_obj *obj = STACK_POP();

                if (GaObj_IS_TRUE(obj, vm)) {
                    STACK_PUSH( obj);
                    JUMP_TO(GA_INS_IMMEDIATE(*ins));
                }
                GaObj_DEC_REF(obj);

                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(JUMP_IF_COMPILED): {
                if (GA_INS_OPCODE(bytecode[GA_INS_IMMEDIATE(*ins)]) == INLINE_INVOKE) {
                    JUMP_TO(GA_INS_IMMEDIATE(*ins));
                }
                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(JUMP_IF_FALSE): {
                struct ga_obj *obj = STACK_POP();

                if (!GaObj_IS_TRUE(obj, vm)) {
                    GaObj_DEC_REF(obj);
                    JUMP_TO(GA_INS_IMMEDIATE(*ins));
                }
                GaObj_DEC_REF(obj);

                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(JUMP_IF_TRUE): {
                struct ga_obj *obj = STACK_POP();

                if (GaObj_IS_TRUE(obj, vm)) {
                    GaObj_DEC_REF(obj);
                    JUMP_TO(GA_INS_IMMEDIATE(*ins));
                }
                GaObj_DEC_REF(obj);

                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(PUSH_EXCEPTION_HANDLER): {
                push_exception_handler(frame, GA_INS_IMMEDIATE(*ins));

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(POP_EXCEPTION_HANDLER): { 
                pop_exception_handler(frame);

                NEXT_INSTRUCTION();
            }
            default:
                break;
        }

        if (interrupt_flag) {
            if (frame->pending_exception_handler) {
                interrupt_flag = false;
                int exception_handler = frame->pending_exception_handler;
                frame->pending_exception_handler = 0;
                JUMP_TO(exception_handler);
            }
            struct ga_obj **ptr = frame->stack;
            while (ptr != stackpointer) {
                if (*ptr) GaObj_DEC_REF(*ptr);
                ptr++;
            }
        }
    }

    vm->top = frame->parent;

    GaFrame_DESTROY(frame);
    GaObj_DEC_REF(mod);

    return GaObj_XMOVE_REF(return_val);
}

void
GaEval_PrintStack(struct vm *vm)
{
    struct stackframe *top = vm->top;

    while (top && top->code) {
        struct ga_proc *code = top->code;
        printf("    at \x1B[0;34m%s\x1B[0m()\n", code->name);
        top = top->parent;
    }
}

void
GaEval_RaiseException(struct vm *vm, struct ga_obj *exception)
{
    struct stackframe *top = vm->top;

    while (top && top->exception_stack_top == 0) {
        VM_SET_INTERRUPT(top);
        top = top->parent;
    }

    GaObj_INC_REF(exception);

    if (!top) {
        struct ga_obj *msg = GaObj_INC_REF(GaObj_STR(exception, vm));
        fprintf(stderr, "%s\n", GaStr_ToCString(msg));
        GaObj_DEC_REF(msg);
        GaObj_DEC_REF(exception);
        vm->unhandled_exception = true;
        GaEval_PrintStack(vm);
        return;
    }

    top->pending_exception_handler = pop_exception_handler(top);
    top->exception_obj = exception;
    
    VM_SET_INTERRUPT(top);
}
