#ifndef _GALLIUM_OPCODE_H
#define _GALLIUM_OPCODE_H

#define NOOP                    0x00    /* No operation */
#define LOAD_CONST              0x01    /* Push a constant onto the top  */
#define LOAD_GLOBAL             0x03    /* Pushes a global variable onto the stack */
#define STORE_LOCAL             0x04    /* Stores a local variable onto the stack */
#define STORE_GLOBAL            0x05    /* Stores a global variable */
#define POP                     0x06    /* Remove item from the top of the stack */
#define ADD                     0x07    /* Add two values on the stack togther */
#define SUB                     0x08    /* Subtracts two values on the stack */
#define MUL                     0x09    /* Multiplies two values on the stack */
#define DIV                     0x0A    /* Divides two values on the stack */
#define RET                     0x0B    /* Returns; breaks out of current function. */
#define LOAD_TRUE               0x0C    /* Pushes "true" */
#define LOAD_FALSE              0x0D    /* Pushes "false" */
#define JUMP_IF_TRUE            0x0E    /* Jumps if the value on the stack is true */
#define JUMP_IF_FALSE           0x0F    /* Jumps if the value on the stack is false */
#define INVOKE                  0x10    /* Calls the item on the stack with x arguments */
#define DUP                     0x11    /* Duplicates the value on the stack */
#define BUILD_TUPLE             0x12    /* Constructs a tuple from items on the stack */
#define BUILD_LIST              0x13    /* Constructs a list from items on the stack */
#define BUILD_DICT              0x14    /* Constructs a dictionary from items on the stack */
#define BUILD_FUNC              0x15    /* Constructs a function */
#define EQUALS                  0x16    /* Compares two values on the stack; pushes true if equal */
#define NOT_EQUALS              0x17    /* Compares two values on stack; pushes true if not equal */
#define GREATER_THAN            0x18    /* compares two values on the stack; pushes true if greater than */
#define LESS_THAN               0x19    /* compares two values on the stack; pushes true if less than */
#define GREATER_THAN_OR_EQU     0x1A    /* compares two values on the stack; pushes true if greater than or equal */
#define LESS_THAN_OR_EQU        0x1B    /* compares two values on the stack; pushes true if less than or equal */
#define JUMP			        0x1C    /* Jumps to address stored in immediate */
#define JUMP_DUP_IF_TRUE        0x1D    /* Duplicates item ontop of stack, jumps if true */
#define JUMP_DUP_IF_FALSE       0x1E    /* Duplicates item ontop of stack, jumps if false */
#define SET_ATTR                0x1F    /* Sets an attribute of an object on the stack */
#define GET_ATTR                0x20    /* Gets an attribute of an object on the stack */
#define PUSH_EXCEPTION_HANDLER  0x21    /* Pushes the address of an exception handler */
#define POP_EXCEPTION_HANDLER   0x22    /* Pops the last pushed exception handler from the exception handler stack */
#define LOAD_INDEX              0x23    /* Loads an index */
#define STORE_INDEX             0x24    /* Stores an index */
#define BUILD_CLASS             0x25    /* Constructs a class */
#define MOD                     0x26    /* Remainder of integer division of two items on the stack*/
#define AND                     0x27
#define OR                      0x28
#define XOR                     0x29
#define SHL                     0x2A
#define SHR                     0x2B
#define GET_ITER                0x2C
#define ITER_NEXT               0x2D
#define ITER_CUR                0x2E
#define STORE_FAST              0x2F
#define LOAD_FAST               0x30
#define BUILD_RANGE_CLOSED      0x31
#define BUILD_RANGE_HALF        0x32
#define BUILD_CLOSURE           0x33
#define NEGATE                  0x34
#define NOT                     0x35
#define LOGICAL_NOT             0x36
#define COMPILE_MACRO           0x37
#define INLINE_INVOKE           0x38
#define JUMP_IF_COMPILED        0x39
#define LOAD_EXCEPTION          0x3A

#endif
