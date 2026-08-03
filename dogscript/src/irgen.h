#ifndef DOGSCRIPT_IRGEN_H
#define DOGSCRIPT_IRGEN_H

#include "ir.h"
#include "parser.h"

int irgen_program(const Program *program, IRProgram *out);

#endif
