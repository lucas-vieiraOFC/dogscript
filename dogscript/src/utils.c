#include "utils.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *ds_read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    char *buf;
    long sz;
    size_t read_n;
    if (!f) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    buf = (char *)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    read_n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (read_n != (size_t)sz) {
        free(buf);
        return NULL;
    }
    buf[sz] = '\0';
    if (out_len) {
        *out_len = (size_t)sz;
    }
    return buf;
}

int ds_write_file(const char *path, const char *data) {
    FILE *f = fopen(path, "wb");
    size_t n;
    if (!f) {
        return -1;
    }
    n = fwrite(data, 1, strlen(data), f);
    fclose(f);
    return n == strlen(data) ? 0 : -1;
}

char *ds_strdup(const char *src) {
    size_t n = strlen(src);
    char *out = (char *)malloc(n + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, src, n + 1);
    return out;
}

char *ds_strndup(const char *src, size_t n) {
    char *out = (char *)malloc(n + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, src, n);
    out[n] = '\0';
    return out;
}

char *ds_path_basename_no_ext(const char *path) {
    const char *slash = strrchr(path, '/');
    const char *base = slash ? slash + 1 : path;
    const char *dot = strrchr(base, '.');
    if (dot && dot > base) {
        return ds_strndup(base, (size_t)(dot - base));
    }
    return ds_strdup(base);
}

char *ds_replace_ext(const char *path, const char *new_ext) {
    const char *dot = strrchr(path, '.');
    size_t base_n = dot ? (size_t)(dot - path) : strlen(path);
    size_t ext_n = strlen(new_ext);
    char *out = (char *)malloc(base_n + ext_n + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, path, base_n);
    memcpy(out + base_n, new_ext, ext_n + 1);
    return out;
}

char *ds_join3(const char *a, const char *b, const char *c) {
    size_t na = strlen(a), nb = strlen(b), nc = strlen(c);
    char *out = (char *)malloc(na + nb + nc + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, a, na);
    memcpy(out + na, b, nb);
    memcpy(out + na + nb, c, nc + 1);
    return out;
}

void ds_fatal(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
    exit(1);
}
