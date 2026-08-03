#include "dogsig.h"

#include <string.h>

/* ret: return-value class. arg_float is a bitmask: bit i set => arg i is double. */
#define RET_INT 0
#define RET_FLOAT 1
#define RET_STR 2

typedef struct {
    const char *symbol;
    int ret;
    unsigned arg_float;
} DogSig;

#define A0 0u
#define A1 (1u << 0)
#define A2 (1u << 1)
#define A3 (1u << 2)

static const DogSig g_sigs[] = {
    {"dog_math_sqrt",      RET_FLOAT, A1},
    {"dog_math_pow",       RET_FLOAT, A1 | A2},
    {"dog_math_abs",       RET_FLOAT, A1},
    {"dog_math_floor",     RET_INT,   A1},
    {"dog_math_ceil",      RET_INT,   A1},
    {"dog_math_round",     RET_INT,   A1},
    {"dog_math_log",       RET_FLOAT, A1},
    {"dog_math_log2",      RET_FLOAT, A1},
    {"dog_math_log10",     RET_FLOAT, A1},
    {"dog_math_sin",       RET_FLOAT, A1},
    {"dog_math_cos",       RET_FLOAT, A1},
    {"dog_math_tan",       RET_FLOAT, A1},
    {"dog_math_pi",        RET_FLOAT, A0},
    {"dog_math_e",         RET_FLOAT, A0},
    {"dog_math_rand",      RET_FLOAT, A0},
    {"dog_math_randint",   RET_INT,   A0},
    {"dog_math_min",       RET_FLOAT, A1 | A2},
    {"dog_math_max",       RET_FLOAT, A1 | A2},
    {"dog_math_clamp",     RET_FLOAT, A1 | A2 | A3},

    /* string-producing helpers (return DogString*) */
    {"dog_str_upper",      RET_STR,   A0},
    {"dog_str_lower",      RET_STR,   A0},
    {"dog_str_trim",       RET_STR,   A0},
    {"dog_str_trimleft",   RET_STR,   A0},
    {"dog_str_trimright",  RET_STR,   A0},
    {"dog_str_replace",    RET_STR,   A0},
    {"dog_str_join",       RET_STR,   A0},
};

static const DogSig *find_sig(const char *symbol) {
    size_t i;
    if (!symbol) {
        return NULL;
    }
    for (i = 0; i < sizeof(g_sigs) / sizeof(g_sigs[0]); i++) {
        if (strcmp(g_sigs[i].symbol, symbol) == 0) {
            return &g_sigs[i];
        }
    }
    return NULL;
}

int dog_sig_ret_is_float(const char *symbol) {
    const DogSig *s = find_sig(symbol);
    return (s && s->ret == RET_FLOAT) ? 1 : 0;
}

int dog_sig_ret_is_str(const char *symbol) {
    const DogSig *s = find_sig(symbol);
    return (s && s->ret == RET_STR) ? 1 : 0;
}

int dog_sig_arg_is_float(const char *symbol, int argidx) {
    const DogSig *s = find_sig(symbol);
    if (!s || argidx < 0 || argidx >= 32) {
        return 0;
    }
    return (s->arg_float & (1u << argidx)) ? 1 : 0;
}
