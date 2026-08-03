#include "analyzer.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *name;
    char *type_name;
} Symbol;

typedef struct Scope {
    Symbol *symbols;
    int symbol_count;
    struct Scope *parent;
} Scope;

static int is_builtin_type(const char *t) {
    return strcmp(t, "str") == 0 || strcmp(t, "int") == 0 || strcmp(t, "float") == 0 ||
           strcmp(t, "bool") == 0 || strcmp(t, "list") == 0 || strcmp(t, "map") == 0 ||
           strcmp(t, "func") == 0 || strcmp(t, "null") == 0;
}

static int is_builtin_symbol(const char *name) {
    return strcmp(name, "print") == 0 || strcmp(name, "input") == 0 ||
           strcmp(name, "str") == 0 || strcmp(name, "int") == 0 ||
           strcmp(name, "float") == 0 || strcmp(name, "bool") == 0 ||
           strcmp(name, "cast") == 0 || strcmp(name, "range") == 0;
}

static Scope *scope_new(Scope *parent) {
    Scope *s = (Scope *)calloc(1, sizeof(Scope));
    s->parent = parent;
    return s;
}

static void scope_free(Scope *s) {
    int i;
    if (!s) {
        return;
    }
    for (i = 0; i < s->symbol_count; i++) {
        free(s->symbols[i].name);
        free(s->symbols[i].type_name);
    }
    free(s->symbols);
    free(s);
}

static int scope_define(Scope *s, const char *name, const char *type_name) {
    int i;
    for (i = 0; i < s->symbol_count; i++) {
        if (strcmp(s->symbols[i].name, name) == 0) {
            return 0;
        }
    }
    s->symbols = (Symbol *)realloc(s->symbols, sizeof(Symbol) * (size_t)(s->symbol_count + 1));
    s->symbols[s->symbol_count].name = ds_strdup(name);
    s->symbols[s->symbol_count].type_name = ds_strdup(type_name);
    s->symbol_count++;
    return 1;
}

static const char *scope_lookup(Scope *s, const char *name) {
    Scope *cur = s;
    int i;
    while (cur) {
        for (i = 0; i < cur->symbol_count; i++) {
            if (strcmp(cur->symbols[i].name, name) == 0) {
                return cur->symbols[i].type_name;
            }
        }
        cur = cur->parent;
    }
    return NULL;
}

/* True if `name` is "module.member" where module is one of the program's
   imported stdlib modules. Such references resolve to native runtime symbols
   during IR generation, so the analyzer treats them as known. */
static int is_imported_member(const Program *prog, const char *name) {
    const char *dot = strchr(name, '.');
    size_t mod_len;
    int i;
    if (!dot || !prog) {
        return 0;
    }
    mod_len = (size_t)(dot - name);
    for (i = 0; i < prog->import_count; i++) {
        if (strlen(prog->imports[i]) == mod_len &&
            strncmp(prog->imports[i], name, mod_len) == 0) {
            return 1;
        }
    }
    return 0;
}

static int analyze_expr(const Program *prog, Scope *scope, const Expr *e, int *errors);
static int analyze_stmt(const Program *prog, Scope *scope, const Stmt *stmt, int *errors);

static int analyze_expr(const Program *prog, Scope *scope, const Expr *e, int *errors) {
    int i;
    if (!e) {
        return 1;
    }
    switch (e->kind) {
        case EXPR_INT:
        case EXPR_FLOAT:
        case EXPR_STRING:
        case EXPR_BOOL:
        case EXPR_NULL:
            return 1;
        case EXPR_IDENT: {
            if (!scope_lookup(scope, e->as.ident) && !is_builtin_symbol(e->as.ident) &&
                !is_imported_member(prog, e->as.ident)) {
                fprintf(stderr, "Analyzer error at %d:%d: unknown identifier '%s'\n",
                        e->line, e->column, e->as.ident);
                (*errors)++;
                return 0;
            }
            return 1;
        }
        case EXPR_BINARY:
            analyze_expr(prog, scope, e->as.binary.left, errors);
            analyze_expr(prog, scope, e->as.binary.right, errors);
            return 1;
        case EXPR_CALL:
            analyze_expr(prog, scope, e->as.call.callee, errors);
            for (i = 0; i < e->as.call.arg_count; i++) {
                analyze_expr(prog, scope, e->as.call.args[i], errors);
            }
            return 1;
        case EXPR_LIST:
            for (i = 0; i < e->as.list.item_count; i++) {
                analyze_expr(prog, scope, e->as.list.items[i], errors);
            }
            return 1;
        case EXPR_INDEX:
            analyze_expr(prog, scope, e->as.index.object, errors);
            analyze_expr(prog, scope, e->as.index.index, errors);
            return 1;
    }
    return 1;
}

static int analyze_stmt(const Program *prog, Scope *scope, const Stmt *stmt, int *errors) {
    int i;
    switch (stmt->kind) {
        case STMT_VAR_DECL:
            if (!is_builtin_type(stmt->as.var_decl.type_name)) {
                fprintf(stderr, "Analyzer warning at %d:%d: unknown type '%s' treated as object\n",
                        stmt->line, stmt->column, stmt->as.var_decl.type_name);
            }
            if (!scope_define(scope, stmt->as.var_decl.name, stmt->as.var_decl.type_name)) {
                fprintf(stderr, "Analyzer error at %d:%d: duplicate symbol '%s'\n",
                        stmt->line, stmt->column, stmt->as.var_decl.name);
                (*errors)++;
            }
            analyze_expr(prog, scope, stmt->as.var_decl.initializer, errors);
            return 1;
        case STMT_ASSIGN:
            if (!scope_lookup(scope, stmt->as.assign.name)) {
                fprintf(stderr, "Analyzer error at %d:%d: assignment to unknown variable '%s'\n",
                        stmt->line, stmt->column, stmt->as.assign.name);
                (*errors)++;
            }
            analyze_expr(prog, scope, stmt->as.assign.value, errors);
            return 1;
        case STMT_EXPR:
            return analyze_expr(prog, scope, stmt->as.expr, errors);
        case STMT_RETURN:
            return analyze_expr(prog, scope, stmt->as.ret, errors);
        case STMT_IF: {
            Scope *inner = scope_new(scope);
            analyze_expr(prog, scope, stmt->as.if_stmt.condition, errors);
            analyze_stmt(prog, inner, stmt->as.if_stmt.then_block, errors);
            scope_free(inner);
            for (i = 0; i < stmt->as.if_stmt.elif_count; i++) {
                Scope *elif_scope = scope_new(scope);
                analyze_expr(prog, scope, stmt->as.if_stmt.elif_conds[i], errors);
                analyze_stmt(prog, elif_scope, stmt->as.if_stmt.elif_blocks[i], errors);
                scope_free(elif_scope);
            }
            if (stmt->as.if_stmt.else_block) {
                Scope *else_scope = scope_new(scope);
                analyze_stmt(prog, else_scope, stmt->as.if_stmt.else_block, errors);
                scope_free(else_scope);
            }
            return 1;
        }
        case STMT_WHILE: {
            Scope *inner = scope_new(scope);
            analyze_expr(prog, scope, stmt->as.while_stmt.condition, errors);
            analyze_stmt(prog, inner, stmt->as.while_stmt.body, errors);
            scope_free(inner);
            return 1;
        }
        case STMT_BLOCK:
            for (i = 0; i < stmt->as.block.count; i++) {
                analyze_stmt(prog, scope, stmt->as.block.items[i], errors);
            }
            return 1;
    }
    return 1;
}

int analyze_program(const Program *program) {
    int i, j;
    int errors = 0;
    Scope *global = scope_new(NULL);

    for (i = 0; i < program->func_count; i++) {
        if (!scope_define(global, program->funcs[i].name, "func")) {
            fprintf(stderr, "Analyzer error: duplicate function '%s'\n", program->funcs[i].name);
            errors++;
        }
    }
    for (i = 0; i < program->func_count; i++) {
        Scope *fn_scope = scope_new(global);
        for (j = 0; j < program->funcs[i].param_count; j++) {
            if (!scope_define(fn_scope, program->funcs[i].params[j].name, program->funcs[i].params[j].type_name)) {
                fprintf(stderr, "Analyzer error: duplicate parameter '%s' in '%s'\n",
                        program->funcs[i].params[j].name, program->funcs[i].name);
                errors++;
            }
        }
        analyze_stmt(program, fn_scope, program->funcs[i].body, &errors);
        scope_free(fn_scope);
    }

    for (i = 0; i < program->exec_count; i++) {
        analyze_expr(program, global, program->exec_calls[i], &errors);
    }

    scope_free(global);
    return errors == 0 ? 0 : -1;
}
