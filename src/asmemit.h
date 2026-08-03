#ifndef DOGSCRIPT_ASMEMIT_H
#define DOGSCRIPT_ASMEMIT_H

#include "ir.h"

int asmemit_x64(const IRProgram *ir, const char *asm_path);

#endif
