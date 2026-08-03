#ifndef DOGSCRIPT_IR_H
#define DOGSCRIPT_IR_H

#include <stddef.h>

typedef enum {
    IR_CONST_INT,
    IR_CONST_FLOAT,
    IR_CONST_STR,
    IR_CONST_BOOL,
    IR_LOAD,
    IR_STORE,
    IR_ADD,
    IR_SUB,
    IR_MUL,
    IR_DIV,
    IR_MOD,
    IR_FADD,
    IR_FSUB,
    IR_FMUL,
    IR_FDIV,
    IR_EQ,
    IR_NEQ,
    IR_LT,
    IR_GT,
    IR_LTE,
    IR_GTE,
    IR_FEQ,
    IR_FNEQ,
    IR_FLT,
    IR_FGT,
    IR_FLTE,
    IR_FGTE,
    IR_I2F,
    IR_F2I,
    IR_STR2C,
    IR_AND,
    IR_OR,
    IR_NOT,
    IR_JUMP,
    IR_JUMP_IF,
    IR_JUMP_IFNOT,
    IR_LABEL,
    IR_CALL,
    IR_CALL_EXTERN,
    IR_RETURN,
    IR_RETURN_VOID,
    IR_ALLOC,
    IR_PUSH_FRAME,
    IR_POP_FRAME,
    IR_CLASS_NEW,
    IR_FIELD_GET,
    IR_FIELD_SET,
    IR_LIST_NEW,
    IR_LIST_PUSH,
    IR_MAP_NEW,
    IR_MAP_SET,
    IR_CAST,
    IR_NOP
} IROp;

typedef struct IRInstr {
    IROp op;
    char *arg1;
    long ival;
    double fval;
    int nargs;
} IRInstr;

typedef struct IRProgram {
    IRInstr *instrs;
    size_t count;
    size_t cap;
} IRProgram;

void ir_init(IRProgram *p);
void ir_free(IRProgram *p);
void ir_emit(IRProgram *p, IROp op, const char *arg1, long ival, double fval, int nargs);

#endif
