#include "dog_runtime.h"

#include <dirent.h>
#include <errno.h>
#include <ftw.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

static DogValue *alloc_v(DogValue v) {
    DogValue *p = (DogValue *)malloc(sizeof(DogValue));
    if (!p) return NULL;
    *p = v;
    return p;
}

static DogString *read_file_full(const char *path, const char *mode) {
    FILE *f = fopen(path, mode);
    long sz;
    char *buf;
    if (!f) return dog_string_new("");
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return dog_string_new(""); }
    sz = ftell(f);
    if (sz < 0) { fclose(f); return dog_string_new(""); }
    rewind(f);
    buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return dog_string_new(""); }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(buf); return dog_string_new(""); }
    buf[sz] = '\0';
    fclose(f);
    return dog_string_new_len(buf, (size_t)sz);
}

DogString *dog_os_file_read(const char *path) { return read_file_full(path, "rb"); }

DogList *dog_os_file_readlines(const char *path) {
    FILE *f = fopen(path, "r");
    char line[4096];
    DogList *out = dog_list_new();
    if (!f) return out;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        dog_list_push(out, dog_string_new(line));
    }
    fclose(f);
    return out;
}

DogList *dog_os_file_readbytes(const char *path) {
    FILE *f = fopen(path, "rb");
    DogList *out = dog_list_new();
    int c;
    if (!f) return out;
    while ((c = fgetc(f)) != EOF) {
        dog_list_push(out, alloc_v(dog_v_int(c)));
    }
    fclose(f);
    return out;
}

DogString *dog_os_file_readchunk(const char *path, int n) {
    FILE *f = fopen(path, "rb");
    char *buf;
    size_t got;
    DogString *out;
    if (!f || n <= 0) return dog_string_new("");
    buf = (char *)malloc((size_t)n + 1);
    got = fread(buf, 1, (size_t)n, f);
    buf[got] = '\0';
    fclose(f);
    out = dog_string_new_len(buf, got);
    free(buf);
    return out;
}

int dog_os_file_write(const char *path, const char *content) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    if (content) fwrite(content, 1, strlen(content), f);
    fclose(f);
    return 1;
}

int dog_os_file_append(const char *path, const char *content) {
    FILE *f = fopen(path, "ab");
    if (!f) return 0;
    if (content) fwrite(content, 1, strlen(content), f);
    fclose(f);
    return 1;
}

int dog_os_file_writelines(const char *path, const DogList *lines) {
    size_t i;
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    if (lines) {
        for (i = 0; i < lines->len; i++) {
            DogString *s = (DogString *)lines->items[i];
            if (s) fwrite(s->data, 1, s->len, f);
            fputc('\n', f);
        }
    }
    fclose(f);
    return 1;
}

int dog_os_file_writebytes(const char *path, const DogList *bytes) {
    size_t i;
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    if (bytes) {
        for (i = 0; i < bytes->len; i++) {
            DogValue *v = (DogValue *)bytes->items[i];
            unsigned char c = (unsigned char)((v && v->type == DOG_INT) ? v->as.i : 0);
            fwrite(&c, 1, 1, f);
        }
    }
    fclose(f);
    return 1;
}

int dog_os_file_exists(const char *path) { return access(path, F_OK) == 0; }

int dog_os_file_isfile(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISREG(st.st_mode);
}

int dog_os_file_isdir(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

int dog_os_file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (int)st.st_size;
}

DogString *dog_os_file_ext(const char *path) {
    const char *dot = strrchr(path, '.');
    return dog_string_new(dot ? dot : "");
}

DogString *dog_os_file_basename(const char *path) {
    const char *slash = strrchr(path, '/');
    const char *base = slash ? slash + 1 : path;
    const char *dot = strrchr(base, '.');
    if (dot && dot > base) return dog_string_new_len(base, (size_t)(dot - base));
    return dog_string_new(base);
}

int dog_os_file_modified(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (int)st.st_mtime;
}

int dog_os_file_created(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
#if defined(__APPLE__)
    return (int)st.st_birthtimespec.tv_sec;
#else
    return (int)st.st_ctime;
#endif
}

int dog_os_file_copy(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    FILE *out = NULL;
    char buf[8192];
    size_t n;
    if (!in) return 0;
    out = fopen(dst, "wb");
    if (!out) { fclose(in); return 0; }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) { fclose(in); fclose(out); return 0; }
    }
    fclose(in);
    fclose(out);
    return 1;
}

int dog_os_file_move(const char *src, const char *dst) { return rename(src, dst) == 0; }
int dog_os_file_remove(const char *path) { return unlink(path) == 0; }

int dog_os_file_touch(const char *path) {
    FILE *f = fopen(path, "ab");
    if (!f) return 0;
    fclose(f);
    return 1;
}

DogString *dog_os_getcwd(void) {
    char buf[PATH_MAX];
    if (!getcwd(buf, sizeof(buf))) return dog_string_new("");
    return dog_string_new(buf);
}

int dog_os_chdir(const char *path) { return chdir(path) == 0; }
int dog_os_mkdir(const char *path) { return mkdir(path, 0755) == 0; }

int dog_os_makedirs(const char *path) {
    char tmp[PATH_MAX];
    char *p;
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(tmp, 0755) == 0 || errno == EEXIST;
}

int dog_os_rmdir(const char *path) { return rmdir(path) == 0; }

static int rm_tree_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    (void)sb;
    (void)ftwbuf;
    if (typeflag == FTW_DP) return rmdir(fpath);
    return unlink(fpath);
}

int dog_os_rmtree(const char *path) {
    return nftw(path, rm_tree_cb, 64, FTW_DEPTH | FTW_PHYS) == 0;
}

static DogList *listdir_kind(const char *path, int want_files, int want_dirs) {
    DIR *d = opendir(path);
    struct dirent *e;
    DogList *out = dog_list_new();
    if (!d) return out;
    while ((e = readdir(d))) {
        char full[PATH_MAX];
        struct stat st;
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        snprintf(full, sizeof(full), "%s/%s", path, e->d_name);
        if (stat(full, &st) != 0) continue;
        if ((want_files && S_ISREG(st.st_mode)) || (want_dirs && S_ISDIR(st.st_mode)) || (!want_files && !want_dirs)) {
            dog_list_push(out, dog_string_new(e->d_name));
        }
    }
    closedir(d);
    return out;
}

DogList *dog_os_listdir(const char *path) { return listdir_kind(path, 0, 0); }
DogList *dog_os_listfiles(const char *path) { return listdir_kind(path, 1, 0); }
DogList *dog_os_listdirs(const char *path) { return listdir_kind(path, 0, 1); }

static DogList *g_walk_out = NULL;
static int walk_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    DogMap *row;
    (void)sb;
    (void)ftwbuf;
    if (typeflag != FTW_D && typeflag != FTW_DP) return 0;
    row = dog_map_new();
    dog_map_set(row, "path", alloc_v(dog_v_str(fpath)));
    dog_map_set(row, "dirs", alloc_v(dog_v_list(dog_os_listdirs(fpath))));
    dog_map_set(row, "files", alloc_v(dog_v_list(dog_os_listfiles(fpath))));
    dog_list_push(g_walk_out, row);
    return 0;
}

DogList *dog_os_walk(const char *path) {
    g_walk_out = dog_list_new();
    nftw(path, walk_cb, 64, FTW_PHYS);
    return g_walk_out;
}

DogString *dog_os_path_join(const DogList *parts) {
    size_t i;
    size_t total = 0;
    char *out, *p;
    if (!parts || parts->len == 0) return dog_string_new("");
    for (i = 0; i < parts->len; i++) {
        DogString *s = (DogString *)parts->items[i];
        total += s ? s->len : 0;
        if (i + 1 < parts->len) total += 1;
    }
    out = (char *)malloc(total + 1);
    p = out;
    for (i = 0; i < parts->len; i++) {
        DogString *s = (DogString *)parts->items[i];
        if (s) { memcpy(p, s->data, s->len); p += s->len; }
        if (i + 1 < parts->len) *p++ = '/';
    }
    *p = '\0';
    return dog_string_new(out);
}

DogString *dog_os_path_abs(const char *path) {
    char buf[PATH_MAX];
    if (!realpath(path, buf)) return dog_string_new(path ? path : "");
    return dog_string_new(buf);
}

DogString *dog_os_path_parent(const char *path) {
    const char *slash = strrchr(path, '/');
    if (!slash) return dog_string_new(".");
    if (slash == path) return dog_string_new("/");
    return dog_string_new_len(path, (size_t)(slash - path));
}

DogList *dog_os_path_split(const char *path) {
    DogList *out = dog_list_new();
    const char *slash = strrchr(path, '/');
    if (!slash) {
        dog_list_push(out, dog_string_new("."));
        dog_list_push(out, dog_string_new(path));
    } else {
        dog_list_push(out, dog_string_new_len(path, (size_t)(slash - path)));
        dog_list_push(out, dog_string_new(slash + 1));
    }
    return out;
}

DogString *dog_os_path_normalize(const char *path) {
    return dog_os_path_abs(path);
}

DogString *dog_os_getenv(const char *key) {
    const char *v = getenv(key);
    return dog_string_new(v ? v : "");
}
int dog_os_setenv(const char *key, const char *val) { return setenv(key, val ? val : "", 1) == 0; }
int dog_os_unsetenv(const char *key) { return unsetenv(key) == 0; }

DogMap *dog_os_environ(void) {
    DogMap *m = dog_map_new();
    char **e = environ;
    while (e && *e) {
        char *eq = strchr(*e, '=');
        if (eq) {
            DogString *k = dog_string_new_len(*e, (size_t)(eq - *e));
            DogString *v = dog_string_new(eq + 1);
            dog_map_set(m, k->data, alloc_v(dog_v_str(v->data)));
            dog_string_free(k);
            dog_string_free(v);
        }
        e++;
    }
    return m;
}

DogString *dog_os_run(const char *cmd) {
    FILE *p = popen(cmd, "r");
    char chunk[1024];
    size_t cap = 4096, len = 0;
    char *buf = (char *)malloc(cap);
    if (!p) return dog_string_new("");
    while (fgets(chunk, sizeof(chunk), p)) {
        size_t n = strlen(chunk);
        if (len + n + 1 > cap) {
            while (len + n + 1 > cap) cap *= 2;
            buf = (char *)realloc(buf, cap);
        }
        memcpy(buf + len, chunk, n);
        len += n;
    }
    buf[len] = '\0';
    pclose(p);
    return dog_string_new(buf);
}

int dog_os_exec(const char *cmd) {
    int rc = system(cmd);
    if (rc == -1) return -1;
    return WEXITSTATUS(rc);
}

int dog_os_getpid(void) { return (int)getpid(); }

DogString *dog_os_name(void) {
    struct utsname un;
    if (uname(&un) != 0) return dog_string_new("unknown");
    return dog_string_new(un.sysname);
}

DogString *dog_os_hostname(void) {
    char buf[256];
    if (gethostname(buf, sizeof(buf)) != 0) return dog_string_new("");
    buf[sizeof(buf) - 1] = '\0';
    return dog_string_new(buf);
}

DogString *dog_os_username(void) {
    struct passwd *pw = getpwuid(getuid());
    if (!pw) return dog_string_new("");
    return dog_string_new(pw->pw_name);
}

DogString *dog_os_homedir(void) {
    const char *h = getenv("HOME");
    if (h) return dog_string_new(h);
    return dog_string_new("");
}

DogString *dog_os_tmpdir(void) {
    const char *t = getenv("TMPDIR");
    if (t) return dog_string_new(t);
    return dog_string_new("/tmp");
}
