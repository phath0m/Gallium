#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gallium/builtins.h>
#include <gallium/compiler.h>
#include <gallium/dict.h>
#include <gallium/list.h>
#include <gallium/object.h>
#include <compiler/ast.h>
#include <compiler/parser.h>
#include <runtime/bytecode.h>

/*
 * I can't be arsed to write a proper stack implementation. This is quite evident
 * throughout my code
 */
#define BREAK_LABELS_MAX    128
#define CONTINUE_LABELS_MAX 128

/* A label, a position or address inside a chunk of bytecode */
typedef uint16_t label_t;

/* A temporary local variable */
typedef uint8_t temporary_t;

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
    int             local_slot_start;
    int             local_slot_counter;
    int             label_counter;
    int             labels_size;
    label_t     *   labels;
    int             break_label_counter;
    int             continue_label_counter;
    label_t         break_labels[BREAK_LABELS_MAX];
    label_t         continue_labels[CONTINUE_LABELS_MAX];
};

/* variable information */
struct var_info {
    const char  *   name;
    int             slot;
    bool            global;
};

static void
proc_destroy_cb(void *p, void *s)
{
    free(p);
}

static void
proc_destroy(struct proc *proc)
{
    dict_destroy(proc->symbols, proc_destroy_cb, NULL);
    list_destroy(proc->bytecode, proc_destroy_cb, NULL);
    free(proc->labels);
    free(proc);
}

static struct proc *
proc_new(struct proc *parent)
{
    struct proc *proc = calloc(sizeof(struct proc), 1);
    proc->bytecode = list_new();
    proc->symbols = dict_new();
    proc->labels_size = 256;
    proc->labels = calloc(proc->labels_size*sizeof(label_t), 1);
    proc->parent = parent;
    
    if (parent) {
        proc->local_slot_start = parent->local_slot_counter;
        proc->local_slot_counter = parent->local_slot_counter;
    }

    return proc;
}

static void
proc_declare_var(struct proc *proc, const char *name)
{
    struct var_info *info = calloc(sizeof(struct var_info), 1);

    info->name = name;
    
    if (!proc->parent) {
        info->global = true;
    } else {
        info->slot = proc->local_slot_counter++;

        struct proc *parent = proc->parent;

        while (parent && parent->parent) {
            parent->local_slot_counter++;
            parent = parent->parent;
        }
    }

    dict_set(proc->symbols, name, info);
}

static bool
proc_var_declared(struct proc *proc, const char *name)
{
    struct proc *cur = proc;

    while (cur) {

        if (dict_has_key(cur->symbols, name)) {
            return true;
        }

        cur = cur->parent;
    }

    return false;
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

static int
proc_get_local_slot(struct proc *proc, const char *name)
{
    struct var_info *info;

    if (!dict_get(proc->symbols, name, (void**)&info)) {
        return -1;
    }

    return info->slot;
}

static bool
proc_is_local(struct proc *proc, const char *name)
{
    if (proc->parent && proc_is_local(proc->parent, name)) {
        return true;
    }


    struct var_info *info;

    if (!dict_get(proc->symbols, name, (void**)&info)) {
        return false;
    }

    return !info->global;
}

static void
proc_emit_store(struct proc *proc, const char *name)
{
    if (proc_is_local(proc, name)) {
        proc_emit_i32(proc, STORE_FAST, proc_get_local_slot(proc, name));
    } else {
        proc_emit_name(proc, STORE_GLOBAL, name);
    }
}

static void
proc_mark_label(struct proc *proc, label_t label)
{
    proc->labels[label] = LIST_COUNT(proc->bytecode);
}

static label_t
proc_pop_break_label(struct proc *proc)
{
    return proc->break_labels[--proc->break_label_counter];
}

static label_t
proc_pop_continue_label(struct proc *proc)
{
    return proc->continue_labels[--proc->continue_label_counter];
}

static void
proc_push_break_label(struct proc *proc, label_t label)
{
    proc->break_labels[proc->break_label_counter++] = label;
}

static void
proc_push_continue_label(struct proc *proc, label_t label)
{
    proc->continue_labels[proc->continue_label_counter++] = label;
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

static temporary_t
proc_reserve_temporary(struct proc *proc)
{
    return proc->local_slot_counter++;
}

static struct ga_code *
proc_to_code(struct proc *proc)
{
    struct ga_ins *bytecode = calloc(sizeof(struct ga_ins)*LIST_COUNT(proc->bytecode), 1);
    struct ga_code *code = calloc(sizeof(struct ga_code), 1);

    code->bytecode = bytecode;
    code->locals_start = proc->local_slot_start;

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
        proc_emit_i32(proc, LOAD_FAST, proc_get_local_slot(proc, term->name));
    } else {
        proc_emit_name(proc, LOAD_GLOBAL, term->name);
    }
}

static void
compile_bool(struct proc *proc, struct ast_node *node)
{
    struct bool_term *term = (struct bool_term*)node;

    if (term->val) {
        proc_emit(proc, LOAD_TRUE);
    } else {
        proc_emit(proc, LOAD_FALSE);
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
compile_index_access(struct proc *proc, struct ast_node *node)
{
    struct index_access_expr *expr = (struct index_access_expr*)node;
    compile_expr(proc, expr->key);
    compile_expr(proc, expr->expr);
    proc_emit(proc, LOAD_INDEX);
}

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
          
            if (!proc_var_declared(proc, term->name)) {
                proc_declare_var(proc, term->name); 
            }

            proc_emit_store(proc, term->name);
            break;
        }
        case AST_INDEX_ACCESS_EXPR: {
            struct index_access_expr *expr = (struct index_access_expr*)dest;
            compile_expr(proc, expr->key);
            compile_expr(proc, expr->expr);
            proc_emit(proc, STORE_INDEX);
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
compile_dict(struct proc *proc, struct ast_node *node)
{
    struct dict_expr *expr = (struct dict_expr*)node;
    struct key_val_expr *kvp;

    list_iter_t iter;
    list_get_iter(expr->kvp_pairs, &iter);

    while (iter_next_elem(&iter, (void**)&kvp)) {
        compile_expr(proc, kvp->val);
        compile_expr(proc, kvp->key);
    }
    
    proc_emit_i32(proc, BUILD_DICT, LIST_COUNT(expr->kvp_pairs));
}

static void
compile_list(struct proc *proc, struct ast_node *node)
{
    struct list_expr *expr = (struct list_expr*)node;
    struct ast_node *item;

    list_iter_t iter;
    list_get_iter(expr->items, &iter);

    while (iter_next_elem(&iter, (void**)&item)) {
        compile_expr(proc, item);
    }

    proc_emit_i32(proc, BUILD_LIST, LIST_COUNT(expr->items));
}

static void
compile_tuple(struct proc *proc, struct ast_node *node)
{
    struct tuple_expr *expr = (struct tuple_expr*)node;
    struct ast_node *item;

    list_iter_t iter;
    list_get_iter(expr->items, &iter);

    while (iter_next_elem(&iter, (void**)&item)) {
        compile_expr(proc, item);
    }

    proc_emit_i32(proc, BUILD_TUPLE, LIST_COUNT(expr->items));
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
        case BINOP_MOD:
            proc_emit(proc, MOD);
            break;
        case BINOP_AND:
            proc_emit(proc, AND);
            break;
        case BINOP_OR:
            proc_emit(proc, OR);
            break;
        case BINOP_XOR:
            proc_emit(proc, XOR);
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
        case BINOP_HALF_RANGE:
            proc_emit(proc, BUILD_RANGE_HALF);
            break;
        case BINOP_CLOSED_RANGE:
            proc_emit(proc, BUILD_RANGE_CLOSED);
            break;
        case BINOP_SHL:
            proc_emit(proc, SHL);
            break;
        case BINOP_SHR:
            proc_emit(proc, SHR);
            break;
        default:
            break;
    }
}

static void
compile_unary_expr(struct proc *proc, struct ast_node *node)
{
    struct unary_expr *expr = (struct unary_expr*)node;

    compile_expr(proc, expr->expr);

    switch (expr->op) {
        case UNARYOP_LOGICAL_NOT:
            proc_emit(proc, LOGICAL_NOT);
            break;
        case UNARYOP_NEGATE:
            proc_emit(proc, NEGATE);
            break;
        case UNARYOP_NOT:
            proc_emit(proc, NOT);
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

static void compile_func(struct proc *, struct ast_node *);

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
        case AST_UNARY_EXPR:
            compile_unary_expr(proc, expr);
            break;
        case AST_CALL_EXPR:
            compile_call_expr(proc, expr);
            break;
        case AST_DICT_EXPR:
            compile_dict(proc, expr);
            break;
        case AST_FUNC_EXPR:
            compile_func(proc, expr);
            break;
        case AST_TUPLE_EXPR:
            compile_tuple(proc, expr);
            break;
        case AST_INDEX_ACCESS_EXPR:
            compile_index_access(proc, expr);
            break;
        case AST_LIST_EXPR:
            compile_list(proc, expr);
            break;
        case AST_MEMBER_ACCESS_EXPR:
            compile_member_access(proc, expr);
            break;
        case AST_BOOL_TERM:
            compile_bool(proc, expr);
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
compile_for_stmt(struct proc *proc, struct ast_node *root)
{
    struct for_stmt *stmt = (struct for_stmt*)root;
    
    label_t begin_label = proc_reserve_label(proc);
    label_t end_label = proc_reserve_label(proc);
    temporary_t iterator = proc_reserve_temporary(proc);
    
    proc_declare_var(proc, stmt->var_name);

    compile_expr(proc, stmt->expr);
    proc_emit(proc, GET_ITER);
    proc_emit_i32(proc, STORE_FAST, iterator);
    proc_mark_label(proc, begin_label); 
    proc_emit_i32(proc, LOAD_FAST, iterator);
    proc_emit(proc, ITER_NEXT);
    proc_emit_label(proc, JUMP_IF_FALSE, end_label);
    proc_emit_i32(proc, LOAD_FAST, iterator);
    proc_emit(proc, ITER_CUR);
    proc_emit_store(proc, stmt->var_name);

    proc_push_continue_label(proc, begin_label);
    proc_push_break_label(proc, end_label);

    compile_stmt(proc, stmt->body); 

    proc_emit_label(proc, JUMP, begin_label);
    proc_mark_label(proc, end_label);
}

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
compile_try_stmt(struct proc *proc, struct ast_node *node)
{
    struct try_stmt *stmt = (struct try_stmt*)node;
    
    label_t except_label = proc_reserve_label(proc);
    label_t end_label = proc_reserve_label(proc);

    proc_emit_label(proc, PUSH_EXCEPTION_HANDLER, except_label);
    compile_stmt(proc, stmt->try_body);
    proc_emit(proc, POP_EXCEPTION_HANDLER);
    proc_emit_label(proc, JUMP, end_label);
    proc_mark_label(proc, except_label);

    if (stmt->has_var) {
        proc_declare_var(proc, stmt->var_name);
        proc_emit_store(proc, stmt->var_name);
    } else {
        proc_emit(proc, POP);
    }

    compile_stmt(proc, stmt->except_body);
    proc_mark_label(proc, end_label);
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
    
    proc_push_break_label(proc, end_label);
    proc_push_continue_label(proc, begin_label);

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
compile_class_decl(struct proc *proc, struct ast_node *node)
{
    struct class_decl *decl = (struct class_decl*)node;
    struct func_decl *method;

    list_iter_t iter;
    list_get_iter(decl->methods, &iter);

    while (iter_next_elem(&iter, (void**)&method)) {
        compile_func(proc, (struct ast_node*)method);
        proc_emit_obj(proc, LOAD_CONST, ga_str_from_cstring(method->name));
    }

    proc_emit_i32(proc, BUILD_DICT, LIST_COUNT(decl->methods));

    if (decl->base) {
        compile_expr(proc, decl->base);
    } else {
        proc_emit_name(proc, LOAD_GLOBAL, "Object");
    }

    proc_emit_name(proc, BUILD_CLASS, decl->name);
    proc_emit_name(proc, STORE_GLOBAL, decl->name);
}

static void
compile_func(struct proc *proc, struct ast_node *node)
{
    struct func_decl *func = (struct func_decl*)node;
    struct proc *func_proc = proc_new(proc);

    list_iter_t iter;
    list_get_iter(func->parameters, &iter);

    struct func_param *param;

    while (iter_next_elem(&iter, (void**)&param)) {
        proc_declare_var(func_proc, param->name);
        proc_emit_obj(proc, LOAD_CONST, ga_str_from_cstring(param->name));
    }
    
    proc_emit_i32(proc, BUILD_TUPLE, LIST_COUNT(func->parameters));
    compile_stmt(func_proc, func->body);
    proc_emit_obj(func_proc, LOAD_CONST, &ga_null_inst);
    proc_emit(func_proc, RET);
    
    if (!proc->parent) {
        proc_emit_ptr(proc, BUILD_FUNC, proc_to_code(func_proc));
    } else {
        proc_emit_ptr(proc, BUILD_CLOSURE, proc_to_code(func_proc));
    }
    
    proc_destroy(func_proc);
}

static void
compile_stmt(struct proc *proc, struct ast_node *node)
{
    switch (node->type) {
        case AST_BREAK_STMT:
            proc_emit_label(proc, JUMP, proc_pop_break_label(proc));
            break;
        case AST_CONTINUE_STMT:
            proc_emit_label(proc, JUMP, proc_pop_continue_label(proc));
            break;
        case AST_FOR_STMT:
            compile_for_stmt(proc, node);
            break;
        case AST_IF_STMT:
            compile_if_stmt(proc, node);
            break;
        case AST_RETURN_STMT:
            compile_return_stmt(proc, node);
            break;
        case AST_TRY_STMT:
            compile_try_stmt(proc, node);
            break;
        case AST_WHILE_STMT:
            compile_while_stmt(proc, node);
            break;
        case AST_CODE_BLOCK:
            compile_code_block(proc, node);
            break;
        case AST_CLASS_DECL:
            compile_class_decl(proc, node);
            break;
        case AST_FUNC_DECL: {
            struct func_decl *decl = (struct func_decl*)node;
            compile_func(proc, node);
        
            if (proc->parent) {
                proc_declare_var(proc, decl->name);
                proc_emit_i32(proc, STORE_FAST, proc_get_local_slot(proc, decl->name));
            } else {
                proc_emit_name(proc, STORE_GLOBAL, decl->name);
            }
            break;
        }
        case AST_EMPTY_STMT: 
            break;
        default:
            compile_expr(proc, node);
            proc_emit(proc, POP);
            break;
    }
}

struct ga_obj *
compiler_compile(struct compiler_state *statep, const char *src)
{
    struct ast_node *root = parser_parse(&statep->parse_state, src);

    if (!root) {
        statep->comp_errno = COMPILER_SYNTAX_ERROR;
        return NULL;
    }

    struct proc *proc = proc_new(NULL);

    compile_stmt(proc, root);
    proc_emit_obj(proc, LOAD_CONST, &ga_null_inst);
    proc_emit(proc, RET);

    struct ga_code *code = proc_to_code(proc);

    proc_destroy(proc);

    return ga_mod_new("__default__", code);
}

void
compiler_explain(struct compiler_state *statep)
{
    switch (statep->comp_errno) {
        case COMPILER_SYNTAX_ERROR:
            parser_explain(&statep->parse_state);
            break;
        default:
            fputs("but I do not know why", stderr);
            break;
    }
}
