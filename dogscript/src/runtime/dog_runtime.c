#include "dog_runtime.h"

#include <stdlib.h>
#include <string.h>

long dog_argc = 0;
char **dog_argv = NULL;

char *dog_strdup(const char *s) {
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

char *dog_strndup(const char *s, size_t n) {
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

DogString *dog_string_new(const char *src) {
    return dog_string_new_len(src ? src : "", src ? strlen(src) : 0);
}

DogString *dog_string_new_len(const char *src, size_t len) {
    DogString *s = (DogString *)calloc(1, sizeof(DogString));
    if (!s) return NULL;
    s->data = dog_strndup(src ? src : "", len);
    if (!s->data) {
        free(s);
        return NULL;
    }
    s->len = len;
    return s;
}

void dog_string_free(DogString *s) {
    if (!s) return;
    free(s->data);
    free(s);
}

DogList *dog_list_new(void) {
    DogList *l = (DogList *)calloc(1, sizeof(DogList));
    if (!l) return NULL;
    l->cap = 8;
    l->items = (void **)calloc(l->cap, sizeof(void *));
    if (!l->items) {
        free(l);
        return NULL;
    }
    return l;
}

void dog_list_free(DogList *l, int free_items) {
    size_t i;
    if (!l) return;
    if (free_items) {
        for (i = 0; i < l->len; i++) {
            free(l->items[i]);
        }
    }
    free(l->items);
    free(l);
}

int dog_list_push(DogList *l, void *item) {
    void **new_items;
    if (!l) return 0;
    if (l->len == l->cap) {
        l->cap *= 2;
        new_items = (void **)realloc(l->items, sizeof(void *) * l->cap);
        if (!new_items) return 0;
        l->items = new_items;
    }
    l->items[l->len++] = item;
    return 1;
}

void *dog_list_get(const DogList *l, size_t idx) {
    if (!l || idx >= l->len) return NULL;
    return l->items[idx];
}

DogMap *dog_map_new(void) {
    DogMap *m = (DogMap *)calloc(1, sizeof(DogMap));
    if (!m) return NULL;
    m->cap = 8;
    m->keys = (DogString **)calloc(m->cap, sizeof(DogString *));
    m->values = (void **)calloc(m->cap, sizeof(void *));
    if (!m->keys || !m->values) {
        free(m->keys);
        free(m->values);
        free(m);
        return NULL;
    }
    return m;
}

static int dog_map_find(const DogMap *m, const char *key) {
    size_t i;
    if (!m || !key) return -1;
    for (i = 0; i < m->len; i++) {
        if (strcmp(m->keys[i]->data, key) == 0) {
            return (int)i;
        }
    }
    return -1;
}

int dog_map_set(DogMap *m, const char *key, void *value) {
    int idx;
    DogString **new_keys;
    void **new_vals;
    if (!m || !key) return 0;
    idx = dog_map_find(m, key);
    if (idx >= 0) {
        m->values[idx] = value;
        return 1;
    }
    if (m->len == m->cap) {
        m->cap *= 2;
        new_keys = (DogString **)realloc(m->keys, sizeof(DogString *) * m->cap);
        new_vals = (void **)realloc(m->values, sizeof(void *) * m->cap);
        if (!new_keys || !new_vals) return 0;
        m->keys = new_keys;
        m->values = new_vals;
    }
    m->keys[m->len] = dog_string_new(key);
    if (!m->keys[m->len]) return 0;
    m->values[m->len] = value;
    m->len++;
    return 1;
}

void *dog_map_get(const DogMap *m, const char *key) {
    int idx = dog_map_find(m, key);
    if (idx < 0) return NULL;
    return m->values[idx];
}

DogString *dog_map_get_key(const DogMap *m, size_t idx) {
    if (!m || idx >= m->len) return NULL;
    return m->keys[idx];
}

void *dog_map_get_value(const DogMap *m, size_t idx) {
    if (!m || idx >= m->len) return NULL;
    return m->values[idx];
}

void dog_map_free(DogMap *m, int free_values) {
    size_t i;
    if (!m) return;
    for (i = 0; i < m->len; i++) {
        dog_string_free(m->keys[i]);
        if (free_values) {
            free(m->values[i]);
        }
    }
    free(m->keys);
    free(m->values);
    free(m);
}

DogValue dog_v_int(int v) {
    DogValue out;
    out.type = DOG_INT;
    out.as.i = v;
    return out;
}

DogValue dog_v_float(double v) {
    DogValue out;
    out.type = DOG_FLOAT;
    out.as.f = v;
    return out;
}

DogValue dog_v_str(const char *v) {
    DogValue out;
    out.type = DOG_STR;
    out.as.s = dog_string_new(v ? v : "");
    return out;
}

DogValue dog_v_bool(int v) {
    DogValue out;
    out.type = DOG_BOOL;
    out.as.b = v ? 1 : 0;
    return out;
}

DogValue dog_v_list(DogList *v) {
    DogValue out;
    out.type = DOG_LIST;
    out.as.l = v;
    return out;
}

DogValue dog_v_map(DogMap *v) {
    DogValue out;
    out.type = DOG_MAP;
    out.as.m = v;
    return out;
}

DogValue dog_v_null(void) {
    DogValue out;
    out.type = DOG_NULL;
    out.as.p = NULL;
    return out;
}

void dog_runtime_init(void) {
}

void dog_runtime_cleanup(void) {
}
