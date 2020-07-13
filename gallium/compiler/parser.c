#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <compiler/ast.h>
#include <compiler/lexer.h>
#include <compiler/parser.h>
#include <gallium/list.h>
#include <gallium/stringbuf.h>

static struct token *
peek_token(struct parser_state *statep)
{
    struct token *token;

    if (!iter_peek_elem(&statep->iter, (void**)&token)) {
        return NULL;
    }

    return token;
}

static struct token *
read_token(struct parser_state *statep)
{
    struct token *token;

    if (!iter_next_elem(&statep->iter, (void**)&token)) {
        return NULL;
    }

    statep->last_tok = token;

    return token;
}

static bool
match_token_class(struct parser_state *statep, token_class_t type)
{
    struct token *token = peek_token(statep);

    if (token) {
        return (token->type == type);
    }

    return false;
}

static bool
match_token_val(struct parser_state *statep, token_class_t type, const char *val)
{
    size_t val_len = strlen(val);
    struct token *token = peek_token(statep);

    if (token) {
        return (token->type == type && token->sb && val_len == STRINGBUF_LEN(token->sb) &&
                strcmp(STRINGBUF_VALUE(token->sb), val) == 0);
    }

    return false;
}

static bool
accept_token_class(struct parser_state *statep, token_class_t type)
{
    if (match_token_class(statep, type)) {
        read_token(statep);
        return true;
    }

    return false;
}

static bool
accept_token_val(struct parser_state *statep, token_class_t type, const char *val)
{
    if (match_token_val(statep, type, val)) {
        read_token(statep);
        return true;
    }

    return false;
}

void
parser_seterrno(struct parser_state *statep, int err, const char *info)
{
    statep->parser_errno = err;
    statep->err_info = info;
}

/* callback destroying a list of struct ast_node *s */
void
ast_list_destroy_cb(void *elem, void *statep)
{
    struct ast_node *node = elem;

    ast_destroy(node);
}

struct ast_node *   parser_parse_expr(struct parser_state *);

struct ast_node *
parse_term(struct parser_state *statep)
{
    if (accept_token_val(statep, TOK_KEYWORD, "true")) {
        return bool_term_new(true);
    }

    if (accept_token_val(statep, TOK_KEYWORD, "false")) {
        return bool_term_new(false);
    }
 
    struct token *tok = read_token(statep);

    if (!tok) {
        parser_seterrno(statep, PARSER_UNEXPECTED_EOF, NULL);
        return NULL;
    }

    if (tok->type == TOK_STRING_LIT) {
        return string_term_new(tok->sb);
    }

    if (tok->type == TOK_INT_LIT) {
        int64_t val = strtoll(STRINGBUF_VALUE(tok->sb), NULL, 10);
        
        if (errno != 0) {
            /* conversion failed... TODO: report error */
            return NULL;
        }
        return integer_term_new(val);
    }

    if (tok->type == TOK_IDENT) {
        return symbol_term_new(STRINGBUF_VALUE(tok->sb));
    }

    parser_seterrno(statep, PARSER_UNEXPECTED_TOK, STRINGBUF_VALUE(tok->sb));
     
    return NULL;
}

struct ast_node *
parse_quote(struct parser_state *statep)
{
    if (!accept_token_class(statep, TOK_BACKTICK)) {
        return parse_term(statep);
    }

    struct list *children = list_new();

    while (!match_token_class(statep, TOK_BACKTICK) && peek_token(statep) != NULL) {
        struct ast_node *node = parser_parse_decl(statep);

        if (!node) {
            goto error;
        }

        list_append(children, node);
    }

    if (!read_token(statep)) {
        parser_seterrno(statep, PARSER_EXPECTED_TOK, "`");
        goto error;
    }

    return quote_expr_new(children);
error:
    list_destroy(children, ast_list_destroy_cb, NULL);
    return NULL;
}

static struct ast_node * parse_func(struct parser_state *, bool);

struct ast_node *
parse_anonymous_func(struct parser_state *statep)
{
    if (match_token_val(statep, TOK_KEYWORD, "func")) {
        return parse_func(statep, true);
    }

	if (match_token_val(statep, TOK_KEYWORD, "macro")) {
        return parse_func(statep, true);
    }

    return parse_quote(statep);
}

struct ast_node *
parse_dict_expr(struct parser_state *statep)
{
    if (!accept_token_class(statep, TOK_OPEN_BRACE)) {
        return parse_anonymous_func(statep);
    }

    struct list *kvp_pairs = list_new();

    while (!match_token_class(statep, TOK_CLOSE_BRACE)) {
        struct ast_node *key = parser_parse_expr(statep);

        if (!key) goto error;

        if (!accept_token_class(statep, TOK_COLON)) {
            parser_seterrno(statep, PARSER_EXPECTED_TOK, ":");
            ast_destroy(key);
            goto error;
        }

        struct ast_node *val = parser_parse_expr(statep);

        if (!val) {
            ast_destroy(key);
            goto error;
        }

        list_append(kvp_pairs, key_val_expr_new(key, val)); 

        if (!accept_token_class(statep, TOK_COMMA)) {
            break;
        }
    }

    if (!accept_token_class(statep, TOK_CLOSE_BRACE)) {
        parser_seterrno(statep, PARSER_EXPECTED_TOK, "}");
        goto error;
    }

    return dict_expr_new(kvp_pairs);

error:
    list_destroy(kvp_pairs, ast_list_destroy_cb, NULL);
    return NULL;
}

struct ast_node *
parse_list_expr(struct parser_state *statep)
{
    if (!accept_token_class(statep, TOK_OPEN_BRACKET)) {
        return parse_dict_expr(statep);
    }

    struct list *items = list_new();

    while (!match_token_class(statep, TOK_CLOSE_BRACKET))  {
        struct ast_node *item = parser_parse_expr(statep);
        
        if (!item) goto error;

        list_append_front(items, item);

        if (!accept_token_class(statep, TOK_COMMA)) {
            break;
        }
    }

    if (!accept_token_class(statep, TOK_CLOSE_BRACKET)) {
        parser_seterrno(statep, PARSER_EXPECTED_TOK, "]");
        goto error;
    }

    return list_expr_new(items);

error:
    list_destroy(items, ast_list_destroy_cb, NULL);
    return NULL;
}

struct ast_node *
parse_tuple(struct parser_state *statep, struct ast_node *first_item)
{
    struct list *items = list_new();

    list_append(items, first_item);

    for (;;) {
        struct ast_node *expr = parser_parse_expr(statep);

        if (!expr) goto error;

        list_append(items, expr);

        if (!accept_token_class(statep, TOK_COMMA)) {
            break;
        }
    }

    if (!accept_token_class(statep, TOK_RIGHT_PAREN)) {
        parser_seterrno(statep, PARSER_EXPECTED_TOK, ")");
        goto error;
    }

    return tuple_expr_new(items);

error:
    list_destroy(items, ast_list_destroy_cb, NULL);
    return NULL;
}

struct ast_node *
parse_grouping(struct parser_state *statep)
{
    struct ast_node *expr = NULL;

    if (accept_token_class(statep, TOK_LEFT_PAREN)) {
        expr = parser_parse_expr(statep);

        if (!expr) {
            parser_seterrno(statep, PARSER_EXPECTED_EXPR, NULL);
            goto error;
        }

        if (accept_token_class(statep, TOK_COMMA)) {
            return parse_tuple(statep, expr);
        }

        if (!accept_token_class(statep, TOK_RIGHT_PAREN)) {
            parser_seterrno(statep, PARSER_EXPECTED_TOK, ")");
            goto error;
        }

        return expr;
    }

    return parse_list_expr(statep);

error:
    if (expr) ast_destroy(expr);
    return NULL;
}

struct ast_node *
parse_index_expr(struct parser_state *statep)
{
    struct ast_node *left = parse_grouping(statep);
    struct ast_node *key = NULL;
    if (!left) return NULL;

    if (accept_token_class(statep, TOK_OPEN_BRACKET)) {
        key = parser_parse_expr(statep);

        if (!key) {
            goto error;
        }

        if (!accept_token_class(statep, TOK_CLOSE_BRACKET)) {
            parser_seterrno(statep, PARSER_EXPECTED_TOK, "]");
            goto error;
        }

        return index_access_expr_new(left, key);
    }

    return left;

error:
    if (left) ast_destroy(left);
    if (key) ast_destroy(key);

    return NULL;
}

struct ast_node *
parse_member_access(struct ast_node *left, struct parser_state *statep)
{
    accept_token_class(statep, TOK_DOT);

    if (!match_token_class(statep, TOK_IDENT)) {
        ast_destroy(left);
        return NULL;
    }

    struct token *tok = read_token(statep);
    
    return member_access_expr_new(left, STRINGBUF_VALUE(tok->sb));
}

static struct list *
parse_arglist(struct parser_state *statep)
{
	struct list *args = list_new();

    read_token(statep);

	while (!match_token_class(statep, TOK_RIGHT_PAREN)) {
		struct ast_node *expr = parser_parse_expr(statep);

		if (!expr) {
			parser_seterrno(statep, PARSER_EXPECTED_EXPR, NULL);
			goto error;
		}

		list_append(args, expr);

		if (!match_token_class(statep, TOK_COMMA)) {
			break;
		}

		read_token(statep);
	}

	if (!accept_token_class(statep, TOK_RIGHT_PAREN)) {
		parser_seterrno(statep, PARSER_EXPECTED_TOK, ")");
		goto error;
	}

    return args;

error:
    if (args) list_destroy(args, ast_list_destroy_cb, NULL);
    return NULL;
}

struct ast_node *
parse_call_macro_expr(struct ast_node *left, struct list *expr_list, struct parser_state *statep)
{
    if (!match_token_class(statep, TOK_OPEN_BRACE)) {
        return call_macro_expr_new(left, expr_list, list_new());
    }

    struct list *token_list = list_new();

    int indent_level = -1;
    struct token *tokens_start = read_token(statep);
    
    while (peek_token(statep)) {
        struct token *tok = read_token(statep);
        
        if (indent_level == -1 && tok->row != tokens_start->row) {
            indent_level = tok->col;
        }

        if (tok->type == TOK_CLOSE_BRACE && tok->col < indent_level) {
            break;
        }
        
        struct token *tok_copy = calloc(sizeof(struct token), 1);
        memcpy(tok_copy, tok, sizeof(struct token));
        list_append(token_list, tok_copy);
    }

    return call_macro_expr_new(left, expr_list, token_list);
}

struct ast_node *
parse_call_expr(struct ast_node *left, bool chained, struct parser_state *statep)
{
    if (!left) return NULL;

    bool is_macro = accept_token_class(statep, TOK_BACKTICK);

    struct ast_node *ret = NULL;

    if (match_token_class(statep, TOK_LEFT_PAREN)) {
        struct list *params = parse_arglist(statep);

        if (!params) return NULL;
       
        if (is_macro) {
            ret = parse_call_macro_expr(parse_call_expr(left, true, statep), params, statep);
        } else {
            //return call_expr_new(parse_call_expr(left, statep), params);
            ret = parse_call_expr(call_expr_new(left, params), true, statep);
        }
    } else if (match_token_class(statep, TOK_DOT)) {
        ret = parse_call_expr(parse_member_access(left, statep), true, statep);
    } else if (is_macro) {
        ret = parse_call_macro_expr(parse_call_expr(left, true, statep), list_new(), statep);
    }

    if (ret) {
        return ret;
    }

    return left;
}

struct ast_node *
parse_unary(struct parser_state *statep)
{
    unaryop_t op;

    if (match_token_class(statep, TOK_LOGICAL_NOT)) {
        op = UNARYOP_LOGICAL_NOT;    
    } else if (match_token_class(statep, TOK_NOT)) {
        op = UNARYOP_NOT;
    } else if (match_token_class(statep, TOK_SUB)) {
        op = UNARYOP_NEGATE;
    } else {
        return parse_call_expr(parse_index_expr(statep), false, statep);
    }

    read_token(statep);

    struct ast_node *expr = parse_call_expr(parse_index_expr(statep), false, statep);

    if (!expr) {
        return NULL;
    }

    return unary_expr_new(op, expr);
}

struct ast_node *
parse_div_mul(struct parser_state *statep)
{
    struct ast_node *left = parse_unary(statep);

    while (match_token_class(statep, TOK_DIV) || match_token_class(statep, TOK_MUL) ||
            match_token_class(statep, TOK_MOD))
    {
        struct token *tok = read_token(statep);
        struct ast_node *right = parse_unary(statep);
        binop_t op;

        if (!right) {
            ast_destroy(left);
            return NULL;
        }

        switch (tok->type) {
            case TOK_DIV:
                op = BINOP_DIV;
                break;
            case TOK_MUL:
                op = BINOP_MUL;
                break;
            case TOK_MOD:
                op = BINOP_MOD;
                break;
            default:
                break;

        }

        left = bin_expr_new(op, left, right);
    }

    return left; 
}

struct ast_node *
parse_add_sub(struct parser_state *statep)
{
    struct ast_node *left = parse_div_mul(statep);

    while (match_token_class(statep, TOK_ADD) || match_token_class(statep, TOK_SUB)) {
        struct token *tok = read_token(statep);
        struct ast_node *right = parse_div_mul(statep);
        binop_t op;

        if (!right) {
            ast_destroy(left);
            return NULL;
        }

        switch (tok->type) {
            case TOK_ADD:
                op = BINOP_ADD;
                break;
            case TOK_SUB:
                op = BINOP_SUB;
                break;
            default:
                break;

        }

        left = bin_expr_new(op, left, right);
    }

    return left;
}

struct ast_node *
parse_bitshift(struct parser_state *statep)
{
    struct ast_node *left = parse_add_sub(statep);

    while (match_token_class(statep, TOK_SHL) || match_token_class(statep, TOK_SHR)) {
        struct token *tok = read_token(statep);
        struct ast_node *right = parse_add_sub(statep);
        binop_t op;

        if (!right) {
            ast_destroy(left);
            return NULL;
        }

        switch (tok->type) {
            case TOK_SHL:
                op = BINOP_SHL;
                break;
            case TOK_SHR:
                op = BINOP_SHR;
                break;
            default:
                break;

        }

        left = bin_expr_new(op, left, right);
    }

    return left;
}

struct ast_node *
parse_relational(struct parser_state *statep)
{
    struct ast_node *left = parse_bitshift(statep);

    while ( match_token_class(statep, TOK_GREATER_THAN) || match_token_class(statep, TOK_LESS_THAN) ||
            match_token_class(statep, TOK_GREATER_THAN_OR_EQU) || match_token_class(statep, TOK_LESS_THAN_OR_EQU))
    {
        struct token *tok = read_token(statep);
        struct ast_node *right = parse_bitshift(statep);
        binop_t op;

        if (!right) {
            return NULL;
        }

        switch (tok->type) {
            case TOK_GREATER_THAN:
                op = BINOP_GREATER_THAN;
                break;
            case TOK_LESS_THAN:
                op = BINOP_LESS_THAN;
                break;
            case TOK_GREATER_THAN_OR_EQU:
                op = BINOP_GREATER_THAN_OR_EQU;
                break;
            case TOK_LESS_THAN_OR_EQU:
                op = BINOP_LESS_THAN_OR_EQU;
                break;
            default:
                break;

        }

        left = bin_expr_new(op, left, right);
    }

    return left;
}

struct ast_node *
parse_equality(struct parser_state *statep)
{
    struct ast_node *left = parse_relational(statep);

    if (!left) return NULL;

    while (match_token_class(statep, TOK_EQUALS) || match_token_class(statep, TOK_NOT_EQUALS)) {
        struct token *tok = read_token(statep);
        struct ast_node *right = parse_relational(statep);
        binop_t op;

        if (!right) {
            ast_destroy(left);
            return NULL;
        }

        switch (tok->type) {
            case TOK_EQUALS:
                op = BINOP_EQUALS;
                break;
            case TOK_NOT_EQUALS:
                op = BINOP_NOT_EQUALS;
                break;
            default:
                break;

        }

        left = bin_expr_new(op, left, right);
    }

    return left;
}

struct ast_node *
parse_and(struct parser_state *statep)
{
    struct ast_node *left = parse_equality(statep);

    if (!left) return NULL;

    while (accept_token_class(statep, TOK_AND)) {
        struct ast_node *right = parse_equality(statep);

        if (!right) {
            ast_destroy(left);
            return NULL; 
        }

        left = bin_expr_new(BINOP_AND, left, right);
    }

    return left;
}

struct ast_node *
parse_xor(struct parser_state *statep)
{
    struct ast_node *left = parse_and(statep);

    if (!left) return NULL;

    while (accept_token_class(statep, TOK_XOR)) {
        struct ast_node *right = parse_and(statep);

        if (!right) {
            ast_destroy(left);
            return NULL;
        }

        left = bin_expr_new(BINOP_XOR, left, right);
    }

    return left;
}

struct ast_node *
parse_or(struct parser_state *statep)
{
    struct ast_node *left = parse_xor(statep);

    if (!left) return NULL;

    while (accept_token_class(statep, TOK_OR)) {
        struct ast_node *right = parse_xor(statep);

        if (!right) {
            ast_destroy(left);
            return NULL;
        }

        left = bin_expr_new(BINOP_OR, left, right);
    }

    return left;
}

struct ast_node *
parse_logical_and(struct parser_state *statep)
{
    struct ast_node *left = parse_or(statep);

    if (!left) return NULL;

    while (accept_token_class(statep, TOK_LOGICAL_AND)) {
        struct ast_node *right = parse_or(statep);

        if (!right) {
            ast_destroy(left);
            return NULL;
        }

        left = bin_expr_new(BINOP_LOGICAL_AND, left, right);
    }

    return left;
}

struct ast_node *
parse_logical_or(struct parser_state *statep)
{
    struct ast_node *left = parse_logical_and(statep);

    if (!left) return NULL;

    while (accept_token_class(statep, TOK_LOGICAL_OR)) {
        struct ast_node *right = parse_logical_and(statep);

        if (!right) {
            ast_destroy(left);
            return NULL;
        }

        left = bin_expr_new(BINOP_LOGICAL_OR, left, right);
    }

    return left;
}

struct ast_node *
parse_range(struct parser_state *statep)
{
    struct ast_node *left = parse_logical_or(statep);

    if (!left) return NULL;

    while (match_token_class(statep, TOK_HALF_RANGE)
            || match_token_class(statep, TOK_CLOSED_RANGE))
    {
        struct token *tok = read_token(statep);
        struct ast_node *right = parse_logical_or(statep);

        if (!right) {
            ast_destroy(left);
        }

        switch (tok->type) {
            case TOK_HALF_RANGE:
                left = bin_expr_new(BINOP_HALF_RANGE, left, right);
                break;
            case TOK_CLOSED_RANGE:
                left = bin_expr_new(BINOP_CLOSED_RANGE, left, right);
                break;
            default:
                break;
        }
    }

    return left;
}

struct ast_node *
parse_assign(struct parser_state *statep)
{
    struct ast_node *left = parse_range(statep);

    if (!left) return NULL;

    if (accept_token_class(statep, TOK_ASSIGN)) {
        struct ast_node *right = parse_assign(statep);
        
        if (!right) {
            ast_destroy(left);
            return NULL;
        }

        return assign_expr_new(left, right);
    }

    return left;
}

struct ast_node *
parser_parse_expr(struct parser_state *statep)
{
    return parse_assign(statep);
}

struct ast_node *   parser_parse_decl(struct parser_state *);
struct ast_node *   parser_parse_stmt(struct parser_state *);

struct ast_node *
parse_code_block(struct parser_state *statep)
{
    read_token(statep);

    struct list *children = list_new();

    while (!match_token_class(statep, TOK_CLOSE_BRACE) && peek_token(statep) != NULL) {
        struct ast_node *node = parser_parse_decl(statep);

        if (!node) {
            goto error;
        }
        
        list_append(children, node);
    }

    if (!read_token(statep)) {
        parser_seterrno(statep, PARSER_EXPECTED_TOK, "}");
        goto error;
    }

    return code_block_new(children);
error:
    list_destroy(children, ast_list_destroy_cb, NULL);
    return NULL;
}

struct ast_node *
parse_for(struct parser_state *statep)
{
    read_token(statep);

    if (!match_token_class(statep, TOK_IDENT)) {
        parser_seterrno(statep, PARSER_EXPECTED_TOK, "<ident>");
        return NULL;
    }

    struct token *tok = read_token(statep);

    if (!accept_token_val(statep, TOK_KEYWORD, "in")) {
        parser_seterrno(statep, PARSER_EXPECTED_TOK, "in");
        return NULL;
    }

    struct ast_node *expr = parser_parse_expr(statep);

    if (!expr) {
        return NULL;
    }

    struct ast_node *body = parser_parse_stmt(statep);

    if (!body) {
        ast_destroy(expr);
        return NULL;
    }

    return for_stmt_new(STRINGBUF_VALUE(tok->sb), expr, body);
}

struct ast_node *
parse_if(struct parser_state *statep)
{
    read_token(statep);

    struct ast_node *cond = parser_parse_expr(statep);

    if (!cond) {
        return NULL;
    }

    struct ast_node *body = parser_parse_stmt(statep);
    struct ast_node *else_body = NULL;

    if (!body) {
        goto error;
    }

    if (match_token_val(statep, TOK_KEYWORD, "else")) {
        read_token(statep);
        else_body = parser_parse_stmt(statep);

        if (!else_body) {
            goto error;
        }
    }

    return if_stmt_new(cond, body, else_body);
    
error:
    if (cond) ast_destroy(cond);
    if (body) ast_destroy(body);
    if (else_body) ast_destroy(else_body);
    return NULL;
}

struct ast_node *
parse_return(struct parser_state *statep)
{
    read_token(statep);

    struct ast_node *val = parser_parse_expr(statep);

    if (!val) {
        return NULL;
    }

    return return_stmt_new(val);
}

struct ast_node *
parse_try(struct parser_state *statep)
{
    read_token(statep);

    struct ast_node *try_body = parser_parse_stmt(statep);

    if (!try_body) {
        goto error;
    }

    if (!accept_token_val(statep, TOK_KEYWORD, "except")) {
        parser_seterrno(statep, PARSER_EXPECTED_TOK, "except");
        goto error;
    }

    const char *varname = NULL;

    if (match_token_class(statep, TOK_IDENT)) {
        struct token *tok = read_token(statep);
        varname = STRINGBUF_VALUE(tok->sb);
    }

    struct ast_node *except_body = parser_parse_stmt(statep);

    if (!except_body) {
        goto error;
    }

    return try_stmt_new(try_body, except_body, varname);

error:
    if (try_body) ast_destroy(try_body);
    if (except_body) ast_destroy(except_body);

    return NULL;
}

struct ast_node *
parse_while(struct parser_state *statep)
{
    read_token(statep);

    struct ast_node *cond = parser_parse_expr(statep);

    if (!cond) {
        return NULL;
    }

    struct ast_node *body = parser_parse_stmt(statep);
    
    if (!body) {
        ast_destroy(cond);
        return NULL;
    }
    
    return while_stmt_new(cond, body);
}

struct ast_node *
parser_parse_stmt(struct parser_state *statep)
{
    if (match_token_class(statep, TOK_SEMICOLON)) {
        while (accept_token_class(statep, TOK_SEMICOLON));

        return &ast_empty_stmt_inst;
    }

    if (match_token_val(statep, TOK_KEYWORD, "if")) {
        return parse_if(statep);
    }

    if (match_token_val(statep, TOK_KEYWORD, "return")) {
        return parse_return(statep);
    }

    if (match_token_val(statep, TOK_KEYWORD, "try")) {
        return parse_try(statep);
    }

    if (match_token_val(statep, TOK_KEYWORD, "while")) {
        return parse_while(statep);
    }

    if (match_token_val(statep, TOK_KEYWORD, "for")) {
        return parse_for(statep);
    }

    if (accept_token_val(statep, TOK_KEYWORD, "break")) {
        return break_stmt_new();
    }

    if (accept_token_val(statep, TOK_KEYWORD, "continue")) {
        return continue_stmt_new();
    }

    if (match_token_class(statep, TOK_OPEN_BRACE)) {
        return parse_code_block(statep);        
    }

    return parser_parse_expr(statep);
}

struct ast_node *parser_parse_decl(struct parser_state *statep);

struct ast_node *
parse_class(struct parser_state *statep)
{
    read_token(statep);

    if (!match_token_class(statep, TOK_IDENT)) {
        /* set the right error */
        return NULL;
    }

    struct token *class_name = read_token(statep);
    struct ast_node *base = NULL;

    if (accept_token_val(statep, TOK_KEYWORD, "extends")) {
        base = parser_parse_expr(statep);

        if (!base) {
            return NULL;
        }
    }

    if (!accept_token_class(statep, TOK_OPEN_BRACE)) {
        parser_seterrno(statep, PARSER_EXPECTED_TOK, "{");
        return NULL;
    }

    struct list *methods = list_new();

    while (!accept_token_class(statep, TOK_CLOSE_BRACE)) {
        struct ast_node *method = parser_parse_decl(statep);

        if (!method) {
            goto error;
        }

        list_append(methods, method);
    }
   
    return class_decl_new(STRINGBUF_VALUE(class_name->sb), base, methods);

error:
    if (base) ast_destroy(base);
    list_destroy(methods, ast_list_destroy_cb, NULL);
    return NULL;
}

struct ast_node *
parse_func(struct parser_state *statep, bool is_expr)
{
    read_token(statep);

    const char *func_name = NULL;

    if (!is_expr) {
        if (!match_token_class(statep, TOK_IDENT)) {
            /* TODO: set error*/
            return NULL;
        }

        struct token *tok = read_token(statep);
        
        func_name = STRINGBUF_VALUE(tok->sb);
    }

    if (!match_token_class(statep, TOK_LEFT_PAREN)) {
        statep->parser_errno = PARSER_EXPECTED_TOK;
        statep->err_info = "(";
        return NULL;
    }
    
    read_token(statep);

    struct list *params = list_new();
    struct list *statements = list_new();

    while (!match_token_class(statep, TOK_RIGHT_PAREN)) {
        if (!match_token_class(statep, TOK_IDENT)) {
            goto error;
        }

        struct token *func_param = read_token(statep);

        list_append(params, func_param_new(STRINGBUF_VALUE(func_param->sb)));

        if (!match_token_class(statep, TOK_COMMA)) {
            break;
        }

        read_token(statep);
    }

    if (!match_token_class(statep, TOK_RIGHT_PAREN)) {
        goto error;
    }

    read_token(statep);

    if (accept_token_class(statep, TOK_OPEN_BRACE)) {
        while (!match_token_class(statep, TOK_CLOSE_BRACE) && peek_token(statep) != NULL) {
            struct ast_node *node = parser_parse_decl(statep);

            if (!node) goto error;

            list_append(statements, node);
        }

        if (!read_token(statep)) {
            parser_seterrno(statep, PARSER_EXPECTED_TOK, "}");
            goto error;
        }
    } else if (accept_token_class(statep, TOK_PHAT_ARROW)) {
        struct ast_node *expr_body = parser_parse_expr(statep);

        if (!expr_body) goto error;

        list_append(statements, return_stmt_new(expr_body));
    }

    if (is_expr) {
        return func_expr_new(params, code_block_new(statements));
    }

    return func_decl_new(func_name, params, code_block_new(statements));

error:
    list_destroy(params, ast_list_destroy_cb, NULL);
    list_destroy(statements, ast_list_destroy_cb, NULL);
    return NULL;
}

/*
struct ast_node *
parse_macro(struct parser_state *statep)
{
    read_token(statep);

    if (!match_token_class(statep, TOK_IDENT)) {
        parser_seterrno(statep, PARSER_EXPECTED_TOK, "<ident>"); 
        return NULL;
    }

    const char *macro_name = STRINGBUF_VALUE(read_token(statep)->sb);

    if (!accept_token_class(statep, TOK_LEFT_PAREN)) {
        parser_seterrno(statep, PARSER_EXPECTED_TOK, "(");
        return NULL;
    }

    if (!match_token_class(statep, TOK_IDENT)) {
        parser_seterrno(statep, PARSER_EXPECTED_TOK, "<ident>");
        return NULL;
    }

    const char *param_name = STRINGBUF_VALUE(read_token(statep)->sb);

    if (!accept_token_class(statep, TOK_RIGHT_PAREN)) {
        parser_seterrno(statep, PARSER_EXPECTED_TOK, ")");
        return NULL;
    }
   
    struct list *statements = list_new();

    if (accept_token_class(statep, TOK_OPEN_BRACE)) {
        while (!match_token_class(statep, TOK_CLOSE_BRACE) && peek_token(statep) != NULL) {
            struct ast_node *node = parser_parse_decl(statep);

            if (!node) goto error;

            list_append(statements, node);
        }

        if (!read_token(statep)) {
            parser_seterrno(statep, PARSER_EXPECTED_TOK, "}");
            goto error;
        }
    } else if (accept_token_class(statep, TOK_PHAT_ARROW)) {
        struct ast_node *expr_body = parser_parse_expr(statep);

        if (!expr_body) goto error;

        list_append(statements, return_stmt_new(expr_body));
    }

    return macro_decl_new(macro_name, param_name, code_block_new(statements));

error:
    list_destroy(statements, ast_list_destroy_cb, NULL);
    return NULL;
}*/

struct ast_node *
parser_parse_decl(struct parser_state  *statep)
{
    if (match_token_val(statep, TOK_KEYWORD, "macro")) {
        return parse_func(statep, false);
    }

    if (match_token_val(statep, TOK_KEYWORD, "func")) {
        return parse_func(statep, false);
    }

    if (match_token_val(statep, TOK_KEYWORD, "class")) {
        return parse_class(statep);
    }

    return parser_parse_stmt(statep);
}

static void
explain_lexer_error(struct parser_state *statep)
{
    fprintf(stderr, "line: %d,%d: ", statep->lex_state.row, statep->lex_state.col);

    switch (statep->lex_state.lex_errno) {
        case LEXER_UNTERMINATED_STR:
            fputs("unterminated string literal", stderr);
            break;
        case LEXER_UKN_CHAR:
            fputs("encountered unexpected character", stderr);
            break;
        default:
            fputs("unknown error countered during lexical analysis", stderr);
            break;
    }
    
    fputs("\n", stderr);
}

void
parser_explain(struct parser_state *statep)
{
    if (statep->parser_errno == PARSER_LEXER_ERR) {
        explain_lexer_error(statep);
        return;
    }

    struct token *tok = statep->last_tok;

    if (tok) {
        fprintf(stderr, "line: %d,%d: ", tok->row, tok->col);
    }

    switch (statep->parser_errno) {
        case PARSER_EXPECTED_TOK:
            fprintf(stderr, "expected '%s'", statep->err_info);
            break;
        case PARSER_UNEXPECTED_TOK:
            fprintf(stderr, "unexpected '%s'", statep->err_info);
            break;
        case PARSER_EXPECTED_EXPR:
            fputs("an expression was expected", stderr);
            break;
        case PARSER_UNEXPECTED_EOF:
            fputs("unexpected end of file", stderr);
            break;
        default:
            fputs("unknown error during semantic analysis", stderr);
            break;
    }
    
    fputs("\n", stderr);
}

void
parser_init_lazy(struct parser_state *statep, struct list *tokens)
{
    memset(statep, 0, sizeof(struct parser_state));

    list_get_iter(tokens, &statep->iter);
}

struct ast_node *
parser_parse(struct parser_state *statep, const char *src)
{
    memset(statep, 0, sizeof(struct parser_state));

    lexer_init(&statep->lex_state);
    lexer_scan(&statep->lex_state, src);

    if (statep->lex_state.lex_errno != LEXER_EOF) {
        statep->parser_errno = PARSER_LEXER_ERR;
        return NULL;
    }

    list_get_iter(statep->lex_state.tokens, &statep->iter);
    
    return parser_parse_all(statep);
}

struct ast_node *
parser_parse_all(struct parser_state *statep)
{
    struct list *nodes = list_new();

    while (peek_token(statep)) {
        struct ast_node *node = parser_parse_decl(statep);

        if (!node) {
            list_destroy(nodes, ast_list_destroy_cb, NULL);
            return NULL;
        }

        list_append(nodes, node);
    }

    return code_block_new(nodes);
}

void
parser_fini(struct parser_state *statep)
{
    lexer_fini(&statep->lex_state);
}
