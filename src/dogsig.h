#ifndef DOGSCRIPT_DOGSIG_H
#define DOGSCRIPT_DOGSIG_H

/* Type signatures for native runtime entry points.

   The DogScript code generator is otherwise typeless: every value lives in an
   8-byte stack slot. Floating-point runtime functions, however, follow the
   System V ABI and pass/return `double` in the SSE registers. This table tells
   irgen what a runtime call returns and tells asmemit which arguments and
   return value must travel through xmm registers. Symbols not in the table are
   assumed to be entirely integer/pointer (GP registers), which is the common
   case (io, os, sys, json, time, list, ...). */

/* Returns 1 if the runtime symbol returns a double (in xmm0), else 0. */
int dog_sig_ret_is_float(const char *symbol);

/* Returns 1 if the runtime symbol returns a heap DogString* (whose first field
   is the char* payload), else 0. Used to normalize string comparisons. */
int dog_sig_ret_is_str(const char *symbol);

/* Returns 1 if argument `argidx` (0-based) of `symbol` is a double, else 0. */
int dog_sig_arg_is_float(const char *symbol, int argidx);

#endif
