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
    .size = sizeof(struct stackframe)
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
vm_push_exception_handler(struct stackframe *frame, int ip)
{
    if (frame->exception_stack_top >= VM_EXCEPTION_HANDLER_MAX) {
        vm_panic(frame, VM_STACK_OVERFLOW, "exception stack overflow\n");
        return;
    }

    frame->exception_stack[frame->exception_stack_top++] = ip;
}

static int
vm_pop_exception_handler(struct stackframe *frame)
{
    return frame->exception_stack[--frame->exception_stack_top];
}

__attribute__((hot))
struct ga_obj *
vm_eval_frame(struct vm *vm, struct stackframe *frame, int argc, struct ga_obj **args)
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
        JUMP_LABEL(STORE_GLOBAL), JUMP_LABEL(POP), JUMP_LABEL(ADD), JUMP_LABEL(SUB), JUMP_LABEL(MUL),
        JUMP_LABEL(DIV), JUMP_LABEL(RET), JUMP_LABEL(LOAD_TRUE), JUMP_LABEL(LOAD_FALSE), JUMP_LABEL(JUMP_IF_TRUE),
        JUMP_LABEL(JUMP_IF_FALSE), JUMP_LABEL(INVOKE), JUMP_LABEL(DUP), JUMP_LABEL(BUILD_TUPLE),
        JUMP_LABEL(BUILD_LIST), JUMP_LABEL(BUILD_DICT), JUMP_LABEL(BUILD_FUNC), JUMP_LABEL(EQUALS),
        JUMP_LABEL(NOT_EQUALS), JUMP_LABEL(GREATER_THAN), JUMP_LABEL(LESS_THAN), JUMP_LABEL(GREATER_THAN_OR_EQU),
        JUMP_LABEL(LESS_THAN_OR_EQU), JUMP_LABEL(JUMP), JUMP_LABEL(JUMP_DUP_IF_TRUE), JUMP_LABEL(JUMP_DUP_IF_FALSE),
        JUMP_LABEL(SET_ATTR), JUMP_LABEL(GET_ATTR), JUMP_LABEL(PUSH_EXCEPTION_HANDLER), JUMP_LABEL(POP_EXCEPTION_HANDLER),
        JUMP_LABEL(LOAD_INDEX), JUMP_LABEL(STORE_INDEX), JUMP_LABEL(BUILD_CLASS), JUMP_LABEL(MOD), JUMP_LABEL(AND),
        JUMP_LABEL(OR), JUMP_LABEL(XOR), JUMP_LABEL(SHL), JUMP_LABEL(SHR), JUMP_LABEL(GET_ITER), JUMP_LABEL(ITER_NEXT),
        JUMP_LABEL(ITER_CUR), JUMP_LABEL(STORE_FAST), JUMP_LABEL(LOAD_FAST), JUMP_LABEL(BUILD_RANGE_CLOSED), 
        JUMP_LABEL(BUILD_RANGE_HALF), JUMP_LABEL(BUILD_CLOSURE), JUMP_LABEL(NEGATE), JUMP_LABEL(NOT),
        JUMP_LABEL(LOGICAL_NOT), JUMP_LABEL(COMPILE_MACRO), JUMP_LABEL(INLINE_INVOKE), JUMP_LABEL(JUMP_IF_COMPILED),
        JUMP_LABEL(LOAD_EXCEPTION), JUMP_LABEL(OPEN_MODULE), JUMP_LABEL(DUPX)
    };

    ga_ins_t *bytecode = frame->code->bytecode;

    struct ga_obj *return_val = NULL;
    struct ga_mod_data *data = frame->code->data;

    /* regularly accessed structures */
    struct vec *strings_vec = &data->string_pool;
    struct vec *objects_vec = &data->object_pool;

    struct ga_obj **locals = frame->fast_cells;

    struct ga_obj *mod = GAOBJ_INC_REF(frame->mod);

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

    frame->parent = vm->top;
    vm->top = frame;

    for (int i = 0; i < argc; i++) {
        locals[frame->code->locals_start + i] = GAOBJ_INC_REF(args[i]);
    }

    while (!interrupt_flag) {
        switch (GA_INS_OPCODE(*ins)) {
            case JUMP_TARGET(BUILD_CLASS): {
                const char *imm_str = (const char*)VEC_FAST_GET(strings_vec, GA_INS_IMMEDIATE(*ins));
                struct ga_obj *base = STACK_POP();
                struct ga_obj *dict = STACK_TOP();
                struct ga_obj *clazz = ga_class_new(imm_str, base, dict);

                STACK_SET_TOP(GAOBJ_INC_REF(clazz));

                GAOBJ_DEC_REF(dict);
                GAOBJ_DEC_REF(base);

                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(BUILD_DICT): {
                struct ga_obj *dict = ga_dict_new();
                for (int i = 0; i < GA_INS_IMMEDIATE(*ins); i++) {
                    struct ga_obj *key = STACK_POP();
                    struct ga_obj *val = STACK_POP();
                    GAOBJ_SETINDEX(dict, vm, key, val);
                    GAOBJ_DEC_REF(key);
                    GAOBJ_DEC_REF(val);
                }

                STACK_PUSH(GAOBJ_INC_REF(dict));

                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(BUILD_CLOSURE):
            case JUMP_TARGET(BUILD_FUNC): {
                struct ga_proc *func_code = VEC_FAST_GET(&data->proc_pool, GA_INS_IMMEDIATE(*ins));
                struct ga_obj *arglist = STACK_POP();
                struct ga_obj *func;

                if (GA_INS_OPCODE(*ins) == BUILD_FUNC)
                    func = ga_func_new(mod, func_code, frame->code);
                else
                    func = ga_closure_new(frame, mod, func_code, frame->code);

                for (int i = 0; i < ga_tuple_get_size(arglist); i++) {
                    struct ga_obj *param_name = ga_tuple_get_elem(arglist, i);
                    ga_func_add_param(func, ga_str_to_cstring(GAOBJ_STR(param_name, vm)), i); 
                }

                GAOBJ_DEC_REF(arglist);
                STACK_PUSH(GAOBJ_INC_REF(func));

                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(BUILD_LIST): {
                struct ga_obj *listp = ga_list_new(); 

                for (int i = GA_INS_IMMEDIATE(*ins) - 1; i >= 0; i--) {
                    struct ga_obj *elem = STACK_POP();
                    ga_list_append(listp, elem);
                    GAOBJ_DEC_REF(elem);
                }

                STACK_PUSH(GAOBJ_INC_REF(listp));

                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(BUILD_TUPLE): {
                struct ga_obj *tuple = ga_tuple_new(GA_INS_IMMEDIATE(*ins));

                for (int i = GA_INS_IMMEDIATE(*ins) - 1; i >= 0; i--) {
                    struct ga_obj *elem = STACK_POP();
                    ga_tuple_init_elem(tuple, i, elem); 
                    GAOBJ_DEC_REF(elem);
                }

                STACK_PUSH(GAOBJ_INC_REF(tuple));

                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(COMPILE_MACRO): {
                struct ga_obj *macro = STACK_POP();
                struct ga_obj *token_list = STACK_POP();
                struct ga_obj *expr_list = STACK_POP();
                
                struct ga_obj *macro_args[] = {
                    expr_list,
                    token_list
                };

                struct ga_obj *res = GAOBJ_XINC_REF(GAOBJ_INVOKE(macro, vm, 2, macro_args));

                if (res) {
                    struct ga_obj *inline_code = GAOBJ_INC_REF(ga_ast_node_compile_inline(res, frame->code));
                    struct ga_obj *ret = ga_code_invoke_inline(vm, inline_code, frame);

                    GAOBJ_DEC_REF(res);
                    
                    STACK_PUSH(GAOBJ_INC_REF(ret));

                    *ins = GA_INS_MAKE(INLINE_INVOKE, vec_add(objects_vec, inline_code));
                }

                GAOBJ_DEC_REF(macro);
                GAOBJ_DEC_REF(token_list);
                GAOBJ_DEC_REF(expr_list);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(DUP): {
                struct ga_obj *obj = STACK_TOP();

                STACK_PUSH(GAOBJ_INC_REF(obj));

                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(DUPX): {
                struct ga_obj *obj = STACK_TOP();

                for (int i = 0; i < GA_INS_IMMEDIATE(*ins); i++) {
                    STACK_PUSH(GAOBJ_INC_REF(obj));
                }

                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(INLINE_INVOKE): {
                struct ga_obj *ret = ga_code_invoke_inline(vm, VEC_FAST_GET(&data->proc_pool, GA_INS_IMMEDIATE(*ins)), frame);

                STACK_PUSH(GAOBJ_INC_REF(ret));

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(INVOKE): {
                struct ga_obj *obj = STACK_POP();
                struct ga_obj *args[128];

                for (int i = GA_INS_IMMEDIATE(*ins) - 1; i >= 0; i--) {
                    args[i] = STACK_POP();
                }

                struct ga_obj *res = GAOBJ_INVOKE(obj, vm, GA_INS_IMMEDIATE(*ins), args);
               
                if (res) {
                    STACK_PUSH(GAOBJ_INC_REF(res));
                }

                for (int i = 0; i < GA_INS_IMMEDIATE(*ins); i++) {
                    GAOBJ_DEC_REF(args[i]);
                }

                GAOBJ_DEC_REF(obj);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(GET_ATTR): {
                const char *imm_str = (const char*)VEC_FAST_GET(strings_vec, GA_INS_IMMEDIATE(*ins));
                struct ga_obj *obj = STACK_TOP();
                struct ga_obj *attr = GAOBJ_GETATTR(obj, vm, imm_str);
                
                if (attr) {
                    STACK_SET_TOP(GAOBJ_INC_REF(attr));
                } else {
                    STACK_SHRINK(1)
                    vm_raise_exception(vm, ga_attribute_error_new(imm_str));
                }
                GAOBJ_DEC_REF(obj);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(SET_ATTR): {
                const char *imm_str = (const char*)VEC_FAST_GET(strings_vec, GA_INS_IMMEDIATE(*ins));
                struct ga_obj *obj = STACK_TOP();
                struct ga_obj *val = STACK_SECOND();

                STACK_SHRINK(2);

                GAOBJ_SETATTR(obj, vm, imm_str, val);

                GAOBJ_DEC_REF(obj);
                GAOBJ_DEC_REF(val);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(LOAD_CONST): {
                STACK_PUSH(GAOBJ_INC_REF(VEC_FAST_GET(objects_vec, GA_INS_IMMEDIATE(*ins))));

                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(LOAD_EXCEPTION): {
                STACK_PUSH(GAOBJ_INC_REF(frame->exception_obj));

                GAOBJ_CLEAR_REF(&frame->exception_obj);

                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(LOAD_FAST): {
                struct stackframe *cur = frame;

                while (cur && !cur->fast_cells[GA_INS_IMMEDIATE(*ins)]) {
                    cur = cur->captive;
                }

                STACK_PUSH(GAOBJ_INC_REF(cur->fast_cells[GA_INS_IMMEDIATE(*ins)]));

                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(LOAD_GLOBAL): {
                const char *imm_str = (const char*)VEC_FAST_GET(strings_vec, GA_INS_IMMEDIATE(*ins));
                struct ga_obj *obj = GAOBJ_GETATTR(mod, vm, imm_str);

                if (!obj) {
                    vm_raise_exception(vm, ga_name_error_new(imm_str));
                } else {
                    STACK_PUSH(GAOBJ_INC_REF(obj));
                }

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(LOAD_INDEX): {
                struct ga_obj *obj = STACK_TOP();
                struct ga_obj *key = STACK_SECOND();
                struct ga_obj *val = GAOBJ_GETINDEX(obj, vm, key);


                if (val) {
                    STACK_SHRINK(1)
                    STACK_SET_TOP(GAOBJ_INC_REF(val));
                } else {
                    STACK_SHRINK(2);
                }

                GAOBJ_DEC_REF(obj);
                GAOBJ_DEC_REF(key);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(LOAD_TRUE): {
                STACK_PUSH(GAOBJ_INC_REF(&ga_bool_true_inst));
                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(LOAD_FALSE): {
                STACK_PUSH(GAOBJ_INC_REF(&ga_bool_false_inst));
                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(STORE_FAST): {
                struct ga_obj *obj = STACK_POP();

                if (locals[GA_INS_IMMEDIATE(*ins)]) {
                    GAOBJ_DEC_REF(locals[GA_INS_IMMEDIATE(*ins)]);
                }

                locals[GA_INS_IMMEDIATE(*ins)] = obj;

                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(STORE_GLOBAL): {
                const char *imm_str = (const char*)VEC_FAST_GET(strings_vec, GA_INS_IMMEDIATE(*ins));
                struct ga_obj *obj = STACK_POP();

                GAOBJ_SETATTR(mod, vm, imm_str, obj);
                GAOBJ_DEC_REF(obj);
                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(STORE_INDEX): {
                struct ga_obj *obj = STACK_TOP();
                struct ga_obj *key = STACK_SECOND();
                struct ga_obj *val = STACK_THIRD();

                STACK_SHRINK(3);
                GAOBJ_SETINDEX(obj, vm, key, val);
                GAOBJ_DEC_REF(obj);
                GAOBJ_DEC_REF(key);
                GAOBJ_DEC_REF(val);
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(OPEN_MODULE): {
                const char *imm_str = (const char*)VEC_FAST_GET(strings_vec, GA_INS_IMMEDIATE(*ins));

                struct ga_obj *imported_mod = ga_mod_open(mod, vm, imm_str);

                if (imported_mod) {
                    STACK_PUSH(GAOBJ_INC_REF(imported_mod));
                }
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(POP): {
                if (return_val) {
                    GAOBJ_DEC_REF(return_val);
                }
                return_val = STACK_POP();
                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(RET): {
                /* stack empty */
                if (stackpointer != frame->stack) {
                    if (return_val) GAOBJ_DEC_REF(return_val);
                    return_val = STACK_POP();
                } else {
                    if (!return_val) return_val = GAOBJ_INC_REF(&ga_null_inst);
                }
                interrupt_flag = true;
                break;
            }
            case JUMP_TARGET(LOGICAL_NOT): {
                struct ga_obj *top = STACK_TOP();
                STACK_SET_TOP(GAOBJ_INC_REF(ga_bool_from_bool(!GAOBJ_IS_TRUE(top, vm))));
                GAOBJ_DEC_REF(top);
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(NEGATE): {
                struct ga_obj *top = STACK_TOP();
                struct ga_obj *res = GAOBJ_NEGATE(top, vm);
                
                if (res) {
                    STACK_SET_TOP(GAOBJ_XINC_REF(res));
                }

                GAOBJ_DEC_REF(top);
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(NOT): {
                struct ga_obj *top = STACK_TOP();
                struct ga_obj *res = GAOBJ_NOT(top, vm);

                if (res) {
                    STACK_SET_TOP(GAOBJ_XINC_REF(res));
                }

                GAOBJ_DEC_REF(top);
                NEXT_INSTRUCTION();
            } 
            case JUMP_TARGET(GREATER_THAN): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = ga_bool_from_bool(GAOBJ_GT(left, vm, right));

                STACK_SET_TOP(GAOBJ_INC_REF(res));
                GAOBJ_DEC_REF(right);
                GAOBJ_DEC_REF(left);
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(GREATER_THAN_OR_EQU): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = ga_bool_from_bool(ga_obj_ge(left, vm, right));

                STACK_SET_TOP(GAOBJ_INC_REF(res));
                GAOBJ_DEC_REF(right);
                GAOBJ_DEC_REF(left);
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(LESS_THAN): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = ga_bool_from_bool(GAOBJ_LT(left, vm, right));

                STACK_SET_TOP(GAOBJ_INC_REF(res));
                GAOBJ_DEC_REF(right);
                GAOBJ_DEC_REF(left);
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(LESS_THAN_OR_EQU): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = ga_bool_from_bool(GAOBJ_LE(left, vm, right));

                STACK_SET_TOP(GAOBJ_INC_REF(res));
                GAOBJ_DEC_REF(right);
                GAOBJ_DEC_REF(left);
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(EQUALS): {
                struct ga_obj *right = STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = ga_bool_from_bool(GAOBJ_EQUALS(left, vm, right));

                STACK_SET_TOP(GAOBJ_INC_REF(res));
                GAOBJ_DEC_REF(right);
                GAOBJ_DEC_REF(left);
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(NOT_EQUALS): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = ga_bool_from_bool(!GAOBJ_EQUALS(left, vm, right));

                STACK_SET_TOP(GAOBJ_INC_REF(res));
                GAOBJ_DEC_REF(right);
                GAOBJ_DEC_REF(left);
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(ADD): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GAOBJ_ADD(left, vm, right);

                STACK_SET_TOP(GAOBJ_XINC_REF(res));
                GAOBJ_DEC_REF(right);
                GAOBJ_DEC_REF(left);
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(SUB): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GAOBJ_SUB(left, vm, right);

                STACK_SET_TOP(GAOBJ_XINC_REF(res));
                GAOBJ_DEC_REF(right);
                GAOBJ_DEC_REF(left);
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(MUL): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GAOBJ_MUL(left, vm, right);

                STACK_SET_TOP(GAOBJ_XINC_REF(res));
                GAOBJ_DEC_REF(right);
                GAOBJ_DEC_REF(left);
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(DIV): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GAOBJ_DIV(left, vm, right);

                STACK_SET_TOP(GAOBJ_XINC_REF(res));
                GAOBJ_DEC_REF(right);
                GAOBJ_DEC_REF(left);
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(MOD): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GAOBJ_MOD(left, vm, right);

                STACK_SET_TOP(GAOBJ_XINC_REF(res));
                GAOBJ_DEC_REF(right);
                GAOBJ_DEC_REF(left);
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(AND): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GAOBJ_AND(left, vm, right);

                STACK_SET_TOP(GAOBJ_XINC_REF(res));
                GAOBJ_DEC_REF(right);
                GAOBJ_DEC_REF(left);
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(OR): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GAOBJ_OR(left, vm, right);

                STACK_SET_TOP(GAOBJ_XINC_REF(res));
                GAOBJ_DEC_REF(right);
                GAOBJ_DEC_REF(left);
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(XOR): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GAOBJ_XOR(left, vm, right);

                STACK_SET_TOP(GAOBJ_XINC_REF(res));
                GAOBJ_DEC_REF(right);
                GAOBJ_DEC_REF(left);
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(SHL): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GAOBJ_SHL(left, vm, right);

                STACK_SET_TOP(GAOBJ_XINC_REF(res));
                GAOBJ_DEC_REF(right);
                GAOBJ_DEC_REF(left);
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(SHR): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GAOBJ_SHR(left, vm, right);

                STACK_SET_TOP(GAOBJ_XINC_REF(res));
                GAOBJ_DEC_REF(right);
                GAOBJ_DEC_REF(left);
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(BUILD_RANGE_CLOSED): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GAOBJ_CLOSED_RANGE(left, vm, right);

                STACK_SET_TOP(GAOBJ_XINC_REF(res));
                GAOBJ_DEC_REF(right);
                GAOBJ_DEC_REF(left);
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(BUILD_RANGE_HALF): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GAOBJ_HALF_RANGE(left, vm, right);

                STACK_SET_TOP(GAOBJ_XINC_REF(res));
                GAOBJ_DEC_REF(right);
                GAOBJ_DEC_REF(left);
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(GET_ITER): {
                struct ga_obj *obj = STACK_TOP();
                struct ga_obj *iter = ga_obj_iter(obj, vm);

                if (iter) {
                    STACK_SET_TOP(GAOBJ_INC_REF(iter));
                } else {
                    STACK_SHRINK(1);
                }

                GAOBJ_DEC_REF(obj);
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(ITER_NEXT): {
                struct ga_obj *obj = STACK_TOP();
                
                STACK_SET_TOP(GAOBJ_INC_REF(ga_bool_from_bool(GAOBJ_ITER_NEXT(obj, vm))));
                GAOBJ_DEC_REF(obj);
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(ITER_CUR): {
                struct ga_obj *obj = STACK_TOP();
                struct ga_obj *cur = GAOBJ_ITER_CUR(obj, vm);

                if (cur) {
                    STACK_SET_TOP(GAOBJ_INC_REF(cur));
                } else {
                    STACK_SHRINK(1);
                }

                GAOBJ_DEC_REF(obj);
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(JUMP): {
                JUMP_TO(GA_INS_IMMEDIATE(*ins));
            }
            case JUMP_TARGET(JUMP_DUP_IF_FALSE): {
                struct ga_obj *obj = STACK_POP();

                if (!GAOBJ_IS_TRUE(obj, vm)) {
                    STACK_PUSH(obj);
                    JUMP_TO(GA_INS_IMMEDIATE(*ins));
                }
                
                GAOBJ_DEC_REF(obj);
                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(JUMP_DUP_IF_TRUE): {
                struct ga_obj *obj = STACK_POP();

                if (GAOBJ_IS_TRUE(obj, vm)) {
                    STACK_PUSH( obj);
                    JUMP_TO(GA_INS_IMMEDIATE(*ins));
                }
                GAOBJ_DEC_REF(obj);
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

                if (!GAOBJ_IS_TRUE(obj, vm)) {
                    GAOBJ_DEC_REF(obj);
                    JUMP_TO(GA_INS_IMMEDIATE(*ins));
                }
                GAOBJ_DEC_REF(obj);
                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(JUMP_IF_TRUE): {
                struct ga_obj *obj = STACK_POP();

                if (GAOBJ_IS_TRUE(obj, vm)) {
                    GAOBJ_DEC_REF(obj);
                    JUMP_TO(GA_INS_IMMEDIATE(*ins));
                }
                GAOBJ_DEC_REF(obj);
                NEXT_INSTRUCTION_FAST();
            }
            case JUMP_TARGET(PUSH_EXCEPTION_HANDLER): {
                vm_push_exception_handler(frame, GA_INS_IMMEDIATE(*ins));
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(POP_EXCEPTION_HANDLER): { 
                vm_pop_exception_handler(frame);
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
                if (*ptr) GAOBJ_DEC_REF(*ptr);
                ptr++;
            }
        }
    }

    vm->top = frame->parent;

    STACKFRAME_DESTROY(frame);
    GAOBJ_DEC_REF(mod);

    return GAOBJ_XMOVE_REF(return_val);
}

void
vm_print_stack(struct vm *vm)
{
    struct stackframe *top = vm->top;

    while (top && top->code) {
        struct ga_proc *code = top->code;
        printf("    at \x1B[0;34m%s\x1B[0m()\n", code->name);
        top = top->parent;
    }
}

void
vm_raise_exception(struct vm *vm, struct ga_obj *exception)
{
    struct stackframe *top = vm->top;

    while (top && top->exception_stack_top == 0) {
        VM_SET_INTERRUPT(top);
        top = top->parent;
    }

    GAOBJ_INC_REF(exception);

    if (!top) {
        struct ga_obj *msg = GAOBJ_INC_REF(GAOBJ_STR(exception, vm));
        fprintf(stderr, "%s\n", ga_str_to_cstring(msg));
        GAOBJ_DEC_REF(msg);
        GAOBJ_DEC_REF(exception);
        vm->unhandled_exception = true;
        
        vm_print_stack(vm);

        return;
    }

    top->pending_exception_handler = vm_pop_exception_handler(top);
    top->exception_obj = exception;
    
    VM_SET_INTERRUPT(top);
}
