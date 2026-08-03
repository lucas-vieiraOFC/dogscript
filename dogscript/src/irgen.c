#include "irgen.h"
#include "dogsig.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VT_INT 0
#define VT_FLOAT 1
#define VT_STR 2    /* plain C string (char*): literals and str() casts */
#define VT_DSTR 3   /* heap DogString* returned by a runtime call */

#define VT_IS_STR(v) ((v) == VT_STR || (v) == VT_DSTR)

typedef struct {
    int next_label;
} IRGen;

static char *mk_label(IRGen *g, const char *prefix) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%s_%d", prefix, g->next_label++);
    return ds_strdup(buf);
}

static int is_user_func(const Program *p, const char *name) {
    int i;
    for (i = 0; i < p->func_count; i++) {
        if (strcmp(p->funcs[i].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

static void emit_expr(IRGen *g, const Program *p, IRProgram *ir, const Expr *e);
static void emit_stmt(IRGen *g, const Program *p, IRProgram *ir, const Stmt *s);

/* Map a module-qualified call like "math.sqrt" to its native runtime symbol
   "dog_math_sqrt". A handful of modules expose their functions under a short
   C prefix that differs from the module name (e.g. the `string` module is
   backed by dog_str_*). Names without a '.' fall back to a dot->underscore
   sanitize so plain extern calls still work. Caller frees the result. */
static char *module_symbol(const char *name) {
    const char *dot = strchr(name, '.');
    char buf[256];
    char module[64];
    const char *prefix;
    size_t mod_len;
    if (!dot) {
        char *sym = ds_strdup(name);
        size_t j;
        for (j = 0; j < strlen(sym); j++) {
            if (sym[j] == '.') {
                sym[j] = '_';
            }
        }
        return sym;
    }
    mod_len = (size_t)(dot - name);
    if (mod_len >= sizeof(module)) {
        mod_len = sizeof(module) - 1;
    }
    memcpy(module, name, mod_len);
    module[mod_len] = '\0';

    if (strcmp(module, "string") == 0) {
        prefix = "str";
    } else {
        prefix = module;
    }
    snprintf(buf, sizeof(buf), "dog_%s_%s", prefix, dot + 1);
    return ds_strdup(buf);
}

static int is_compare_op(const char *op) {
    return strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 || strcmp(op, "=") == 0 ||
           strcmp(op, "<") == 0 || strcmp(op, ">") == 0 ||
           strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0;
}

/* Static value type (VT_INT or VT_FLOAT) of an expression, used to choose
   integer vs floating-point IR ops. Strings/pointers/bools collapse to VT_INT
   since they share the integer (GP-register) representation. Local variables
   are assumed integer/pointer; only floats produced inline or by float-typed
   runtime calls are tracked, which covers the math module's usage. */
static int expr_vt(const Program *p, const Expr *e) {
    if (!e) {
        return VT_INT;
    }
    switch (e->kind) {
        case EXPR_FLOAT:
            return VT_FLOAT;
        case EXPR_STRING:
            return VT_STR;
        case EXPR_INT:
        case EXPR_BOOL:
        case EXPR_NULL:
            return VT_INT;
        case EXPR_IDENT:
            if (strchr(e->as.ident, '.')) {
                char *sym = module_symbol(e->as.ident);
                int vt = dog_sig_ret_is_float(sym) ? VT_FLOAT
                       : dog_sig_ret_is_str(sym) ? VT_DSTR : VT_INT;
                free(sym);
                return vt;
            }
            return VT_INT;
        case EXPR_BINARY:
            if (is_compare_op(e->as.binary.op)) {
                return VT_INT;
            }
            if (expr_vt(p, e->as.binary.left) == VT_FLOAT ||
                expr_vt(p, e->as.binary.right) == VT_FLOAT) {
                return VT_FLOAT;
            }
            return VT_INT;
        case EXPR_CALL: {
            const Expr *callee = e->as.call.callee;
            const char *name = (callee && callee->kind == EXPR_IDENT) ? callee->as.ident : NULL;
            if (!name) {
                return VT_INT;
            }
            if (strcmp(name, "float") == 0) {
                return VT_FLOAT;
            }
            if (strcmp(name, "str") == 0) {
                return VT_STR;
            }
            if (strcmp(name, "int") == 0 || strcmp(name, "bool") == 0 ||
                strcmp(name, "cast") == 0) {
                return VT_INT;
            }
            if (is_user_func(p, name)) {
                return VT_INT;
            }
            {
                char *sym = module_symbol(name);
                int vt = dog_sig_ret_is_float(sym) ? VT_FLOAT
                       : dog_sig_ret_is_str(sym) ? VT_DSTR : VT_INT;
                free(sym);
                return vt;
            }
        }
        case EXPR_LIST:
        case EXPR_INDEX:
            return VT_INT;
    }
    return VT_INT;
}

static void emit_call(IRGen *g, const Program *p, IRProgram *ir, const Expr *e) {
    int i;
    const Expr *callee = e->as.call.callee;
    char *name = NULL;
    (void)g;
    for (i = 0; i < e->as.call.arg_count; i++) {
        emit_expr(g, p, ir, e->as.call.args[i]);
    }
    if (callee->kind == EXPR_IDENT) {
        name = callee->as.ident;
    }
    if (!name) {
        ir_emit(ir, IR_CALL_EXTERN, "dog_io_println", 0, 0.0, e->as.call.arg_count);
        return;
    }
    if (strcmp(name, "print") == 0) {
        ir_emit(ir, IR_CALL_EXTERN, "dog_io_println", 0, 0.0, e->as.call.arg_count);
        return;
    }
    if (strcmp(name, "input") == 0) {
        ir_emit(ir, IR_CALL_EXTERN, "dog_io_input", 0, 0.0, e->as.call.arg_count);
        return;
    }
    if (strcmp(name, "float") == 0) {
        if (e->as.call.arg_count == 1 && expr_vt(p, e->as.call.args[0]) == VT_INT) {
            ir_emit(ir, IR_I2F, NULL, 0, 0.0, 0);
        }
        return;
    }
    if (strcmp(name, "int") == 0) {
        if (e->as.call.arg_count == 1 && expr_vt(p, e->as.call.args[0]) == VT_FLOAT) {
            ir_emit(ir, IR_F2I, NULL, 0, 0.0, 0);
        }
        return;
    }
    if (strcmp(name, "str") == 0 || strcmp(name, "bool") == 0 || strcmp(name, "cast") == 0) {
        return;
    }
    if (is_user_func(p, name)) {
        ir_emit(ir, IR_CALL, name, 0, 0.0, e->as.call.arg_count);
    } else {
        char *sym = module_symbol(name);
        ir_emit(ir, IR_CALL_EXTERN, sym, 0, 0.0, e->as.call.arg_count);
        free(sym);
    }
}

static void emit_expr(IRGen *g, const Program *p, IRProgram *ir, const Expr *e) {
    switch (e->kind) {
        case EXPR_INT:
            ir_emit(ir, IR_CONST_INT, NULL, strtol(e->as.literal, NULL, 10), 0.0, 0);
            break;
        case EXPR_FLOAT:
            ir_emit(ir, IR_CONST_FLOAT, NULL, 0, strtod(e->as.literal, NULL), 0);
            break;
        case EXPR_STRING:
            ir_emit(ir, IR_CONST_STR, e->as.literal, 0, 0.0, 0);
            break;
        case EXPR_BOOL:
            ir_emit(ir, IR_CONST_BOOL, NULL, strcmp(e->as.literal, "true") == 0 ? 1 : 0, 0.0, 0);
            break;
        case EXPR_NULL:
            ir_emit(ir, IR_CONST_INT, NULL, 0, 0.0, 0);
            break;
        case EXPR_IDENT:
            /* A module-qualified name used as a value (e.g. math.pi) resolves
               to a zero-argument native call. Plain names are locals. */
            if (strchr(e->as.ident, '.')) {
                char *sym = module_symbol(e->as.ident);
                ir_emit(ir, IR_CALL_EXTERN, sym, 0, 0.0, 0);
                free(sym);
            } else {
                ir_emit(ir, IR_LOAD, e->as.ident, 0, 0.0, 0);
            }
            break;
        case EXPR_BINARY: {
            const char *op = e->as.binary.op;
            int lvt = expr_vt(p, e->as.binary.left);
            int rvt = expr_vt(p, e->as.binary.right);
            int float_ctx = (lvt == VT_FLOAT || rvt == VT_FLOAT);
            int is_eq = (strcmp(op, "==") == 0 || strcmp(op, "=") == 0);
            int is_ne = (strcmp(op, "!=") == 0);

            /* String value comparison: normalize each operand to a char* and
               compare bytes via the runtime, rather than comparing pointers. */
            if ((is_eq || is_ne) && (VT_IS_STR(lvt) || VT_IS_STR(rvt))) {
                emit_expr(g, p, ir, e->as.binary.left);
                if (lvt == VT_DSTR) ir_emit(ir, IR_STR2C, NULL, 0, 0.0, 0);
                emit_expr(g, p, ir, e->as.binary.right);
                if (rvt == VT_DSTR) ir_emit(ir, IR_STR2C, NULL, 0, 0.0, 0);
                ir_emit(ir, IR_CALL_EXTERN, "dog_str_equal", 0, 0.0, 2);
                if (is_ne) ir_emit(ir, IR_NOT, NULL, 0, 0.0, 0);
                break;
            }

            emit_expr(g, p, ir, e->as.binary.left);
            if (float_ctx && lvt == VT_INT) ir_emit(ir, IR_I2F, NULL, 0, 0.0, 0);
            emit_expr(g, p, ir, e->as.binary.right);
            if (float_ctx && rvt == VT_INT) ir_emit(ir, IR_I2F, NULL, 0, 0.0, 0);
            if (float_ctx) {
                if (strcmp(op, "+") == 0) ir_emit(ir, IR_FADD, NULL, 0, 0.0, 0);
                else if (strcmp(op, "-") == 0) ir_emit(ir, IR_FSUB, NULL, 0, 0.0, 0);
                else if (strcmp(op, "*") == 0) ir_emit(ir, IR_FMUL, NULL, 0, 0.0, 0);
                else if (strcmp(op, "/") == 0) ir_emit(ir, IR_FDIV, NULL, 0, 0.0, 0);
                else if (strcmp(op, "=") == 0 || strcmp(op, "==") == 0) ir_emit(ir, IR_FEQ, NULL, 0, 0.0, 0);
                else if (strcmp(op, "!=") == 0) ir_emit(ir, IR_FNEQ, NULL, 0, 0.0, 0);
                else if (strcmp(op, "<") == 0) ir_emit(ir, IR_FLT, NULL, 0, 0.0, 0);
                else if (strcmp(op, ">") == 0) ir_emit(ir, IR_FGT, NULL, 0, 0.0, 0);
                else if (strcmp(op, "<=") == 0) ir_emit(ir, IR_FLTE, NULL, 0, 0.0, 0);
                else if (strcmp(op, ">=") == 0) ir_emit(ir, IR_FGTE, NULL, 0, 0.0, 0);
            } else {
                if (strcmp(op, "+") == 0) ir_emit(ir, IR_ADD, NULL, 0, 0.0, 0);
                else if (strcmp(op, "-") == 0) ir_emit(ir, IR_SUB, NULL, 0, 0.0, 0);
                else if (strcmp(op, "*") == 0) ir_emit(ir, IR_MUL, NULL, 0, 0.0, 0);
                else if (strcmp(op, "/") == 0) ir_emit(ir, IR_DIV, NULL, 0, 0.0, 0);
                else if (strcmp(op, "=") == 0 || strcmp(op, "==") == 0) ir_emit(ir, IR_EQ, NULL, 0, 0.0, 0);
                else if (strcmp(op, "!=") == 0) ir_emit(ir, IR_NEQ, NULL, 0, 0.0, 0);
                else if (strcmp(op, "<") == 0) ir_emit(ir, IR_LT, NULL, 0, 0.0, 0);
                else if (strcmp(op, ">") == 0) ir_emit(ir, IR_GT, NULL, 0, 0.0, 0);
                else if (strcmp(op, "<=") == 0) ir_emit(ir, IR_LTE, NULL, 0, 0.0, 0);
                else if (strcmp(op, ">=") == 0) ir_emit(ir, IR_GTE, NULL, 0, 0.0, 0);
            }
            break;
        }
        case EXPR_CALL:
            emit_call(g, p, ir, e);
            break;
        case EXPR_LIST: {
            int i;
            /* Build the list at runtime: new, then push each element. The list
               pointer stays on the value stack across pushes (see IR_LIST_PUSH). */
            ir_emit(ir, IR_LIST_NEW, NULL, 0, 0.0, 0);
            for (i = 0; i < e->as.list.item_count; i++) {
                emit_expr(g, p, ir, e->as.list.items[i]);
                ir_emit(ir, IR_LIST_PUSH, NULL, 0, 0.0, 0);
            }
            break;
        }
        case EXPR_INDEX: {
            int kvt = expr_vt(p, e->as.index.index);
            emit_expr(g, p, ir, e->as.index.object);
            emit_expr(g, p, ir, e->as.index.index);
            if (VT_IS_STR(kvt)) {
                /* string key -> map lookup */
                if (kvt == VT_DSTR) ir_emit(ir, IR_STR2C, NULL, 0, 0.0, 0);
                ir_emit(ir, IR_CALL_EXTERN, "dog_map_get", 0, 0.0, 2);
            } else {
                ir_emit(ir, IR_CALL_EXTERN, "dog_list_get", 0, 0.0, 2);
            }
            break;
        }
    }
}

static void emit_stmt(IRGen *g, const Program *p, IRProgram *ir, const Stmt *s) {
    int i;
    switch (s->kind) {
        case STMT_VAR_DECL:
            ir_emit(ir, IR_ALLOC, s->as.var_decl.name, 0, 0.0, 0);
            if (s->as.var_decl.initializer) {
                emit_expr(g, p, ir, s->as.var_decl.initializer);
                ir_emit(ir, IR_STORE, s->as.var_decl.name, 0, 0.0, 0);
            }
            break;
        case STMT_ASSIGN:
            emit_expr(g, p, ir, s->as.assign.value);
            ir_emit(ir, IR_STORE, s->as.assign.name, 0, 0.0, 0);
            break;
        case STMT_EXPR:
            emit_expr(g, p, ir, s->as.expr);
            break;
        case STMT_RETURN:
            if (s->as.ret) {
                emit_expr(g, p, ir, s->as.ret);
                ir_emit(ir, IR_RETURN, NULL, 0, 0.0, 0);
            } else {
                ir_emit(ir, IR_RETURN_VOID, NULL, 0, 0.0, 0);
            }
            break;
        case STMT_BLOCK:
            for (i = 0; i < s->as.block.count; i++) {
                emit_stmt(g, p, ir, s->as.block.items[i]);
            }
            break;
        case STMT_IF: {
            char *l_else = mk_label(g, "else");
            char *l_end = mk_label(g, "ifend");
            emit_expr(g, p, ir, s->as.if_stmt.condition);
            ir_emit(ir, IR_JUMP_IFNOT, l_else, 0, 0.0, 0);
            emit_stmt(g, p, ir, s->as.if_stmt.then_block);
            ir_emit(ir, IR_JUMP, l_end, 0, 0.0, 0);
            ir_emit(ir, IR_LABEL, l_else, 0, 0.0, 0);
            for (i = 0; i < s->as.if_stmt.elif_count; i++) {
                char *l_next = mk_label(g, "elifnext");
                emit_expr(g, p, ir, s->as.if_stmt.elif_conds[i]);
                ir_emit(ir, IR_JUMP_IFNOT, l_next, 0, 0.0, 0);
                emit_stmt(g, p, ir, s->as.if_stmt.elif_blocks[i]);
                ir_emit(ir, IR_JUMP, l_end, 0, 0.0, 0);
                ir_emit(ir, IR_LABEL, l_next, 0, 0.0, 0);
                free(l_next);
            }
            if (s->as.if_stmt.else_block) {
                emit_stmt(g, p, ir, s->as.if_stmt.else_block);
            }
            ir_emit(ir, IR_LABEL, l_end, 0, 0.0, 0);
            free(l_else);
            free(l_end);
            break;
        }
        case STMT_WHILE: {
            char *l_begin = mk_label(g, "while_begin");
            char *l_end = mk_label(g, "while_end");
            ir_emit(ir, IR_LABEL, l_begin, 0, 0.0, 0);
            emit_expr(g, p, ir, s->as.while_stmt.condition);
            ir_emit(ir, IR_JUMP_IFNOT, l_end, 0, 0.0, 0);
            emit_stmt(g, p, ir, s->as.while_stmt.body);
            ir_emit(ir, IR_JUMP, l_begin, 0, 0.0, 0);
            ir_emit(ir, IR_LABEL, l_end, 0, 0.0, 0);
            free(l_begin);
            free(l_end);
            break;
        }
    }
}

int irgen_program(const Program *program, IRProgram *out) {
    int i, j;
    IRGen g;
    g.next_label = 1;
    ir_init(out);

    for (i = 0; i < program->func_count; i++) {
        FunctionDecl *fn = &program->funcs[i];
        char label[256];
        snprintf(label, sizeof(label), "fn:%s", fn->name);
        ir_emit(out, IR_LABEL, label, 0, 0.0, fn->param_count);
        ir_emit(out, IR_PUSH_FRAME, NULL, 0, 0.0, 0);
        for (j = 0; j < fn->param_count; j++) {
            ir_emit(out, IR_ALLOC, fn->params[j].name, 0, 0.0, 0);
        }
        emit_stmt(&g, program, out, fn->body);
        ir_emit(out, IR_RETURN_VOID, NULL, 0, 0.0, 0);
        ir_emit(out, IR_POP_FRAME, NULL, 0, 0.0, 0);
    }

    ir_emit(out, IR_LABEL, "fn:@entry", 0, 0.0, 0);
    ir_emit(out, IR_PUSH_FRAME, NULL, 0, 0.0, 0);
    for (i = 0; i < program->exec_count; i++) {
        emit_expr(&g, program, out, program->exec_calls[i]);
    }
    ir_emit(out, IR_RETURN_VOID, NULL, 0, 0.0, 0);
    ir_emit(out, IR_POP_FRAME, NULL, 0, 0.0, 0);
    return 0;
}
