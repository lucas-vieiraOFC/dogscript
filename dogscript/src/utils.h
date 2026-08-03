#ifndef DOGSCRIPT_UTILS_H
#define DOGSCRIPT_UTILS_H

#include <stddef.h>

char *ds_read_file(const char *path, size_t *out_len);
int ds_write_file(const char *path, const char *data);
char *ds_strdup(const char *src);
char *ds_strndup(const char *src, size_t n);
char *ds_path_basename_no_ext(const char *path);
char *ds_replace_ext(const char *path, const char *new_ext);
char *ds_join3(const char *a, const char *b, const char *c);
void ds_fatal(const char *fmt, ...);

#endif
