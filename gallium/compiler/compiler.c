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

struct proc_builder_ins {
    struct ga_ins   ins;        /* actual bytecode instruction */
    label_t         label;      /* label this references */
    bool            is_label_ref;  /* does this reference a label? */
};

/* procedure builder */
struct proc_builder {
    struct list *           bytecode;
    struct dict *           symbols;
    struct proc_builder *   parent;
    int                     local_slot_start;
    int                     local_slot_counter;
    int                     label_counter;
    int                     labels_size;
    label_t     *           labels;
    int                     break_label_counter;
    int                     continue_label_counter;                 /* break label stack pointer */
    label_t                 break_labels[BREAK_LABELS_MAX];         /* stack for "break" labels */
    label_t                 continue_labels[CONTINUE_LABELS_MAX];   /* stack for "continue" labels */
};

/* variable information */
struct var_info {
    const char  *   name;
    int             slot;
    bool            global;
};

/* adds an object to the module's module data (to be tracked for deletion) */
static struct ga_obj *
add_constant(struct compiler_state *statep, struct ga_obj *obj)
{
    list_append(statep->mod_data->constants, obj);

    return obj;
}

/* copies a string to the module's string table */
static const char *
add_string(struct compiler_state *statep, const char *src)
{
    char *src_copy = calloc(strlen(src)+1, 1);

    strcpy(src_copy, src);
    list_append(statep->mod_data->strings, src_copy);

    return src_copy;
}

static void
builder_destroy_cb(void *p, void *s)
{
    free(p);
}

static void
builder_destroy(struct proc_builder *builder)
{
    dict_destroy(builder->symbols, builder_destroy_cb, NULL);
    list_destroy(builder->bytecode, builder_destroy_cb, NULL);
    free(builder->labels);
    free(builder);
}

static struct proc_builder *
builder_new(struct proc_builder *parent)
{
    struct proc_builder *builder = calloc(sizeof(struct proc_builder), 1);

    builder->bytecode = list_new();
    builder->symbols = dict_new();
    builder->labels_size = 256;
    builder->labels = calloc(builder->labels_size*sizeof(label_t), 1);
    builder->parent = parent;
    
    if (parent) {
        builder->local_slot_start = parent->local_slot_counter;
        builder->local_slot_counter = parent->local_slot_counter;
    }

    return builder;
}

static void
builder_declare_var(struct proc_builder *builder, const char *name)
{
    struct var_info *info = calloc(sizeof(struct var_info), 1);

    info->name = name;
    
    if (!builder->parent) {
        info->global = true;
    } else {
        info->slot = builder->local_slot_counter++;

        struct proc_builder *parent = builder->parent;

        while (parent && parent->parent) {
            parent->local_slot_counter++;
            parent = parent->parent;
        }
    }

    dict_set(builder->symbols, name, info);
}

static bool
builder_has_var(struct proc_builder *builder, const char *name)
{
    struct proc_builder *cur = builder;

    while (cur) {

        if (dict_has_key(cur->symbols, name)) {
            return true;
        }

        cur = cur->parent;
    }

    return false;
}

static void
builder_emit(struct proc_builder *builder, int opcode)
{
   struct proc_builder_ins *ins = calloc(sizeof(struct proc_builder_ins), 1);
   ins->ins.opcode = opcode;
   list_append(builder->bytecode, ins);
}

static void
builder_emit_i32(struct proc_builder *builder, int opcode, uint32_t imm)
{
    struct proc_builder_ins *ins = calloc(sizeof(struct proc_builder_ins), 1);
    ins->ins.opcode = opcode;
    ins->ins.un.imm_i32 = imm;
    list_append(builder->bytecode, ins);
}

static void
builder_emit_label(struct proc_builder *builder, int opcode, label_t label)
{
    struct proc_builder_ins *ins = calloc(sizeof(struct proc_builder_ins), 1);
    ins->ins.opcode = opcode;
    ins->is_label_ref = true;
    ins->label = label;
    list_append(builder->bytecode, ins);
}

static void
builder_emit_name(struct compiler_state *statep, struct proc_builder *builder, int opcode, const char *name)
{
    struct proc_builder_ins *ins = calloc(sizeof(struct proc_builder_ins), 1);
    ins->ins.opcode = opcode;
    ins->ins.un.imm_str = add_string(statep, name);
    list_append(builder->bytecode, ins);
}

static void
builder_emit_obj(struct proc_builder *builder, int opcode, struct ga_obj *obj)
{
    struct proc_builder_ins *ins = calloc(sizeof(struct proc_builder_ins), 1);
    ins->ins.opcode = opcode;
    ins->ins.un.imm_obj = GAOBJ_INC_REF(obj);
    list_append(builder->bytecode, ins);
}

static void
builder_emit_ptr(struct proc_builder *builder, int opcode, void *ptr)
{
    struct proc_builder_ins *ins = calloc(sizeof(struct proc_builder_ins), 1);
    ins->ins.opcode = opcode;
    ins->ins.un.imm_ptr = ptr;
    list_append(builder->bytecode, ins);
}

static int
builder_get_local_slot(struct proc_builder *builder, const char *name)
{
    struct var_info *info;

    if (!dict_get(builder->symbols, name, (void**)&info)) {
        return builder_get_local_slot(builder->parent, name);
    }

    return info->slot;
}

static bool
builder_has_local(struct proc_builder *builder, const char *name)
{
    if (builder->parent && builder_has_local(builder->parent, name)) {
        return true;
    }


    struct var_info *info;

    if (!dict_get(builder->symbols, name, (void**)&info)) {
        return false;
    }

    return !info->global;
}

static void
builder_emit_store(struct compiler_state *statep, struct proc_builder *builder, const char *name)
{
    if (builder_has_local(builder, name)) {
        builder_emit_i32(builder, STORE_FAST, builder_get_local_slot(builder, name));
    } else {
        builder_emit_name(statep, builder, STORE_GLOBAL, name);
    }
}

static void
builder_mark_label(struct proc_builder *builder, label_t label)
{
    builder->labels[label] = LIST_COUNT(builder->bytecode);
}

static label_t
builder_pop_break_label(struct proc_builder *builder)
{
    return builder->break_labels[--builder->break_label_counter];
}

static label_t
builder_pop_continue_label(struct proc_builder *builder)
{
    return builder->continue_labels[--builder->continue_label_counter];
}

static void
builder_push_break_label(struct proc_builder *builder, label_t label)
{
    builder->break_labels[builder->break_label_counter++] = label;
}

static void
builder_push_continue_label(struct proc_builder *builder, label_t label)
{
    builder->continue_labels[builder->continue_label_counter++] = label;
}

static label_t
builder_reserve_label(struct proc_builder *builder)
{
    if (builder->label_counter >= builder->labels_size) {
        builder->labels_size += 256;
        builder->labels = (label_t*)realloc(builder->labels, builder->labels_size*sizeof(label_t));
    }

    return builder->label_counter++;
}

static temporary_t
builder_reserve_temporary(struct proc_builder *builder)
{
    return builder->local_slot_counter++;
}

static struct ga_proc *
builder_finalize(struct proc_builder *builder)
{
    struct ga_ins *bytecode = calloc(sizeof(struct ga_ins)*LIST_COUNT(builder->bytecode), 1);
    struct ga_proc *code = calloc(sizeof(struct ga_proc), 1);

    code->bytecode = bytecode;
    code->locals_start = builder->local_slot_start;
    code->compiler_private = builder;

    int i = 0;
    struct proc_builder_ins *ins;
    list_iter_t iter;
    list_get_iter(builder->bytecode, &iter);

    while (iter_next_elem(&iter, (void**)&ins)) {
        
        if (ins->is_label_ref) {
            uint32_t label_addr = builder->labels[ins->label];
            ins->ins.un.imm_i32 = label_addr;
        }

        memcpy(&bytecode[i++], &ins->ins, sizeof(struct proc_builder_ins));
    }

    return code;
}

static void
compile_symbol(struct compiler_state *statep, struct proc_builder *builder, struct ast_node *node)
{
    struct symbol_term *term = (struct symbol_term*)node;

    if (builder_has_local(builder, term->name)) {
        builder_emit_i32(builder, LOAD_FAST, builder_get_local_slot(builder, term->name));
    } else {
        builder_emit_name(statep, builder, LOAD_GLOBAL, term->name);
    }
}

static void
compile_bool(struct compiler_state *statep, struct proc_builder *builder, struct ast_node *node)
{
    struct bool_term *term = (struct bool_term*)node;

    if (term->val) {
        builder_emit(builder, LOAD_TRUE);
    } else {
        builder_emit(builder, LOAD_FALSE);
    }
}

static void
compile_integer(struct compiler_state *statep, struct proc_builder *builder, struct ast_node *node)
{
    struct integer_term *term = (struct integer_term*)node;

    builder_emit_obj(builder, LOAD_CONST, add_constant(statep, ga_int_from_i64(term->val)));
}

static void
compile_string(struct compiler_state *statep, struct proc_builder *builder, struct ast_node *node)
{
    struct string_term *term = (struct string_term*)node;

    /* TODO: implement ga_str_from_stringbuf!!!! */
    builder_emit_obj(builder, LOAD_CONST, add_constant(statep, ga_str_from_cstring(term->val)));
}

static void compile_expr(struct compiler_state *statep, struct proc_builder *, struct ast_node *);

static void
compile_index_access(struct compiler_state *statep, struct proc_builder *builder, struct ast_node *node)
{
    struct index_access_expr *expr = (struct index_access_expr*)node;
    compile_expr(statep, builder, expr->key);
    compile_expr(statep, builder, expr->expr);
    builder_emit(builder, LOAD_INDEX);
}

static void
compile_member_access(struct compiler_state *statep, struct proc_builder *builder, struct ast_node *node)
{
    struct member_access_expr *expr = (struct member_access_expr*)node;
    compile_expr(statep, builder, expr->expr);
    builder_emit_name(statep, builder, GET_ATTR, expr->member);
}

static void
compile_assign(struct compiler_state *statep, struct proc_builder *builder, struct ast_node *node)
{
    struct assign_expr *assignexpr = (struct assign_expr*)node;
    struct ast_node *dest = assignexpr->left;

    compile_expr(statep, builder, assignexpr->right);
    builder_emit(builder, DUP);

    switch (dest->type) {
        case AST_SYMBOL_TERM: {
            struct symbol_term *term = (struct symbol_term*)dest;
          
            if (!builder_has_var(builder, term->name)) {
                builder_declare_var(builder, term->name); 
            }

            builder_emit_store(statep, builder, term->name);
            break;
        }
        case AST_INDEX_ACCESS_EXPR: {
            struct index_access_expr *expr = (struct index_access_expr*)dest;
            compile_expr(statep, builder, expr->key);
            compile_expr(statep, builder, expr->expr);
            builder_emit(builder, STORE_INDEX);
            break;
        }
        case AST_MEMBER_ACCESS_EXPR: {
            struct member_access_expr *expr = (struct member_access_expr*)dest;
            compile_expr(statep, builder, expr->expr);
            builder_emit_name(statep, builder, SET_ATTR, expr->member);
            break;
        }
        default: {
            printf("but I do not know why\n");
            break;
        }
    }
}

static void
compile_dict(struct compiler_state *statep, struct proc_builder *builder, struct ast_node *node)
{
    struct dict_expr *expr = (struct dict_expr*)node;
    struct key_val_expr *kvp;

    list_iter_t iter;
    list_get_iter(expr->kvp_pairs, &iter);

    while (iter_next_elem(&iter, (void**)&kvp)) {
        compile_expr(statep, builder, kvp->val);
        compile_expr(statep, builder, kvp->key);
    }
    
    builder_emit_i32(builder, BUILD_DICT, LIST_COUNT(expr->kvp_pairs));
}

static void
compile_list(struct compiler_state *statep, struct proc_builder *builder, struct ast_node *node)
{
    struct list_expr *expr = (struct list_expr*)node;
    struct ast_node *item;

    list_iter_t iter;
    list_get_iter(expr->items, &iter);

    while (iter_next_elem(&iter, (void**)&item)) {
        compile_expr(statep, builder, item);
    }

    builder_emit_i32(builder, BUILD_LIST, LIST_COUNT(expr->items));
}

static void
compile_tuple(struct compiler_state *statep, struct proc_builder *builder, struct ast_node *node)
{
    struct tuple_expr *expr = (struct tuple_expr*)node;
    struct ast_node *item;

    list_iter_t iter;
    list_get_iter(expr->items, &iter);

    while (iter_next_elem(&iter, (void**)&item)) {
        compile_expr(statep, builder, item);
    }

    builder_emit_i32(builder, BUILD_TUPLE, LIST_COUNT(expr->items));
}

static void
compile_logical_binop(struct compiler_state *statep, struct proc_builder *builder, struct bin_expr *binexpr)
{
    label_t short_circuit = builder_reserve_label(builder);

    switch (binexpr->op) {
        case BINOP_LOGICAL_AND:
            compile_expr(statep, builder, binexpr->left);
            builder_emit_label(builder, JUMP_DUP_IF_FALSE, short_circuit);
            compile_expr(statep, builder, binexpr->right);
            builder_mark_label(builder, short_circuit);
            break;
        case BINOP_LOGICAL_OR:
            compile_expr(statep, builder, binexpr->left);
            builder_emit_label(builder, JUMP_DUP_IF_TRUE, short_circuit);
            compile_expr(statep, builder, binexpr->right);
            builder_mark_label(builder, short_circuit);
            break;
        default:
            break;
    }
}

static void
compile_binop(struct compiler_state *statep, struct proc_builder *builder, struct ast_node *node)
{
    struct bin_expr *binexpr = (struct bin_expr*)node;

    switch (binexpr->op) {
        case BINOP_LOGICAL_AND:
        case BINOP_LOGICAL_OR:
            compile_logical_binop(statep, builder, binexpr);
            return;
        default:
            break;
    }

    compile_expr(statep, builder, binexpr->left);
    compile_expr(statep, builder, binexpr->right);

    switch (binexpr->op) {
        case BINOP_ADD:
            builder_emit(builder, ADD);
            break;
        case BINOP_SUB:
            builder_emit(builder, SUB);
            break;
        case BINOP_MUL:
            builder_emit(builder, MUL);
            break;
        case BINOP_DIV:
            builder_emit(builder, DIV);
            break;
        case BINOP_MOD:
            builder_emit(builder, MOD);
            break;
        case BINOP_AND:
            builder_emit(builder, AND);
            break;
        case BINOP_OR:
            builder_emit(builder, OR);
            break;
        case BINOP_XOR:
            builder_emit(builder, XOR);
            break;
        case BINOP_GREATER_THAN:
            builder_emit(builder, GREATER_THAN);
            break;
        case BINOP_GREATER_THAN_OR_EQU:
            builder_emit(builder, GREATER_THAN_OR_EQU);
            break;
        case BINOP_LESS_THAN:
            builder_emit(builder, LESS_THAN);
            break;
        case BINOP_LESS_THAN_OR_EQU:
            builder_emit(builder, LESS_THAN_OR_EQU);
            break;
        case BINOP_EQUALS:
            builder_emit(builder, EQUALS);
            break;
        case BINOP_NOT_EQUALS:
            builder_emit(builder, NOT_EQUALS);
            break;
        case BINOP_HALF_RANGE:
            builder_emit(builder, BUILD_RANGE_HALF);
            break;
        case BINOP_CLOSED_RANGE:
            builder_emit(builder, BUILD_RANGE_CLOSED);
            break;
        case BINOP_SHL:
            builder_emit(builder, SHL);
            break;
        case BINOP_SHR:
            builder_emit(builder, SHR);
            break;
        default:
            break;
    }
}

static void
compile_unary_expr(struct compiler_state *statep, struct proc_builder *builder, struct ast_node *node)
{
    struct unary_expr *expr = (struct unary_expr*)node;

    compile_expr(statep, builder, expr->expr);

    switch (expr->op) {
        case UNARYOP_LOGICAL_NOT:
            builder_emit(builder, LOGICAL_NOT);
            break;
        case UNARYOP_NEGATE:
            builder_emit(builder, NEGATE);
            break;
        case UNARYOP_NOT:
            builder_emit(builder, NOT);
            break;
    }
}

static void
compile_call_expr(struct compiler_state *statep, struct proc_builder *builder, struct ast_node *node)
{
    struct call_expr *call = (struct call_expr*)node;

    struct ast_node *arg;
    list_iter_t iter;
    list_get_iter(call->arguments, &iter);

    while (iter_next_elem(&iter, (void**)&arg)) {
        compile_expr(statep, builder, arg);
    }

    compile_expr(statep, builder, call->target);

    builder_emit_i32(builder, INVOKE, LIST_COUNT(call->arguments));
}

static void
compile_call_macro_expr(struct compiler_state *statep, struct proc_builder *builder, struct ast_node *node)
{
    struct call_macro_expr *call = (struct call_macro_expr*)node;
    struct ga_obj *args_obj = ga_tuple_new(LIST_COUNT(call->expr_list));

    struct ast_node *arg;

    int i = 0;
    list_iter_t iter;
    list_get_iter(call->expr_list, &iter);

    while (iter_next_elem(&iter, (void**)&arg)) {
        ga_tuple_init_elem(args_obj, i, ga_ast_node_new(arg, NULL));
        i++;
    }

    label_t macro_label = builder_reserve_label(builder);
    
    builder_emit_label(builder, JUMP_IF_COMPILED, macro_label);

    builder_emit_obj(builder, LOAD_CONST, add_constant(statep, args_obj));
    builder_emit_obj(builder, LOAD_CONST, add_constant(statep, ga_tokenstream_new(call->token_list)));

    compile_expr(statep, builder, call->target);

    builder_mark_label(builder, macro_label);
    
    builder_emit(builder, COMPILE_MACRO);
}

static void compile_func(struct compiler_state *statep, struct proc_builder *, struct ast_node *);

static void
compile_expr(struct compiler_state *statep, struct proc_builder *builder, struct ast_node *expr)
{
    switch (expr->type) {
        case AST_ASSIGN_EXPR:
            compile_assign(statep, builder, expr);
            break;
        case AST_BIN_EXPR:
            compile_binop(statep, builder, expr);
            break;
        case AST_UNARY_EXPR:
            compile_unary_expr(statep, builder, expr);
            break;
        case AST_CALL_EXPR:
            compile_call_expr(statep, builder, expr);
            break;
        case AST_CALL_MACRO_EXPR:
            compile_call_macro_expr(statep, builder, expr);
            break;
        case AST_DICT_EXPR:
            compile_dict(statep, builder, expr);
            break;
        case AST_FUNC_EXPR:
            compile_func(statep, builder, expr);
            break;
        case AST_TUPLE_EXPR:
            compile_tuple(statep, builder, expr);
            break;
        case AST_INDEX_ACCESS_EXPR:
            compile_index_access(statep, builder, expr);
            break;
        case AST_LIST_EXPR:
            compile_list(statep, builder, expr);
            break;
        case AST_MEMBER_ACCESS_EXPR:
            compile_member_access(statep, builder, expr);
            break;
        case AST_BOOL_TERM:
            compile_bool(statep, builder, expr);
            break;
        case AST_INTEGER_TERM:
            compile_integer(statep, builder, expr);
            break;
        case AST_STRING_TERM:
            compile_string(statep, builder, expr);
            break;
        case AST_SYMBOL_TERM:
            compile_symbol(statep, builder, expr);
            break;
        default:
            printf("I don't know how.\n");
            break;
    }
}

static void compile_stmt(struct compiler_state *statep, struct proc_builder *, struct ast_node*);

static void
compile_for_stmt(struct compiler_state *statep, struct proc_builder *builder, struct ast_node *root)
{
    struct for_stmt *stmt = (struct for_stmt*)root;
    
    label_t begin_label = builder_reserve_label(builder);
    label_t end_label = builder_reserve_label(builder);
    temporary_t iterator = builder_reserve_temporary(builder);
    
    builder_declare_var(builder, stmt->var_name);

    compile_expr(statep, builder, stmt->expr);
    builder_emit(builder, GET_ITER);
    builder_emit_i32(builder, STORE_FAST, iterator);
    builder_mark_label(builder, begin_label); 
    builder_emit_i32(builder, LOAD_FAST, iterator);
    builder_emit(builder, ITER_NEXT);
    builder_emit_label(builder, JUMP_IF_FALSE, end_label);
    builder_emit_i32(builder, LOAD_FAST, iterator);
    builder_emit(builder, ITER_CUR);
    builder_emit_store(statep, builder, stmt->var_name);

    builder_push_continue_label(builder, begin_label);
    builder_push_break_label(builder, end_label);

    compile_stmt(statep, builder, stmt->body); 

    builder_emit_label(builder, JUMP, begin_label);
    builder_mark_label(builder, end_label);
}

static void
compile_if_stmt(struct compiler_state *statep, struct proc_builder *builder, struct ast_node *root)
{
    struct if_stmt *stmt = (struct if_stmt*)root;

    label_t end_label = builder_reserve_label(builder);
    label_t else_label = builder_reserve_label(builder);

    compile_expr(statep, builder, stmt->cond);
    builder_emit_label(builder, JUMP_IF_FALSE, else_label);
    compile_stmt(statep, builder, stmt->if_body);
    builder_emit_label(builder, JUMP, end_label);
    builder_mark_label(builder, else_label);
    
    if (stmt->else_body) {
        compile_stmt(statep, builder, stmt->else_body);
    }

    builder_mark_label(builder, end_label);
}

static void
compile_return_stmt(struct compiler_state *statep, struct proc_builder *builder, struct ast_node *node)
{
    struct return_stmt *stmt = (struct return_stmt*)node;
    compile_expr(statep, builder, stmt->val);
    builder_emit(builder, RET);
}

static void
compile_try_stmt(struct compiler_state *statep, struct proc_builder *builder, struct ast_node *node)
{
    struct try_stmt *stmt = (struct try_stmt*)node;
    
    label_t except_label = builder_reserve_label(builder);
    label_t end_label = builder_reserve_label(builder);

    builder_emit_label(builder, PUSH_EXCEPTION_HANDLER, except_label);
    compile_stmt(statep, builder, stmt->try_body);
    builder_emit(builder, POP_EXCEPTION_HANDLER);
    builder_emit_label(builder, JUMP, end_label);
    builder_mark_label(builder, except_label);

    if (stmt->has_var) {
        builder_declare_var(builder, stmt->var_name);
        builder_emit_store(statep, builder, stmt->var_name);
    } else {
        builder_emit(builder, POP);
    }

    compile_stmt(statep, builder, stmt->except_body);
    builder_mark_label(builder, end_label);
}

static void
compile_while_stmt(struct compiler_state *statep, struct proc_builder *builder, struct ast_node *root)
{
    struct while_stmt *stmt = (struct while_stmt*)root;
    
    label_t begin_label = builder_reserve_label(builder);
    label_t end_label = builder_reserve_label(builder);

    builder_mark_label(builder, begin_label);
    compile_expr(statep, builder, stmt->cond);
    builder_emit_label(builder, JUMP_IF_FALSE, end_label);
    
    builder_push_break_label(builder, end_label);
    builder_push_continue_label(builder, begin_label);

    compile_stmt(statep, builder, stmt->body);

    builder_emit_label(builder, JUMP, begin_label);
    builder_mark_label(builder, end_label);
}

static void
compile_code_block(struct compiler_state *statep, struct proc_builder *builder, struct ast_node *root)
{
    struct code_block *block = (struct code_block*)root;
    struct ast_node *node;
    list_iter_t iter;
    list_get_iter(block->children, &iter);

    while (iter_next_elem(&iter, (void**)&node)) {
        compile_stmt(statep, builder, node);
    }
}

static void
compile_class_decl(struct compiler_state *statep, struct proc_builder *builder, struct ast_node *node)
{
    struct class_decl *decl = (struct class_decl*)node;
    struct func_decl *method;

    list_iter_t iter;
    list_get_iter(decl->methods, &iter);

    while (iter_next_elem(&iter, (void**)&method)) {
        compile_func(statep, builder, (struct ast_node*)method);
        builder_emit_obj(builder, LOAD_CONST, add_constant(statep, ga_str_from_cstring(method->name)));
    }

    builder_emit_i32(builder, BUILD_DICT, LIST_COUNT(decl->methods));

    if (decl->base) {
        compile_expr(statep, builder, decl->base);
    } else {
        builder_emit_name(statep, builder, LOAD_GLOBAL, "Object");
    }

    builder_emit_name(statep, builder, BUILD_CLASS, decl->name);
    builder_emit_name(statep, builder, STORE_GLOBAL, decl->name);
}

static void
compile_func(struct compiler_state *statep, struct proc_builder *builder, struct ast_node *node)
{
    struct func_decl *func = (struct func_decl*)node;
    struct proc_builder *func_proc = builder_new(builder);

    list_iter_t iter;
    list_get_iter(func->parameters, &iter);

    struct func_param *param;

    while (iter_next_elem(&iter, (void**)&param)) {
        builder_declare_var(func_proc, param->name);
        builder_emit_obj(builder, LOAD_CONST, add_constant(statep, ga_str_from_cstring(param->name)));
    }
    
    builder_emit_i32(builder, BUILD_TUPLE, LIST_COUNT(func->parameters));
    compile_stmt(statep, func_proc, func->body);
    builder_emit_obj(func_proc, LOAD_CONST, &ga_null_inst);
    builder_emit(func_proc, RET);
    
    if (!builder->parent) {
        builder_emit_ptr(builder, BUILD_FUNC, builder_finalize(func_proc));
    } else {
        builder_emit_ptr(builder, BUILD_CLOSURE, builder_finalize(func_proc));
    }
    
    //builder_destroy(func_proc);
}

static void
compile_stmt(struct compiler_state *statep, struct proc_builder *builder, struct ast_node *node)
{
    switch (node->type) {
        case AST_BREAK_STMT:
            builder_emit_label(builder, JUMP, builder_pop_break_label(builder));
            break;
        case AST_CONTINUE_STMT:
            builder_emit_label(builder, JUMP, builder_pop_continue_label(builder));
            break;
        case AST_FOR_STMT:
            compile_for_stmt(statep, builder, node);
            break;
        case AST_IF_STMT:
            compile_if_stmt(statep, builder, node);
            break;
        case AST_RETURN_STMT:
            compile_return_stmt(statep, builder, node);
            break;
        case AST_TRY_STMT:
            compile_try_stmt(statep, builder, node);
            break;
        case AST_WHILE_STMT:
            compile_while_stmt(statep, builder, node);
            break;
        case AST_CODE_BLOCK:
            compile_code_block(statep, builder, node);
            break;
        case AST_CLASS_DECL:
            compile_class_decl(statep, builder, node);
            break;
        case AST_FUNC_DECL: {
            struct func_decl *decl = (struct func_decl*)node;
            compile_func(statep, builder, node);
        
            if (builder->parent) {
                builder_declare_var(builder, decl->name);
                builder_emit_i32(builder, STORE_FAST, builder_get_local_slot(builder, decl->name));
            } else {
                builder_emit_name(statep, builder, STORE_GLOBAL, decl->name);
            }
            break;
        }
        case AST_EMPTY_STMT: 
            break;
        default:
            compile_expr(statep, builder, node);
            builder_emit(builder, POP);
            break;
    }
}

void
ga_proc_destroy(struct ga_proc *builder)
{
    builder_destroy(builder->compiler_private);
    free(builder);
}

struct ga_obj *
compiler_compile(struct compiler_state *statep, const char *src)
{
    struct ast_node *root = parser_parse(&statep->parse_state, src);

    if (!root) {
        statep->comp_errno = COMPILER_SYNTAX_ERROR;
        return NULL;
    }

    return compiler_compile_ast(statep, root);
}

struct ga_obj *
compiler_compile_ast(struct compiler_state *statep, struct ast_node *root)
{
    struct ga_mod_data *data = calloc(sizeof(struct ga_mod_data), 1);
    struct proc_builder *builder = builder_new(NULL);

    data->constants = list_new();
    data->procs = list_new();
    data->strings = list_new();

    statep->mod_data = data;
    
    compile_stmt(statep, builder, root);
    builder_emit_obj(builder, LOAD_CONST, &ga_null_inst);
    builder_emit(builder, RET);

    struct ga_proc *code = builder_finalize(builder);

    return ga_code_new(code, data);
}

struct ga_obj *
compiler_compile_inline(struct compiler_state *statep, struct ga_proc *parent_code, struct ast_node *root)
{
    struct ga_mod_data *data = calloc(sizeof(struct ga_mod_data), 1);
    struct proc_builder *builder = builder_new(parent_code->compiler_private);

    data->constants = list_new();
    data->procs = list_new();
    data->strings = list_new();

    statep->mod_data = data;

    compile_stmt(statep, builder, root);
    builder_emit_obj(builder, LOAD_CONST, &ga_null_inst);
    builder_emit(builder, RET);

    struct ga_proc *code = builder_finalize(builder);

    return ga_code_new(code, data);
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
