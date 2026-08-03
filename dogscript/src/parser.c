#include "parser.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void parser_advance(Parser *p) {
    if (p->had_previous) {
        token_free(&p->previous);
    }
    p->previous = p->current;
    p->had_previous = 1;
    p->current = lexer_next(&p->lexer);
}

static int parser_check(Parser *p, TokenType type) {
    return p->current.type == type;
}

static int parser_match(Parser *p, TokenType type) {
    if (!parser_check(p, type)) {
        return 0;
    }
    parser_advance(p);
    return 1;
}

static void parser_expect(Parser *p, TokenType type, const char *msg) {
    if (!parser_check(p, type)) {
        ds_fatal("Parse error at %d:%d: expected %s (%s), got %s",
                 p->current.line, p->current.column, msg, token_type_name(type),
                 token_type_name(p->current.type));
    }
    parser_advance(p);
}

static TokenType parser_peek_next_type(Parser *p) {
    Lexer temp = p->lexer;
    Token tk = lexer_next(&temp);
    TokenType t = tk.type;
    token_free(&tk);
    return t;
}

static Expr *new_expr(ExprKind kind, int line, int col) {
    Expr *e = (Expr *)calloc(1, sizeof(Expr));
    if (!e) {
        ds_fatal("Out of memory allocating expression");
    }
    e->kind = kind;
    e->line = line;
    e->column = col;
    return e;
}

static Stmt *new_stmt(StmtKind kind, int line, int col) {
    Stmt *s = (Stmt *)calloc(1, sizeof(Stmt));
    if (!s) {
        ds_fatal("Out of memory allocating statement");
    }
    s->kind = kind;
    s->line = line;
    s->column = col;
    return s;
}

static void push_stmt(Stmt ***arr, int *count, Stmt *s) {
    *arr = (Stmt **)realloc(*arr, sizeof(Stmt *) * (size_t)(*count + 1));
    (*arr)[*count] = s;
    (*count)++;
}

static void push_expr(Expr ***arr, int *count, Expr *e) {
    *arr = (Expr **)realloc(*arr, sizeof(Expr *) * (size_t)(*count + 1));
    (*arr)[*count] = e;
    (*count)++;
}

static char *consume_identifier(Parser *p, const char *what) {
    char *name;
    if (!parser_check(p, TOK_IDENTIFIER)) {
        ds_fatal("Parse error at %d:%d: expected %s identifier",
                 p->current.line, p->current.column, what);
    }
    name = ds_strdup(p->current.lexeme);
    parser_advance(p);
    return name;
}

static Expr *parse_expression(Parser *p);
static Stmt *parse_statement(Parser *p);

static Expr *parse_primary(Parser *p) {
    Expr *e;
    int line = p->current.line;
    int col = p->current.column;
    if (parser_match(p, TOK_INT)) {
        e = new_expr(EXPR_INT, line, col);
        e->as.literal = ds_strdup(p->previous.lexeme);
        return e;
    }
    if (parser_match(p, TOK_FLOAT)) {
        e = new_expr(EXPR_FLOAT, line, col);
        e->as.literal = ds_strdup(p->previous.lexeme);
        return e;
    }
    if (parser_match(p, TOK_STRING)) {
        e = new_expr(EXPR_STRING, line, col);
        e->as.literal = ds_strdup(p->previous.lexeme);
        return e;
    }
    if (parser_match(p, TOK_TRUE)) {
        e = new_expr(EXPR_BOOL, line, col);
        e->as.literal = ds_strdup("true");
        return e;
    }
    if (parser_match(p, TOK_FALSE)) {
        e = new_expr(EXPR_BOOL, line, col);
        e->as.literal = ds_strdup("false");
        return e;
    }
    if (parser_match(p, TOK_NULL)) {
        e = new_expr(EXPR_NULL, line, col);
        e->as.literal = ds_strdup("null");
        return e;
    }
    if (parser_match(p, TOK_IDENTIFIER)) {
        char *name = ds_strdup(p->previous.lexeme);
        while (parser_match(p, TOK_DOT)) {
            char *part = consume_identifier(p, "member");
            char *joined = ds_join3(name, ".", part);
            free(name);
            free(part);
            name = joined;
        }
        e = new_expr(EXPR_IDENT, line, col);
        e->as.ident = name;
        return e;
    }
    if (parser_match(p, TOK_LPAREN)) {
        Expr *inner = parse_expression(p);
        parser_expect(p, TOK_RPAREN, "')'");
        return inner;
    }
    if (parser_match(p, TOK_LBRACKET)) {
        e = new_expr(EXPR_LIST, line, col);
        e->as.list.items = NULL;
        e->as.list.item_count = 0;
        if (!parser_check(p, TOK_RBRACKET)) {
            do {
                Expr *item = parse_expression(p);
                push_expr(&e->as.list.items, &e->as.list.item_count, item);
            } while (parser_match(p, TOK_COMMA));
        }
        parser_expect(p, TOK_RBRACKET, "']'");
        return e;
    }
    ds_fatal("Parse error at %d:%d: expected expression", p->current.line, p->current.column);
    return NULL;
}

static Expr *parse_call(Parser *p) {
    Expr *expr = parse_primary(p);
    while (parser_check(p, TOK_LPAREN) || parser_check(p, TOK_LBRACKET)) {
        if (parser_match(p, TOK_LPAREN)) {
            Expr *call = new_expr(EXPR_CALL, expr->line, expr->column);
            call->as.call.callee = expr;
            call->as.call.args = NULL;
            call->as.call.arg_count = 0;
            if (!parser_check(p, TOK_RPAREN)) {
                do {
                    Expr *arg = parse_expression(p);
                    push_expr(&call->as.call.args, &call->as.call.arg_count, arg);
                } while (parser_match(p, TOK_COMMA));
            }
            parser_expect(p, TOK_RPAREN, "')'");
            expr = call;
        } else {
            Expr *idx = new_expr(EXPR_INDEX, expr->line, expr->column);
            parser_advance(p); /* consume '[' */
            idx->as.index.object = expr;
            idx->as.index.index = parse_expression(p);
            parser_expect(p, TOK_RBRACKET, "']'");
            expr = idx;
        }
    }
    return expr;
}

static Expr *parse_factor(Parser *p) {
    Expr *left = parse_call(p);
    while (parser_check(p, TOK_STAR) || parser_check(p, TOK_SLASH)) {
        Token op = p->current;
        Expr *bin;
        parser_advance(p);
        bin = new_expr(EXPR_BINARY, op.line, op.column);
        bin->as.binary.op = ds_strdup(op.lexeme);
        bin->as.binary.left = left;
        bin->as.binary.right = parse_call(p);
        left = bin;
    }
    return left;
}

static Expr *parse_term(Parser *p) {
    Expr *left = parse_factor(p);
    while (parser_check(p, TOK_PLUS) || parser_check(p, TOK_MINUS)) {
        Token op = p->current;
        Expr *bin;
        parser_advance(p);
        bin = new_expr(EXPR_BINARY, op.line, op.column);
        bin->as.binary.op = ds_strdup(op.lexeme);
        bin->as.binary.left = left;
        bin->as.binary.right = parse_factor(p);
        left = bin;
    }
    return left;
}

static Expr *parse_compare(Parser *p) {
    Expr *left = parse_term(p);
    while (parser_check(p, TOK_LT) || parser_check(p, TOK_GT) ||
           parser_check(p, TOK_LTE) || parser_check(p, TOK_GTE)) {
        Token op = p->current;
        Expr *bin;
        parser_advance(p);
        bin = new_expr(EXPR_BINARY, op.line, op.column);
        bin->as.binary.op = ds_strdup(op.lexeme);
        bin->as.binary.left = left;
        bin->as.binary.right = parse_term(p);
        left = bin;
    }
    return left;
}

static Expr *parse_equality(Parser *p) {
    Expr *left = parse_compare(p);
    while (parser_check(p, TOK_EQ) || parser_check(p, TOK_NEQ) || parser_check(p, TOK_ASSIGN)) {
        Token op = p->current;
        Expr *bin;
        parser_advance(p);
        bin = new_expr(EXPR_BINARY, op.line, op.column);
        bin->as.binary.op = ds_strdup(op.lexeme);
        bin->as.binary.left = left;
        bin->as.binary.right = parse_compare(p);
        left = bin;
    }
    return left;
}

static Expr *parse_expression(Parser *p) {
    return parse_equality(p);
}

static Stmt *parse_block(Parser *p) {
    Stmt *block = new_stmt(STMT_BLOCK, p->current.line, p->current.column);
    block->as.block.items = NULL;
    block->as.block.count = 0;
    parser_expect(p, TOK_LBRACE, "'{'");
    while (!parser_check(p, TOK_RBRACE) && !parser_check(p, TOK_EOF)) {
        Stmt *s = parse_statement(p);
        push_stmt(&block->as.block.items, &block->as.block.count, s);
    }
    parser_expect(p, TOK_RBRACE, "'}'");
    return block;
}

static Stmt *parse_var_decl(Parser *p, int is_const) {
    Stmt *s = new_stmt(STMT_VAR_DECL, p->current.line, p->current.column);
    if (!is_const) {
        parser_expect(p, TOK_VAR, "'var'");
        if (parser_check(p, TOK_CREATE)) {
            parser_advance(p);
        }
    } else {
        parser_expect(p, TOK_CONST, "'const'");
    }
    s->as.var_decl.name = consume_identifier(p, "variable");
    s->as.var_decl.type_name = consume_identifier(p, "type");
    s->as.var_decl.nullable = parser_match(p, TOK_QUESTION);
    s->as.var_decl.initializer = NULL;
    s->as.var_decl.is_const = is_const;
    if (parser_match(p, TOK_ASSIGN)) {
        s->as.var_decl.initializer = parse_expression(p);
    }
    parser_expect(p, TOK_SEMICOLON, "';'");
    return s;
}

static Stmt *parse_if_stmt(Parser *p) {
    Stmt *s = new_stmt(STMT_IF, p->current.line, p->current.column);
    s->as.if_stmt.elif_conds = NULL;
    s->as.if_stmt.elif_blocks = NULL;
    s->as.if_stmt.elif_count = 0;
    s->as.if_stmt.else_block = NULL;

    parser_expect(p, TOK_IF, "'if'");
    s->as.if_stmt.condition = parse_expression(p);
    if (parser_check(p, TOK_THEN)) {
        parser_advance(p);
    }
    s->as.if_stmt.then_block = parse_block(p);

    while (parser_match(p, TOK_ELIF)) {
        Expr *cond = parse_expression(p);
        Stmt *blk = parse_block(p);
        s->as.if_stmt.elif_conds = (Expr **)realloc(
            s->as.if_stmt.elif_conds, sizeof(Expr *) * (size_t)(s->as.if_stmt.elif_count + 1));
        s->as.if_stmt.elif_blocks = (Stmt **)realloc(
            s->as.if_stmt.elif_blocks, sizeof(Stmt *) * (size_t)(s->as.if_stmt.elif_count + 1));
        s->as.if_stmt.elif_conds[s->as.if_stmt.elif_count] = cond;
        s->as.if_stmt.elif_blocks[s->as.if_stmt.elif_count] = blk;
        s->as.if_stmt.elif_count++;
    }

    if (parser_match(p, TOK_ELSE)) {
        s->as.if_stmt.else_block = parse_block(p);
    }
    return s;
}

static Stmt *parse_while_stmt(Parser *p) {
    Stmt *s = new_stmt(STMT_WHILE, p->current.line, p->current.column);
    parser_expect(p, TOK_WHILE, "'while'");
    s->as.while_stmt.condition = parse_expression(p);
    if (parser_check(p, TOK_DO)) {
        parser_advance(p);
    }
    s->as.while_stmt.body = parse_block(p);
    return s;
}

static Stmt *parse_statement(Parser *p) {
    if (parser_check(p, TOK_VAR)) {
        return parse_var_decl(p, 0);
    }
    if (parser_check(p, TOK_CONST)) {
        return parse_var_decl(p, 1);
    }
    if (parser_check(p, TOK_RETURN)) {
        Stmt *s = new_stmt(STMT_RETURN, p->current.line, p->current.column);
        parser_advance(p);
        s->as.ret = NULL;
        if (!parser_check(p, TOK_SEMICOLON)) {
            s->as.ret = parse_expression(p);
        }
        parser_expect(p, TOK_SEMICOLON, "';'");
        return s;
    }
    if (parser_check(p, TOK_IF)) {
        return parse_if_stmt(p);
    }
    if (parser_check(p, TOK_WHILE)) {
        return parse_while_stmt(p);
    }
    if (parser_check(p, TOK_LBRACE)) {
        return parse_block(p);
    }

    if (parser_check(p, TOK_IDENTIFIER) && parser_peek_next_type(p) == TOK_ASSIGN) {
        Stmt *s = new_stmt(STMT_ASSIGN, p->current.line, p->current.column);
        s->as.assign.name = consume_identifier(p, "assignment target");
        parser_expect(p, TOK_ASSIGN, "'='");
        s->as.assign.value = parse_expression(p);
        parser_expect(p, TOK_SEMICOLON, "';'");
        return s;
    }

    {
        Stmt *s = new_stmt(STMT_EXPR, p->current.line, p->current.column);
        s->as.expr = parse_expression(p);
        parser_expect(p, TOK_SEMICOLON, "';'");
        return s;
    }
}

static Param parse_param(Parser *p) {
    Param pm;
    pm.name = consume_identifier(p, "parameter");
    pm.type_name = consume_identifier(p, "parameter type");
    pm.nullable = parser_match(p, TOK_QUESTION);
    return pm;
}

static FunctionDecl parse_function(Parser *p) {
    FunctionDecl fn;
    memset(&fn, 0, sizeof(fn));
    parser_expect(p, TOK_FUNC, "'func'");
    parser_expect(p, TOK_CREATE, "'create'");
    fn.name = consume_identifier(p, "function");
    parser_expect(p, TOK_LPAREN, "'('");
    fn.params = NULL;
    fn.param_count = 0;
    if (!parser_check(p, TOK_RPAREN)) {
        do {
            Param pm = parse_param(p);
            fn.params = (Param *)realloc(fn.params, sizeof(Param) * (size_t)(fn.param_count + 1));
            fn.params[fn.param_count++] = pm;
        } while (parser_match(p, TOK_COMMA));
    }
    parser_expect(p, TOK_RPAREN, "')'");
    fn.return_type = ds_strdup("null");
    if (parser_check(p, TOK_IDENTIFIER)) {
        free(fn.return_type);
        fn.return_type = ds_strdup(p->current.lexeme);
        parser_advance(p);
        if (parser_check(p, TOK_QUESTION)) {
            parser_advance(p);
        }
    }
    fn.body = parse_block(p);
    return fn;
}

static Expr *parse_exec_call(Parser *p) {
    Expr *e;
    parser_expect(p, TOK_FUNC, "'func'");
    parser_expect(p, TOK_EXEC, "'exec'");
    e = parse_expression(p);
    parser_expect(p, TOK_SEMICOLON, "';'");
    return e;
}

static void skip_class(Parser *p, Program *prg) {
    char *name;
    parser_expect(p, TOK_CLASS, "'class'");
    parser_expect(p, TOK_CREATE, "'create'");
    name = consume_identifier(p, "class name");
    prg->class_names = (char **)realloc(prg->class_names, sizeof(char *) * (size_t)(prg->class_count + 1));
    prg->class_names[prg->class_count++] = name;
    if (parser_match(p, TOK_EXTENDS)) {
        char *base = consume_identifier(p, "base class");
        free(base);
    }
    parser_expect(p, TOK_LBRACE, "'{'");
    {
        int depth = 1;
        while (depth > 0 && !parser_check(p, TOK_EOF)) {
            if (parser_match(p, TOK_LBRACE)) {
                depth++;
            } else if (parser_match(p, TOK_RBRACE)) {
                depth--;
            } else {
                parser_advance(p);
            }
        }
    }
}

void parser_init(Parser *p, const char *source) {
    memset(p, 0, sizeof(*p));
    lexer_init(&p->lexer, source);
    p->current = lexer_next(&p->lexer);
    p->had_previous = 0;
}

Program parser_parse(Parser *p) {
    Program prg;
    memset(&prg, 0, sizeof(prg));

    while (!parser_check(p, TOK_EOF)) {
        if (parser_match(p, TOK_USE)) {
            parser_expect(p, TOK_LT, "'<'");
            if (!parser_check(p, TOK_IDENTIFIER)) {
                ds_fatal("Parse error at %d:%d: expected library name after use <",
                         p->current.line, p->current.column);
            }
            prg.imports = (char **)realloc(prg.imports, sizeof(char *) * (size_t)(prg.import_count + 1));
            prg.imports[prg.import_count++] = ds_strdup(p->current.lexeme);
            parser_advance(p);
            parser_expect(p, TOK_GT, "'>'");
            continue;
        }
        if (parser_check(p, TOK_CLASS)) {
            skip_class(p, &prg);
            continue;
        }
        if (parser_check(p, TOK_FUNC) && parser_peek_next_type(p) == TOK_CREATE) {
            FunctionDecl fn = parse_function(p);
            prg.funcs = (FunctionDecl *)realloc(prg.funcs, sizeof(FunctionDecl) * (size_t)(prg.func_count + 1));
            prg.funcs[prg.func_count++] = fn;
            continue;
        }
        if (parser_check(p, TOK_FUNC) && parser_peek_next_type(p) == TOK_EXEC) {
            Expr *exec_expr = parse_exec_call(p);
            push_expr(&prg.exec_calls, &prg.exec_count, exec_expr);
            continue;
        }
        ds_fatal("Parse error at %d:%d: unexpected token '%s'",
                 p->current.line, p->current.column, p->current.lexeme);
    }
    return prg;
}

static void free_expr(Expr *e) {
    int i;
    if (!e) {
        return;
    }
    switch (e->kind) {
        case EXPR_INT:
        case EXPR_FLOAT:
        case EXPR_STRING:
        case EXPR_BOOL:
        case EXPR_NULL:
            free(e->as.literal);
            break;
        case EXPR_IDENT:
            free(e->as.ident);
            break;
        case EXPR_BINARY:
            free(e->as.binary.op);
            free_expr(e->as.binary.left);
            free_expr(e->as.binary.right);
            break;
        case EXPR_CALL:
            free_expr(e->as.call.callee);
            for (i = 0; i < e->as.call.arg_count; i++) {
                free_expr(e->as.call.args[i]);
            }
            free(e->as.call.args);
            break;
        case EXPR_LIST:
            for (i = 0; i < e->as.list.item_count; i++) {
                free_expr(e->as.list.items[i]);
            }
            free(e->as.list.items);
            break;
        case EXPR_INDEX:
            free_expr(e->as.index.object);
            free_expr(e->as.index.index);
            break;
    }
    free(e);
}

static void free_stmt(Stmt *s) {
    int i;
    if (!s) {
        return;
    }
    switch (s->kind) {
        case STMT_VAR_DECL:
            free(s->as.var_decl.name);
            free(s->as.var_decl.type_name);
            free_expr(s->as.var_decl.initializer);
            break;
        case STMT_ASSIGN:
            free(s->as.assign.name);
            free_expr(s->as.assign.value);
            break;
        case STMT_EXPR:
            free_expr(s->as.expr);
            break;
        case STMT_RETURN:
            free_expr(s->as.ret);
            break;
        case STMT_IF:
            free_expr(s->as.if_stmt.condition);
            free_stmt(s->as.if_stmt.then_block);
            for (i = 0; i < s->as.if_stmt.elif_count; i++) {
                free_expr(s->as.if_stmt.elif_conds[i]);
                free_stmt(s->as.if_stmt.elif_blocks[i]);
            }
            free(s->as.if_stmt.elif_conds);
            free(s->as.if_stmt.elif_blocks);
            free_stmt(s->as.if_stmt.else_block);
            break;
        case STMT_WHILE:
            free_expr(s->as.while_stmt.condition);
            free_stmt(s->as.while_stmt.body);
            break;
        case STMT_BLOCK:
            for (i = 0; i < s->as.block.count; i++) {
                free_stmt(s->as.block.items[i]);
            }
            free(s->as.block.items);
            break;
    }
    free(s);
}

void parser_free_program(Program *prg) {
    int i, j;
    for (i = 0; i < prg->import_count; i++) {
        free(prg->imports[i]);
    }
    free(prg->imports);

    for (i = 0; i < prg->class_count; i++) {
        free(prg->class_names[i]);
    }
    free(prg->class_names);

    for (i = 0; i < prg->func_count; i++) {
        FunctionDecl *fn = &prg->funcs[i];
        free(fn->name);
        for (j = 0; j < fn->param_count; j++) {
            free(fn->params[j].name);
            free(fn->params[j].type_name);
        }
        free(fn->params);
        free(fn->return_type);
        free_stmt(fn->body);
    }
    free(prg->funcs);

    for (i = 0; i < prg->exec_count; i++) {
        free_expr(prg->exec_calls[i]);
    }
    free(prg->exec_calls);
}
