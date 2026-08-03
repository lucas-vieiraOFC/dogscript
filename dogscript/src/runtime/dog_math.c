#include "dog_runtime.h"

#include <math.h>
#include <stdlib.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_E
#define M_E 2.71828182845904523536
#endif

static int g_seeded = 0;

static void dog_math_seed_once(void) {
    if (!g_seeded) {
        srand((unsigned int)time(NULL));
        g_seeded = 1;
    }
}

double dog_math_sqrt(double x) { return sqrt(x); }
double dog_math_pow(double base, double exp) { return pow(base, exp); }
double dog_math_abs(double x) { return fabs(x); }
int dog_math_floor(double x) { return (int)floor(x); }
int dog_math_ceil(double x) { return (int)ceil(x); }
int dog_math_round(double x) { return (int)llround(x); }
double dog_math_log(double x) { return log(x); }
double dog_math_log2(double x) { return log2(x); }
double dog_math_log10(double x) { return log10(x); }
double dog_math_sin(double x) { return sin(x); }
double dog_math_cos(double x) { return cos(x); }
double dog_math_tan(double x) { return tan(x); }
double dog_math_pi(void) { return M_PI; }
double dog_math_e(void) { return M_E; }

double dog_math_rand(void) {
    dog_math_seed_once();
    return (double)rand() / (double)RAND_MAX;
}

int dog_math_randint(int min, int max) {
    int span;
    dog_math_seed_once();
    if (max < min) {
        int t = min;
        min = max;
        max = t;
    }
    span = (max - min) + 1;
    return min + (rand() % span);
}

double dog_math_min(double a, double b) { return a < b ? a : b; }
double dog_math_max(double a, double b) { return a > b ? a : b; }

double dog_math_clamp(double val, double min, double max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}
