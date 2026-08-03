#include "dog_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void dog_io_print(const char *msg) {
    fputs(msg ? msg : "", stdout);
    fflush(stdout);
}

void dog_io_println(const char *msg) {
    fputs(msg ? msg : "", stdout);
    fputc('\n', stdout);
    fflush(stdout);
}

DogString *dog_io_input(const char *prompt) {
    char buf[4096];
    if (prompt) {
        fputs(prompt, stdout);
        fflush(stdout);
    }
    if (!fgets(buf, sizeof(buf), stdin)) {
        return dog_string_new("");
    }
    buf[strcspn(buf, "\n")] = '\0';
    return dog_string_new(buf);
}

void dog_io_clear(void) {
    printf("\033[2J\033[H");
    fflush(stdout);
}

void dog_io_color(int code) {
    printf("\033[%dm", code);
    fflush(stdout);
}

DogString *dog_io_bold(const char *msg) {
    size_t n = strlen(msg ? msg : "");
    char *out = (char *)malloc(n + 9);
    sprintf(out, "\033[1m%s\033[0m", msg ? msg : "");
    {
        DogString *ret = dog_string_new(out);
        free(out);
        return ret;
    }
}

DogString *dog_io_underline(const char *msg) {
    size_t n = strlen(msg ? msg : "");
    char *out = (char *)malloc(n + 9);
    sprintf(out, "\033[4m%s\033[0m", msg ? msg : "");
    {
        DogString *ret = dog_string_new(out);
        free(out);
        return ret;
    }
}

void dog_io_table(const DogList *headers, const DogList *rows) {
    size_t i, j, cols = headers ? headers->len : 0;
    size_t *widths;
    if (!headers || cols == 0) return;
    widths = (size_t *)calloc(cols, sizeof(size_t));
    for (i = 0; i < cols; i++) {
        DogString *h = (DogString *)headers->items[i];
        widths[i] = h ? h->len : 0;
    }
    if (rows) {
        for (i = 0; i < rows->len; i++) {
            DogList *row = (DogList *)rows->items[i];
            for (j = 0; row && j < row->len && j < cols; j++) {
                DogString *c = (DogString *)row->items[j];
                if (c && c->len > widths[j]) widths[j] = c->len;
            }
        }
    }
    for (j = 0; j < cols; j++) {
        DogString *h = (DogString *)headers->items[j];
        printf("| %-*s ", (int)widths[j], h ? h->data : "");
    }
    printf("|\n");
    for (j = 0; j < cols; j++) {
        size_t k;
        printf("|");
        for (k = 0; k < widths[j] + 2; k++) printf("-");
    }
    printf("|\n");
    if (rows) {
        for (i = 0; i < rows->len; i++) {
            DogList *row = (DogList *)rows->items[i];
            for (j = 0; j < cols; j++) {
                DogString *c = (row && j < row->len) ? (DogString *)row->items[j] : NULL;
                printf("| %-*s ", (int)widths[j], c ? c->data : "");
            }
            printf("|\n");
        }
    }
    free(widths);
}

void dog_io_progress(int current, int total, const char *label) {
    int width = 30;
    int filled, i;
    if (total <= 0) total = 1;
    if (current < 0) current = 0;
    if (current > total) current = total;
    filled = (current * width) / total;
    printf("\r%s [", label ? label : "progress");
    for (i = 0; i < width; i++) {
        putchar(i < filled ? '#' : '-');
    }
    printf("] %d%%", (current * 100) / total);
    if (current >= total) putchar('\n');
    fflush(stdout);
}
