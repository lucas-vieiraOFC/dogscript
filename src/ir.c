#include "ir.h"
#include "utils.h"

#include <stdlib.h>
#include <string.h>

void ir_init(IRProgram *p) {
    p->instrs = NULL;
    p->count = 0;
    p->cap = 0;
}

void ir_free(IRProgram *p) {
    size_t i;
    for (i = 0; i < p->count; i++) {
        free(p->instrs[i].arg1);
    }
    free(p->instrs);
    p->instrs = NULL;
    p->count = 0;
    p->cap = 0;
}

void ir_emit(IRProgram *p, IROp op, const char *arg1, long ival, double fval, int nargs) {
    IRInstr *in;
    if (p->count == p->cap) {
        size_t next = p->cap == 0 ? 64 : p->cap * 2;
        p->instrs = (IRInstr *)realloc(p->instrs, sizeof(IRInstr) * next);
        p->cap = next;
    }
    in = &p->instrs[p->count++];
    in->op = op;
    in->arg1 = arg1 ? ds_strdup(arg1) : NULL;
    in->ival = ival;
    in->fval = fval;
    in->nargs = nargs;
}
