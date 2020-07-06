#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ds/dict.h>
#include <ds/list.h>
#include <emit/compiler.h>
#include <gallium/builtins.h>
#include <gallium/object.h>
#include <parser/ast.h>
#include <parser/parser.h>
#include <runtime/bytecode.h>

/* A label, a position or address inside a chunk of bytecode */
typedef uint16_t label_t;

struct proc_ins {
    struct ga_ins   ins;        /* actual bytecode instruction */
    label_t         label;      /* label this references */
    bool            is_label_ref;  /* does this reference a label? */
};

/* procedure */
struct proc {
    struct list *   bytecode;
    struct dict *   symbols;
    struct proc *   parent;
    int             label_counter;
    int             labels_size;
    label_t     *   labels;
};

static struct proc *
proc_new()
{
    struct proc *proc = calloc(sizeof(struct proc), 1);
    proc->bytecode = list_new();
    proc->symbols = dict_new();
    proc->labels_size = 256;
    proc->labels = calloc(proc->labels_size*sizeof(label_t), 1);
    return proc;
}

static void
proc_declare_var(struct proc *proc, const char *name)
{
    dict_set(proc->symbols, name, NULL);
}

static void
proc_emit(struct proc *proc, int opcode)
{
   struct proc_ins *ins = calloc(sizeof(struct proc_ins), 1);
   ins->ins.opcode = opcode;
   list_append(proc->bytecode, ins);
}

static void
proc_emit_i32(struct proc *proc, int opcode, uint32_t imm)
{
    struct proc_ins *ins = calloc(sizeof(struct proc_ins), 1);
    ins->ins.opcode = opcode;
    ins->ins.un.imm_i32 = imm;
    list_append(proc->bytecode, ins);
}

static void
proc_emit_label(struct proc *proc, int opcode, label_t label)
{
    struct proc_ins *ins = calloc(sizeof(struct proc_ins), 1);
    ins->ins.opcode = opcode;
    ins->is_label_ref = true;
    ins->label = label;
    list_append(proc->bytecode, ins);
}

static void
proc_emit_name(struct proc *proc, int opcode, const char *name)
{
    struct proc_ins *ins = calloc(sizeof(struct proc_ins), 1);
    ins->ins.opcode = opcode;
    ins->ins.un.imm_str = name;
    list_append(proc->bytecode, ins);
}

static void
proc_emit_obj(struct proc *proc, int opcode, struct ga_obj *obj)
{
    struct proc_ins *ins = calloc(sizeof(struct proc_ins), 1);
    ins->ins.opcode = opcode;
    ins->ins.un.imm_obj = GAOBJ_INC_REF(obj);
    list_append(proc->bytecode, ins);
}

static void
proc_emit_ptr(struct proc *proc, int opcode, void *ptr)
{
    struct proc_ins *ins = calloc(sizeof(struct proc_ins), 1);
    ins->ins.opcode = opcode;
    ins->ins.un.imm_ptr = ptr;
    list_append(proc->bytecode, ins);
}

static bool
proc_is_local(struct proc *proc, const char *name)
{
    return dict_has_key(proc->symbols, name);
}

static void
proc_mark_label(struct proc *proc, label_t label)
{
    proc->labels[label] = LIST_COUNT(proc->bytecode);
}

static label_t
proc_reserve_label(struct proc *proc)
{
    if (proc->label_counter >= proc->labels_size) {
        proc->labels_size += 256;
        proc->labels = (label_t*)realloc(proc->labels, proc->labels_size*sizeof(label_t));
    }

    return proc->label_counter++;
}

static struct ga_code *
proc_to_code(struct proc *proc)
{
    struct ga_ins *bytecode = calloc(sizeof(struct ga_ins)*LIST_COUNT(proc->bytecode), 1);
    struct ga_code *code = calloc(sizeof(struct ga_code), 1);

    code->bytecode = bytecode;

    int i = 0;
    struct proc_ins *ins;
    list_iter_t iter;
    list_get_iter(proc->bytecode, &iter);

    while (iter_next_elem(&iter, (void**)&ins)) {
        
        if (ins->is_label_ref) {
            uint32_t label_addr = proc->labels[ins->label];
            ins->ins.un.imm_i32 = label_addr;
        }

        memcpy(&bytecode[i++], &ins->ins, sizeof(struct proc_ins));
    }

    return code;
}

static void
compile_symbol(struct proc *proc, struct ast_node *node)
{
    struct symbol_term *term = (struct symbol_term*)node;

    if (proc_is_local(proc, term->name)) {
        proc_emit_name(proc, LOAD_LOCAL, term->name);
    } else {
        proc_emit_name(proc, LOAD_GLOBAL, term->name);
    }
}

static void
compile_integer(struct proc *proc, struct ast_node *node)
{
    struct integer_term *term = (struct integer_term*)node;

    proc_emit_obj(proc, LOAD_CONST, ga_int_from_i64(term->val));
}

static void
compile_string(struct proc *proc, struct ast_node *node)
{
    struct string_term *term = (struct string_term*)node;

    /* TODO: implement ga_str_from_stringbuf!!!! */
    proc_emit_obj(proc, LOAD_CONST, ga_str_from_cstring(term->val));
}

static void compile_expr(struct proc *, struct ast_node *);

static void
compile_member_access(struct proc *proc, struct ast_node *node)
{
    struct member_access_expr *expr = (struct member_access_expr*)node;
    compile_expr(proc, expr->expr);
    proc_emit_name(proc, GET_ATTR, expr->member);
}

static void
compile_assign(struct proc *proc, struct ast_node *node)
{
    struct assign_expr *assignexpr = (struct assign_expr*)node;
    struct ast_node *dest = assignexpr->left;

    compile_expr(proc, assignexpr->right);
    proc_emit(proc, DUP);

    switch (dest->type) {
        case AST_SYMBOL_TERM: {
            struct symbol_term *term = (struct symbol_term*)dest;
            proc_declare_var(proc, term->name); 
            proc_emit_name(proc, STORE_LOCAL, term->name);
            break;
        }
        case AST_MEMBER_ACCESS_EXPR: {
            struct member_access_expr *expr = (struct member_access_expr*)dest;
            compile_expr(proc, expr->expr);
            proc_emit_name(proc, SET_ATTR, expr->member);
            break;
        }
        default: {
            printf("but I do not know why\n");
            break;
        }
    }
}

static void
compile_logical_binop(struct proc *proc, struct bin_expr *binexpr)
{
    label_t short_circuit = proc_reserve_label(proc);

    switch (binexpr->op) {
        case BINOP_LOGICAL_AND:
            compile_expr(proc, binexpr->left);
            proc_emit_label(proc, JUMP_DUP_IF_FALSE, short_circuit);
            compile_expr(proc, binexpr->right);
            proc_mark_label(proc, short_circuit);
            break;
        case BINOP_LOGICAL_OR:
            compile_expr(proc, binexpr->left);
            proc_emit_label(proc, JUMP_DUP_IF_TRUE, short_circuit);
            compile_expr(proc, binexpr->right);
            proc_mark_label(proc, short_circuit);
            break;
        default:
            break;
    }
}

static void
compile_binop(struct proc *proc, struct ast_node *node)
{
    struct bin_expr *binexpr = (struct bin_expr*)node;

    switch (binexpr->op) {
        case BINOP_LOGICAL_AND:
        case BINOP_LOGICAL_OR:
            compile_logical_binop(proc, binexpr);
            return;
        default:
            break;
    }

    compile_expr(proc, binexpr->left);
    compile_expr(proc, binexpr->right);

    switch (binexpr->op) {
        case BINOP_ADD:
            proc_emit(proc, ADD);
            break;
        case BINOP_SUB:
            proc_emit(proc, SUB);
            break;
        case BINOP_MUL:
            proc_emit(proc, MUL);
            break;
        case BINOP_DIV:
            proc_emit(proc, DIV);
            break;
        case BINOP_GREATER_THAN:
            proc_emit(proc, GREATER_THAN);
            break;
        case BINOP_GREATER_THAN_OR_EQU:
            proc_emit(proc, GREATER_THAN_OR_EQU);
            break;
        case BINOP_LESS_THAN:
            proc_emit(proc, LESS_THAN);
            break;
        case BINOP_LESS_THAN_OR_EQU:
            proc_emit(proc, LESS_THAN_OR_EQU);
            break;
        case BINOP_EQUALS:
            proc_emit(proc, EQUALS);
            break;
        case BINOP_NOT_EQUALS:
            proc_emit(proc, NOT_EQUALS);
            break;
        default:
            break;
    }
}

static void
compile_call_expr(struct proc *proc, struct ast_node *node)
{
    struct call_expr *call = (struct call_expr*)node;

    struct ast_node *arg;
    list_iter_t iter;
    list_get_iter(call->arguments, &iter);

    while (iter_next_elem(&iter, (void**)&arg)) {
        compile_expr(proc, arg);
    }

    compile_expr(proc, call->target);

    proc_emit_i32(proc, INVOKE, LIST_COUNT(call->arguments));
}

static void
compile_expr(struct proc *proc, struct ast_node *expr)
{
    switch (expr->type) {
        case AST_ASSIGN_EXPR:
            compile_assign(proc, expr);
            break;
        case AST_BIN_EXPR:
            compile_binop(proc, expr);
            break;
        case AST_CALL_EXPR:
            compile_call_expr(proc, expr);
            break;
        case AST_MEMBER_ACCESS_EXPR:
            compile_member_access(proc, expr);
            break;
        case AST_INTEGER_TERM:
            compile_integer(proc, expr);
            break;
        case AST_STRING_TERM:
            compile_string(proc, expr);
            break;
        case AST_SYMBOL_TERM:
            compile_symbol(proc, expr);
            break;
        default:
            printf("I don't know how.\n");
            break;
    }
}

static void compile_stmt(struct proc *, struct ast_node*);

static void
compile_if_stmt(struct proc *proc, struct ast_node *root)
{
    struct if_stmt *stmt = (struct if_stmt*)root;

    label_t end_label = proc_reserve_label(proc);
    label_t else_label = proc_reserve_label(proc);

    compile_expr(proc, stmt->cond);
    proc_emit_label(proc, JUMP_IF_FALSE, else_label);
    compile_stmt(proc, stmt->if_body);
    proc_emit_label(proc, JUMP, end_label);
    proc_mark_label(proc, else_label);
    
    if (stmt->else_body) {
        compile_stmt(proc, stmt->else_body);
    }

    proc_mark_label(proc, end_label);
}

static void
compile_return_stmt(struct proc *proc, struct ast_node *node)
{
    struct return_stmt *stmt = (struct return_stmt*)node;
    compile_expr(proc, stmt->val);
    proc_emit(proc, RET);
}

static void
compile_while_stmt(struct proc *proc, struct ast_node *root)
{
    struct while_stmt *stmt = (struct while_stmt*)root;
    
    label_t begin_label = proc_reserve_label(proc);
    label_t end_label = proc_reserve_label(proc);

    proc_mark_label(proc, begin_label);
    compile_expr(proc, stmt->cond);
    proc_emit_label(proc, JUMP_IF_FALSE, end_label);
    compile_stmt(proc, stmt->body);
    proc_emit_label(proc, JUMP, begin_label);
    proc_mark_label(proc, end_label);
}

static void
compile_code_block(struct proc *proc, struct ast_node *root)
{
    struct code_block *block = (struct code_block*)root;
    struct ast_node *node;
    list_iter_t iter;
    list_get_iter(block->children, &iter);

    while (iter_next_elem(&iter, (void**)&node)) {
        compile_stmt(proc, node);
    }
}

static void
compile_func_decl(struct proc *proc, struct ast_node *node)
{
    struct func_decl *decl = (struct func_decl*)node;
    struct proc *func_proc = proc_new();

    list_iter_t iter;
    list_get_iter(decl->parameters, &iter);

    struct func_param *param;

    while (iter_next_elem(&iter, (void**)&param)) {
        proc_declare_var(func_proc, param->name);
        proc_emit_obj(proc, LOAD_CONST, ga_str_from_cstring(param->name));
    }
    
    proc_emit_i32(proc, BUILD_TUPLE, LIST_COUNT(decl->parameters));
    compile_stmt(func_proc, decl->body);
    proc_emit_obj(func_proc, LOAD_CONST, &ga_null_inst);
    proc_emit(func_proc, RET);
    proc_emit_ptr(proc, BUILD_FUNC, proc_to_code(func_proc));
    proc_emit_name(proc, STORE_GLOBAL, decl->name);
}

static void
compile_stmt(struct proc *proc, struct ast_node *node)
{
    switch (node->type) {
        case AST_IF_STMT:
            compile_if_stmt(proc, node);
            break;
        case AST_RETURN_STMT:
            compile_return_stmt(proc, node);
            break;
        case AST_WHILE_STMT:
            compile_while_stmt(proc, node);
            break;
        case AST_CODE_BLOCK:
            compile_code_block(proc, node);
            break;
        case AST_FUNC_DECL:
            compile_func_decl(proc, node);
            break;
        default:
            compile_expr(proc, node);
            proc_emit(proc, POP);
            break;
    }
}

struct ga_obj *
compiler_compile(struct compiler *comp, const char *src)
{
    struct parser_state parse_state;
    memset(&parse_state, 0, sizeof(parse_state));

    struct ast_node *root = parser_parse(&parse_state, src);

    if (!root) {
        parser_explain(&parse_state);
        return NULL;
    }
    struct proc *proc = proc_new();

    compile_stmt(proc, root);
    proc_emit_obj(proc, LOAD_CONST, &ga_null_inst);
    proc_emit(proc, RET);

    struct ga_code *code = proc_to_code(proc);

    return ga_mod_new("__default__", code);
}
