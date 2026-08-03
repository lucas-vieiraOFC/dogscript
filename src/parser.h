#ifndef DOGSCRIPT_PARSER_H
#define DOGSCRIPT_PARSER_H

#include "lexer.h"

typedef enum {
    EXPR_INT,
    EXPR_FLOAT,
    EXPR_STRING,
    EXPR_BOOL,
    EXPR_NULL,
    EXPR_IDENT,
    EXPR_BINARY,
    EXPR_CALL,
    EXPR_LIST,
    EXPR_INDEX
} ExprKind;

typedef struct Expr Expr;
typedef struct Stmt Stmt;

struct Expr {
    ExprKind kind;
    int line;
    int column;
    union {
        char *literal;
        char *ident;
        struct {
            char *op;
            Expr *left;
            Expr *right;
        } binary;
        struct {
            Expr *callee;
            Expr **args;
            int arg_count;
        } call;
        struct {
            Expr **items;
            int item_count;
        } list;
        struct {
            Expr *object;
            Expr *index;
        } index;
    } as;
};

typedef enum {
    STMT_VAR_DECL,
    STMT_ASSIGN,
    STMT_EXPR,
    STMT_RETURN,
    STMT_IF,
    STMT_WHILE,
    STMT_BLOCK
} StmtKind;

typedef struct {
    char *name;
    char *type_name;
    int nullable;
    Expr *initializer;
    int is_const;
} VarDecl;

typedef struct {
    char *name;
    Expr *value;
} AssignStmt;

typedef struct {
    Expr *condition;
    Stmt *then_block;
    Expr **elif_conds;
    Stmt **elif_blocks;
    int elif_count;
    Stmt *else_block;
} IfStmt;

typedef struct {
    Expr *condition;
    Stmt *body;
} WhileStmt;

struct Stmt {
    StmtKind kind;
    int line;
    int column;
    union {
        VarDecl var_decl;
        AssignStmt assign;
        Expr *expr;
        Expr *ret;
        IfStmt if_stmt;
        WhileStmt while_stmt;
        struct {
            Stmt **items;
            int count;
        } block;
    } as;
};

typedef struct {
    char *name;
    char *type_name;
    int nullable;
} Param;

typedef struct {
    char *name;
    Param *params;
    int param_count;
    char *return_type;
    Stmt *body;
} FunctionDecl;

typedef struct {
    char **imports;
    int import_count;
    char **class_names;
    int class_count;
    FunctionDecl *funcs;
    int func_count;
    Expr **exec_calls;
    int exec_count;
} Program;

typedef struct {
    Lexer lexer;
    Token current;
    Token previous;
    int had_previous;
} Parser;

void parser_init(Parser *p, const char *source);
Program parser_parse(Parser *p);
void parser_free_program(Program *prg);

#endif
