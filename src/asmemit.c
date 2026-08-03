#include "asmemit.h"
#include "dogsig.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

typedef struct {
    char *name;
    long offset;
} Local;

typedef struct {
    char *fn_label;
    size_t start;
    size_t end;
    int param_count;
    Local *locals;
    int local_count;
    long stack_size;
} FnInfo;

static char *sanitize(const char *in) {
    size_t i, n = strlen(in);
    char *out = ds_strdup(in);
    for (i = 0; i < n; i++) {
        if (!(out[i] == '_' || (out[i] >= '0' && out[i] <= '9') ||
              (out[i] >= 'a' && out[i] <= 'z') || (out[i] >= 'A' && out[i] <= 'Z'))) {
            out[i] = '_';
        }
    }
    return out;
}

static void outln(FILE *f, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
}

static int find_local(const FnInfo *fn, const char *name) {
    int i;
    for (i = 0; i < fn->local_count; i++) {
        if (strcmp(fn->locals[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static void collect_functions(const IRProgram *ir, FnInfo **out_fns, int *out_count) {
    size_t i;
    FnInfo *fns = NULL;
    int count = 0;
    for (i = 0; i < ir->count; i++) {
        IRInstr *in = &ir->instrs[i];
        if (in->op == IR_LABEL && in->arg1 && strncmp(in->arg1, "fn:", 3) == 0) {
            fns = (FnInfo *)realloc(fns, sizeof(FnInfo) * (size_t)(count + 1));
            memset(&fns[count], 0, sizeof(FnInfo));
            fns[count].fn_label = ds_strdup(in->arg1 + 3);
            fns[count].start = i;
            fns[count].end = ir->count;
            fns[count].param_count = in->nargs;
            if (count > 0) {
                fns[count - 1].end = i;
            }
            count++;
        }
    }
    *out_fns = fns;
    *out_count = count;
}

typedef struct {
    char *value;
    char *label;
} StrConst;

static char *get_str_label(StrConst **arr, int *count, const char *s) {
    int i;
    char buf[64];
    for (i = 0; i < *count; i++) {
        if (strcmp((*arr)[i].value, s) == 0) {
            return (*arr)[i].label;
        }
    }
    *arr = (StrConst *)realloc(*arr, sizeof(StrConst) * (size_t)(*count + 1));
    snprintf(buf, sizeof(buf), ".Lstr_%d", *count);
    (*arr)[*count].value = ds_strdup(s);
    (*arr)[*count].label = ds_strdup(buf);
    (*count)++;
    return (*arr)[*count - 1].label;
}

static const char *cmp_set_op(IROp op) {
    switch (op) {
        case IR_EQ: return "sete";
        case IR_NEQ: return "setne";
        case IR_LT: return "setl";
        case IR_GT: return "setg";
        case IR_LTE: return "setle";
        case IR_GTE: return "setge";
        default: return "sete";
    }
}

int asmemit_x64(const IRProgram *ir, const char *asm_path) {
    FILE *f = fopen(asm_path, "wb");
    FnInfo *fns = NULL;
    int fn_count = 0;
    int fi, si;
    StrConst *strs = NULL;
    int str_count = 0;
    const char *arg_regs[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};

    if (!f) {
        return -1;
    }

    collect_functions(ir, &fns, &fn_count);
    for (fi = 0; fi < fn_count; fi++) {
        size_t i;
        long off = 8;
        for (i = fns[fi].start + 1; i < fns[fi].end; i++) {
            IRInstr *in = &ir->instrs[i];
            if (in->op == IR_ALLOC && in->arg1 && find_local(&fns[fi], in->arg1) < 0) {
                fns[fi].locals = (Local *)realloc(fns[fi].locals, sizeof(Local) * (size_t)(fns[fi].local_count + 1));
                fns[fi].locals[fns[fi].local_count].name = ds_strdup(in->arg1);
                fns[fi].locals[fns[fi].local_count].offset = off;
                fns[fi].local_count++;
                off += 8;
            } else if (in->op == IR_CONST_STR && in->arg1) {
                (void)get_str_label(&strs, &str_count, in->arg1);
            }
        }
        fns[fi].stack_size = ((off + 15) / 16) * 16;
    }

    outln(f, ".text");
    outln(f, ".globl _start");
    outln(f, "_start:");
    outln(f, "    movq (%%rsp), %%rdi");
    outln(f, "    leaq 8(%%rsp), %%rsi");
    outln(f, "    call main");
    outln(f, "    movq %%rax, %%rdi");
    outln(f, "    movq $60, %%rax");
    outln(f, "    syscall");
    outln(f, "");

    outln(f, ".globl main");
    outln(f, "main:");
    outln(f, "    pushq %%rbp");
    outln(f, "    movq %%rsp, %%rbp");
    outln(f, "    movq %%rdi, dog_argc(%%rip)");
    outln(f, "    movq %%rsi, dog_argv(%%rip)");
    outln(f, "    call dog_runtime_init");
    outln(f, "    call dog_main");
    outln(f, "    call dog_runtime_cleanup");
    outln(f, "    xorl %%eax, %%eax");
    outln(f, "    popq %%rbp");
    outln(f, "    ret");
    outln(f, "");

    for (fi = 0; fi < fn_count; fi++) {
        char *sym = NULL;
        size_t i;
        if (strcmp(fns[fi].fn_label, "@entry") == 0) {
            sym = ds_strdup("dog_main");
        } else {
            char *san = sanitize(fns[fi].fn_label);
            sym = ds_join3("dog_fn_", "", san);
            free(san);
        }
        outln(f, ".globl %s", sym);
        outln(f, "%s:", sym);
        outln(f, "    pushq %%rbp");
        outln(f, "    movq %%rsp, %%rbp");
        if (fns[fi].stack_size > 0) {
            outln(f, "    subq $%ld, %%rsp", fns[fi].stack_size);
        }
        for (i = 0; i < (size_t)fns[fi].param_count && i < (size_t)fns[fi].local_count && i < 6; i++) {
            outln(f, "    movq %s, -%ld(%%rbp)", arg_regs[i], fns[fi].locals[i].offset);
        }
        for (i = fns[fi].start + 1; i < fns[fi].end; i++) {
            IRInstr *in = &ir->instrs[i];
            switch (in->op) {
                case IR_LABEL:
                    if (in->arg1 && strncmp(in->arg1, "fn:", 3) != 0) {
                        char *l = sanitize(in->arg1);
                        outln(f, ".L_%s:", l);
                        free(l);
                    }
                    break;
                case IR_CONST_INT:
                case IR_CONST_BOOL:
                    outln(f, "    movq $%ld, %%rax", in->ival);
                    outln(f, "    pushq %%rax");
                    break;
                case IR_CONST_FLOAT: {
                    /* Push the raw IEEE-754 bit pattern of the double. */
                    union { double d; unsigned long u; } bits;
                    bits.d = in->fval;
                    outln(f, "    movabsq $%lu, %%rax", bits.u);
                    outln(f, "    pushq %%rax");
                    break;
                }
                case IR_CONST_STR: {
                    char *lbl = get_str_label(&strs, &str_count, in->arg1 ? in->arg1 : "");
                    outln(f, "    leaq %s(%%rip), %%rax", lbl);
                    outln(f, "    pushq %%rax");
                    break;
                }
                case IR_LOAD: {
                    int idx = find_local(&fns[fi], in->arg1);
                    if (idx >= 0) {
                        outln(f, "    movq -%ld(%%rbp), %%rax", fns[fi].locals[idx].offset);
                        outln(f, "    pushq %%rax");
                    } else {
                        outln(f, "    movq $0, %%rax");
                        outln(f, "    pushq %%rax");
                    }
                    break;
                }
                case IR_STORE: {
                    int idx = find_local(&fns[fi], in->arg1);
                    outln(f, "    popq %%rax");
                    if (idx >= 0) {
                        outln(f, "    movq %%rax, -%ld(%%rbp)", fns[fi].locals[idx].offset);
                    }
                    break;
                }
                case IR_ADD:
                case IR_SUB:
                case IR_MUL:
                    outln(f, "    popq %%rbx");
                    outln(f, "    popq %%rax");
                    if (in->op == IR_ADD) outln(f, "    addq %%rbx, %%rax");
                    else if (in->op == IR_SUB) outln(f, "    subq %%rbx, %%rax");
                    else outln(f, "    imulq %%rbx, %%rax");
                    outln(f, "    pushq %%rax");
                    break;
                case IR_DIV:
                case IR_MOD:
                    outln(f, "    popq %%rbx");
                    outln(f, "    popq %%rax");
                    outln(f, "    cqto");
                    outln(f, "    idivq %%rbx");
                    if (in->op == IR_MOD) outln(f, "    movq %%rdx, %%rax");
                    outln(f, "    pushq %%rax");
                    break;
                case IR_EQ:
                case IR_NEQ:
                case IR_LT:
                case IR_GT:
                case IR_LTE:
                case IR_GTE:
                    outln(f, "    popq %%rbx");
                    outln(f, "    popq %%rax");
                    outln(f, "    cmpq %%rbx, %%rax");
                    outln(f, "    %s %%al", cmp_set_op(in->op));
                    outln(f, "    movzbq %%al, %%rax");
                    outln(f, "    pushq %%rax");
                    break;
                case IR_NOT:
                    outln(f, "    popq %%rax");
                    outln(f, "    cmpq $0, %%rax");
                    outln(f, "    sete %%al");
                    outln(f, "    movzbq %%al, %%rax");
                    outln(f, "    pushq %%rax");
                    break;
                case IR_FADD:
                case IR_FSUB:
                case IR_FMUL:
                case IR_FDIV:
                    /* left at 8(%rsp), right at (%rsp). */
                    outln(f, "    movsd 8(%%rsp), %%xmm0");
                    outln(f, "    movsd (%%rsp), %%xmm1");
                    outln(f, "    addq $8, %%rsp");
                    if (in->op == IR_FADD) outln(f, "    addsd %%xmm1, %%xmm0");
                    else if (in->op == IR_FSUB) outln(f, "    subsd %%xmm1, %%xmm0");
                    else if (in->op == IR_FMUL) outln(f, "    mulsd %%xmm1, %%xmm0");
                    else outln(f, "    divsd %%xmm1, %%xmm0");
                    outln(f, "    movsd %%xmm0, (%%rsp)");
                    break;
                case IR_FEQ:
                case IR_FNEQ:
                case IR_FLT:
                case IR_FGT:
                case IR_FLTE:
                case IR_FGTE:
                    /* left at 8(%rsp), right at (%rsp); result is a 0/1 int. */
                    outln(f, "    movsd 8(%%rsp), %%xmm0");
                    outln(f, "    movsd (%%rsp), %%xmm1");
                    outln(f, "    addq $16, %%rsp");
                    outln(f, "    ucomisd %%xmm1, %%xmm0");
                    if (in->op == IR_FEQ) outln(f, "    sete %%al");
                    else if (in->op == IR_FNEQ) outln(f, "    setne %%al");
                    else if (in->op == IR_FLT) outln(f, "    setb %%al");
                    else if (in->op == IR_FGT) outln(f, "    seta %%al");
                    else if (in->op == IR_FLTE) outln(f, "    setbe %%al");
                    else outln(f, "    setae %%al");
                    outln(f, "    movzbq %%al, %%rax");
                    outln(f, "    pushq %%rax");
                    break;
                case IR_I2F:
                    outln(f, "    popq %%rax");
                    outln(f, "    cvtsi2sd %%rax, %%xmm0");
                    outln(f, "    subq $8, %%rsp");
                    outln(f, "    movsd %%xmm0, (%%rsp)");
                    break;
                case IR_F2I:
                    outln(f, "    movsd (%%rsp), %%xmm0");
                    outln(f, "    cvttsd2si %%xmm0, %%rax");
                    outln(f, "    movq %%rax, (%%rsp)");
                    break;
                case IR_STR2C:
                    /* Replace a DogString* on the stack with its char* payload
                       (the first field of the struct, at offset 0). */
                    outln(f, "    movq (%%rsp), %%rax");
                    outln(f, "    movq (%%rax), %%rax");
                    outln(f, "    movq %%rax, (%%rsp)");
                    break;
                case IR_LIST_NEW:
                    outln(f, "    call dog_list_new");
                    outln(f, "    pushq %%rax");
                    break;
                case IR_LIST_PUSH:
                    /* Stack: [..., list, elem] with elem on top. Append elem to
                       list and leave the list pointer on the stack. */
                    outln(f, "    popq %%rsi");
                    outln(f, "    movq (%%rsp), %%rdi");
                    outln(f, "    call dog_list_push");
                    break;
                case IR_JUMP: {
                    char *l = sanitize(in->arg1 ? in->arg1 : "lbl");
                    outln(f, "    jmp .L_%s", l);
                    free(l);
                    break;
                }
                case IR_JUMP_IF:
                case IR_JUMP_IFNOT: {
                    char *l = sanitize(in->arg1 ? in->arg1 : "lbl");
                    outln(f, "    popq %%rax");
                    outln(f, "    cmpq $0, %%rax");
                    if (in->op == IR_JUMP_IF) outln(f, "    jne .L_%s", l);
                    else outln(f, "    je .L_%s", l);
                    free(l);
                    break;
                }
                case IR_CALL: {
                    int a;
                    char *san = sanitize(in->arg1 ? in->arg1 : "");
                    char *target = ds_join3("dog_fn_", "", san);
                    free(san);
                    for (a = in->nargs - 1; a >= 0 && a < 6; a--) {
                        outln(f, "    popq %s", arg_regs[a]);
                    }
                    outln(f, "    call %s", target);
                    outln(f, "    pushq %%rax");
                    free(target);
                    break;
                }
                case IR_CALL_EXTERN: {
                    /* Dispatch arguments to GP vs SSE registers per the System V
                       ABI, using the runtime signature table to classify floats.
                       Args sit on the value stack with the last one on top, so
                       we consume from the top while register slots were assigned
                       front-to-back. */
                    int a, gp = 0, fp = 0, nf = 0;
                    int isf[16];
                    int slot[16];
                    char *target = sanitize(in->arg1 ? in->arg1 : "");
                    int n = in->nargs;
                    if (n > 16) {
                        n = 16;
                    }
                    for (a = 0; a < n; a++) {
                        isf[a] = dog_sig_arg_is_float(target, a);
                        if (isf[a]) {
                            slot[a] = fp++;
                            nf++;
                        } else {
                            slot[a] = gp++;
                        }
                    }
                    for (a = n - 1; a >= 0; a--) {
                        if (isf[a] && slot[a] < 8) {
                            outln(f, "    movsd (%%rsp), %%xmm%d", slot[a]);
                            outln(f, "    addq $8, %%rsp");
                        } else if (!isf[a] && slot[a] < 6) {
                            outln(f, "    popq %s", arg_regs[slot[a]]);
                        } else {
                            outln(f, "    addq $8, %%rsp");
                        }
                    }
                    outln(f, "    movl $%d, %%eax", nf);
                    outln(f, "    call %s", target);
                    if (dog_sig_ret_is_float(target)) {
                        outln(f, "    subq $8, %%rsp");
                        outln(f, "    movsd %%xmm0, (%%rsp)");
                    } else {
                        outln(f, "    pushq %%rax");
                    }
                    free(target);
                    break;
                }
                case IR_RETURN:
                    outln(f, "    popq %%rax");
                    outln(f, "    leave");
                    outln(f, "    ret");
                    break;
                case IR_RETURN_VOID:
                    outln(f, "    movq $0, %%rax");
                    outln(f, "    leave");
                    outln(f, "    ret");
                    break;
                default:
                    break;
            }
        }
        outln(f, "    movq $0, %%rax");
        outln(f, "    leave");
        outln(f, "    ret");
        outln(f, "");
        free(sym);
    }

    if (str_count > 0) {
        outln(f, ".section .rodata");
        for (si = 0; si < str_count; si++) {
            const char *s = strs[si].value ? strs[si].value : "";
            size_t i;
            outln(f, "%s:", strs[si].label);
            fputs("    .asciz \"", f);
            for (i = 0; i < strlen(s); i++) {
                unsigned char c = (unsigned char)s[i];
                if (c == '\\' || c == '"') {
                    fputc('\\', f);
                    fputc(c, f);
                } else if (c == '\n') {
                    fputs("\\n", f);
                } else if (c == '\t') {
                    fputs("\\t", f);
                } else if (c < 32 || c > 126) {
                    fprintf(f, "\\%03o", c);
                } else {
                    fputc(c, f);
                }
            }
            fputs("\"\n", f);
        }
    }

    fclose(f);
    for (fi = 0; fi < fn_count; fi++) {
        int i;
        free(fns[fi].fn_label);
        for (i = 0; i < fns[fi].local_count; i++) {
            free(fns[fi].locals[i].name);
        }
        free(fns[fi].locals);
    }
    free(fns);
    for (si = 0; si < str_count; si++) {
        free(strs[si].value);
        free(strs[si].label);
    }
    free(strs);
    return 0;
}
