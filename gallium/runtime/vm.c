#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <gallium/builtins.h>
#include <gallium/dict.h>
#include <gallium/list.h>
#include <gallium/object.h>
#include <gallium/pool.h>
#include <gallium/vm.h>
#include <runtime/bytecode.h>

struct pool ga_vm_stackframe_pool = {
    .size = sizeof(struct stackframe)
};

static void
vm_panic(struct stackframe *frame, int err, const char *fmt, ...)
{
    va_list vlist;
    struct vm *vm = frame->vm;
    *frame->sentinel_ptr = true;
    vm->vm_errno = err;
    
    va_start(vlist, fmt);
    vfprintf(stderr, fmt, vlist);
    va_end(vlist);
}

__attribute__((always_inline))
static inline void
vm_push(struct stackframe *frame, struct ga_obj *obj)
{
    (*(*frame->stack_ptr)++) = obj;
}

__attribute__((always_inline))
static inline struct ga_obj *
vm_pop(struct stackframe *frame)
{
    return *(--(*frame->stack_ptr));
}

static void
vm_push_exception_handler(struct stackframe *frame, struct ga_ins *ins)
{
    if (frame->exception_stack_top >= VM_EXCEPTION_HANDLER_MAX) {
        vm_panic(frame, VM_STACK_OVERFLOW, "exception stack overflow\n");
        return;
    }

    frame->exception_stack[frame->exception_stack_top++] = ins;
}

static struct ga_ins *
vm_pop_exception_handler(struct stackframe *frame)
{
    return frame->exception_stack[--frame->exception_stack_top];
}

struct ga_obj *
vm_exec_code(struct vm *vm, struct ga_code *code, struct stackframe *frame, int argc, struct ga_obj **args)
{

#define JUMP_TARGET(o) o: _opcode##o
#define JUMP_LABEL(o) [o]=&&_opcode##o

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
        JUMP_LABEL(LOGICAL_NOT)
    };

    bool sentinel = false;
    struct ga_obj *return_val = &ga_null_inst;
    struct ga_ins *bytecode = code->bytecode;
    struct ga_ins *ins = bytecode;

#define JUMP_TO(target) ins = &bytecode[target]; if (!sentinel) goto *jump_table[ins->opcode] ; else break;
#define NEXT_INSTRUCTION() if (!sentinel) goto *jump_table[(++ins)->opcode] ; else break;

    if (!frame) {
        frame = STACKFRAME_NEW(vm, code, NULL);
    }

    struct ga_obj **locals = frame->fast_cells;
    struct ga_obj **stackpointer = frame->stack;

    frame->sentinel_ptr = &sentinel;
    frame->ins_ptr = &ins;
    frame->stack_ptr = &stackpointer; 

#define STACK_PUSH(o) *(stackpointer++) = (o)
#define STACK_POP() *(--stackpointer)
#define STACK_TOP() (stackpointer[-1])
#define STACK_SECOND() (stackpointer[-2])
#define STACK_THIRD() (stackpointer[-3])
#define STACK_SET_TOP(o) stackpointer[-1] = (o)
#define STACK_SHRINK(n) stackpointer -= n;

    vm->top = frame;

    for (int i = 0; i < argc; i++) {
        locals[code->locals_start + i] = GAOBJ_INC_REF(args[i]);
    }

    while (!sentinel) {
        switch (ins->opcode) {
            case JUMP_TARGET(BUILD_CLASS): {
                struct ga_obj *base = STACK_POP();
                struct ga_obj *dict = STACK_TOP();
                struct ga_obj *clazz = ga_class_new(ins->un.imm_str, base, dict);

                STACK_SET_TOP(GAOBJ_INC_REF(clazz));

                GAOBJ_DEC_REF(dict);
                GAOBJ_DEC_REF(base);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(BUILD_DICT): {
                struct ga_obj *dict = ga_dict_new();
                for (int i = 0; i < ins->un.imm_i32; i++) {
                    struct ga_obj *key = STACK_POP();
                    struct ga_obj *val = STACK_POP();
                    GAOBJ_SETINDEX(dict, vm, key, val);
                    GAOBJ_DEC_REF(key);
                    GAOBJ_DEC_REF(val);
                }

                STACK_PUSH(GAOBJ_INC_REF(dict));

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(BUILD_CLOSURE):
            case JUMP_TARGET(BUILD_FUNC): {
                struct ga_code *func_code = ins->un.imm_ptr;
                struct ga_obj *arglist = STACK_POP();
                struct ga_obj *func;

                if (ins->opcode == BUILD_FUNC)
                    func = ga_func_new(func_code);
                else
                    func = ga_closure_new(frame, func_code);

                for (int i = 0; i < ga_tuple_get_size(arglist); i++) {
                    struct ga_obj *param_name = ga_tuple_get_elem(arglist, i);
                    ga_func_add_param(func, ga_str_to_cstring(GAOBJ_STR(param_name, vm)), i); 
                }

                func_code->mod = frame->code->mod;

                GAOBJ_DEC_REF(arglist);
                STACK_PUSH(GAOBJ_INC_REF(func));

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(BUILD_LIST): {
                struct ga_obj *listp = ga_list_new(); 

                for (int i = ins->un.imm_i32 - 1; i >= 0; i--) {
                    struct ga_obj *elem = STACK_POP();
                    ga_list_append(listp, elem);
                    GAOBJ_DEC_REF(elem);
                }

                STACK_PUSH( GAOBJ_INC_REF(listp));
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(BUILD_TUPLE): {
                struct ga_obj *tuple = ga_tuple_new(ins->un.imm_i32);

                for (int i = ins->un.imm_i32 - 1; i >= 0; i--) {
                    struct ga_obj *elem = STACK_POP();
                    ga_tuple_init_elem(tuple, i, elem); 
                    GAOBJ_DEC_REF(elem);
                }

                STACK_PUSH(GAOBJ_INC_REF(tuple));

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(DUP): {
                struct ga_obj *obj = STACK_POP();

                STACK_PUSH(GAOBJ_INC_REF(obj));
                STACK_PUSH(GAOBJ_INC_REF(obj));

                GAOBJ_DEC_REF(obj);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(INVOKE): {
                struct ga_obj *obj = STACK_POP();
                struct ga_obj *args[128];

                for (int i = ins->un.imm_i32 - 1; i >= 0; i--) {
                    args[i] = STACK_POP();
                }

                struct ga_obj *res = GAOBJ_INVOKE(obj, vm, ins->un.imm_i32, args);
               
                if (res) {
                    STACK_PUSH(GAOBJ_INC_REF(res));
                }

                for (int i = 0; i < ins->un.imm_i32; i++) {
                    GAOBJ_DEC_REF(args[i]);
                }

                GAOBJ_DEC_REF(obj);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(GET_ATTR): {
                struct ga_obj *obj = STACK_TOP();
                struct ga_obj *attr = GAOBJ_GETATTR(obj, vm, ins->un.imm_str);
                
                if (attr) {
                    STACK_SET_TOP(GAOBJ_INC_REF(attr));
                } else {
                    STACK_SHRINK(1)
                    vm_raise_exception(vm, ga_attribute_error_new(ins->un.imm_str));
                }
                GAOBJ_DEC_REF(obj);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(SET_ATTR): {
                struct ga_obj *obj = STACK_TOP();
                struct ga_obj *val = STACK_SECOND();

                STACK_SHRINK(2);

                GAOBJ_SETATTR(obj, vm, ins->un.imm_str, val);

                GAOBJ_DEC_REF(obj);
                GAOBJ_DEC_REF(val);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(LOAD_CONST): {
                STACK_PUSH(GAOBJ_INC_REF(ins->un.imm_obj));

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(LOAD_FAST): {
                struct stackframe *cur = frame;

                while (cur && !cur->fast_cells[ins->un.imm_i32]) {
                    cur = cur->captive;
                }

                STACK_PUSH(GAOBJ_INC_REF(cur->fast_cells[ins->un.imm_i32]));

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(LOAD_GLOBAL): {
                struct ga_obj *obj = GAOBJ_GETATTR(frame->code->mod, vm, ins->un.imm_str);

                if (!obj) {
                    vm_raise_exception(vm, ga_name_error_new(ins->un.imm_str));
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

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(LOAD_FALSE): {
                STACK_PUSH(GAOBJ_INC_REF(&ga_bool_false_inst));

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(STORE_FAST): {
                struct ga_obj *obj = STACK_POP();

                if (locals[ins->un.imm_i32]) {
                    GAOBJ_DEC_REF(locals[ins->un.imm_i32]);
                }

                locals[ins->un.imm_i32] = obj;

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(STORE_GLOBAL): {
                struct ga_obj *obj = STACK_POP();

                GAOBJ_SETATTR(frame->code->mod, vm, ins->un.imm_str, obj);

                GAOBJ_DEC_REF(obj);

                NEXT_INSTRUCTION();
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
            case JUMP_TARGET(POP): {
                GAOBJ_DEC_REF(STACK_POP());
                
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(RET): {
                return_val = STACK_POP();
                sentinel = true;
                break;
            }
			case JUMP_TARGET(LOGICAL_NOT): {
                struct ga_obj *top = STACK_TOP();
                struct ga_obj *res = GAOBJ_LOGICAL_NOT(top, vm);

                if (res) {
                    STACK_SET_TOP(GAOBJ_INC_REF(res));
                }

                GAOBJ_DEC_REF(top);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(NEGATE): {
                struct ga_obj *top = STACK_TOP();
                struct ga_obj *res = GAOBJ_NEGATE(top, vm);
                
                if (res) {
                    STACK_SET_TOP(GAOBJ_INC_REF(res));
                }

                GAOBJ_DEC_REF(top);
                
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(NOT): {
                struct ga_obj *top = STACK_TOP();
                struct ga_obj *res = GAOBJ_NOT(top, vm);

                if (res) {
                    STACK_SET_TOP(GAOBJ_INC_REF(res));
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

                STACK_SET_TOP(GAOBJ_INC_REF(res));

                GAOBJ_DEC_REF(right);
                GAOBJ_DEC_REF(left);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(SUB): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GAOBJ_SUB(left, vm, right);

                STACK_SET_TOP(GAOBJ_INC_REF(res));

                GAOBJ_DEC_REF(right);
                GAOBJ_DEC_REF(left);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(MUL): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GAOBJ_MUL(left, vm, right);

                STACK_SET_TOP(GAOBJ_INC_REF(res));

                GAOBJ_DEC_REF(right);
                GAOBJ_DEC_REF(left);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(DIV): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GAOBJ_DIV(left, vm, right);

                STACK_SET_TOP(GAOBJ_INC_REF(res));

                GAOBJ_DEC_REF(right);
                GAOBJ_DEC_REF(left);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(MOD): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GAOBJ_MOD(left, vm, right);

                STACK_SET_TOP(GAOBJ_INC_REF(res));

                GAOBJ_DEC_REF(right);
                GAOBJ_DEC_REF(left);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(AND): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GAOBJ_AND(left, vm, right);

                STACK_SET_TOP(GAOBJ_INC_REF(res));

                GAOBJ_DEC_REF(right);
                GAOBJ_DEC_REF(left);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(OR): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GAOBJ_OR(left, vm, right);

                STACK_SET_TOP(GAOBJ_INC_REF(res));

                GAOBJ_DEC_REF(right);
                GAOBJ_DEC_REF(left);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(XOR): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GAOBJ_XOR(left, vm, right);

                STACK_SET_TOP(GAOBJ_INC_REF(res));

                GAOBJ_DEC_REF(right);
                GAOBJ_DEC_REF(left);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(SHL): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GAOBJ_SHL(left, vm, right);

                STACK_SET_TOP(GAOBJ_INC_REF(res));

                GAOBJ_DEC_REF(right);
                GAOBJ_DEC_REF(left);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(SHR): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GAOBJ_SHR(left, vm, right);

                STACK_SET_TOP(GAOBJ_INC_REF(res));

                GAOBJ_DEC_REF(right);
                GAOBJ_DEC_REF(left);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(BUILD_RANGE_CLOSED): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GAOBJ_CLOSED_RANGE(left, vm, right);

                STACK_SET_TOP(GAOBJ_INC_REF(res));

                GAOBJ_DEC_REF(right);
                GAOBJ_DEC_REF(left);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(BUILD_RANGE_HALF): {
                struct ga_obj *right= STACK_POP();
                struct ga_obj *left = STACK_TOP();
                struct ga_obj *res = GAOBJ_HALF_RANGE(left, vm, right);

                STACK_SET_TOP(GAOBJ_INC_REF(res));

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
                JUMP_TO(ins->un.imm_i32);
            }
            case JUMP_TARGET(JUMP_DUP_IF_FALSE): {
                struct ga_obj *obj = STACK_POP();

                if (!GAOBJ_IS_TRUE(obj, vm)) {
                    STACK_PUSH(obj);
                    JUMP_TO(ins->un.imm_i32);
                }
                
                GAOBJ_DEC_REF(obj);
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(JUMP_DUP_IF_TRUE): {
                struct ga_obj *obj = STACK_POP();

                if (GAOBJ_IS_TRUE(obj, vm)) {
                    STACK_PUSH( obj);
                    JUMP_TO(ins->un.imm_i32);
                }

                GAOBJ_DEC_REF(obj);
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(JUMP_IF_FALSE): {
                struct ga_obj *obj = STACK_POP();

                if (!GAOBJ_IS_TRUE(obj, vm)) {
                    GAOBJ_DEC_REF(obj);
                    JUMP_TO(ins->un.imm_i32);
                }

                GAOBJ_DEC_REF(obj);
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(JUMP_IF_TRUE): {
                struct ga_obj *obj = STACK_POP();

                if (GAOBJ_IS_TRUE(obj, vm)) {
                    GAOBJ_DEC_REF(obj);
                    JUMP_TO(ins->un.imm_i32);
                }

                GAOBJ_DEC_REF(obj);
                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(PUSH_EXCEPTION_HANDLER): {
                vm_push_exception_handler(frame, &bytecode[ins->un.imm_i32-1]);

                NEXT_INSTRUCTION();
            }
            case JUMP_TARGET(POP_EXCEPTION_HANDLER): { 
                vm_pop_exception_handler(frame);
                
                NEXT_INSTRUCTION();
            }
            default:
                break;
        }
    }

    if (stackpointer != frame->stack) {
        vm_panic(frame, VM_STACK_OVERFLOW, "stack inbalance!");
    }

    vm->top = frame->parent;

    STACKFRAME_DESTROY(frame);

    return GAOBJ_MOVE_REF(return_val);
}

void
vm_raise_exception(struct vm *vm, struct ga_obj *exception)
{
    struct stackframe *top = vm->top;

    while (top && top->exception_stack_top == 0) {
        *top->sentinel_ptr = true;
        top = top->parent;
    }

    GAOBJ_INC_REF(exception);

    if (!top) {
        fputs("Unhandled exception: \n", stderr);
        struct ga_obj *msg = GAOBJ_INC_REF(GAOBJ_STR(exception, vm));
        fprintf(stderr, "%s\n", ga_str_to_cstring(msg));
        GAOBJ_DEC_REF(msg);
        GAOBJ_DEC_REF(exception);
        return;
    }
    
    *top->ins_ptr = vm_pop_exception_handler(top);
    vm_push(top, exception);
}
