#include "dog_runtime.h"

#include <stdlib.h>
#include <string.h>

static int value_equal(void *a, void *b) {
    return a == b;
}

DogList *dog_list_append(DogList *l, void *item) {
    if (!l) l = dog_list_new();
    dog_list_push(l, item);
    return l;
}

DogList *dog_list_prepend(DogList *l, void *item) {
    size_t i;
    if (!l) l = dog_list_new();
    if (l->len == l->cap) {
        l->cap *= 2;
        l->items = (void **)realloc(l->items, sizeof(void *) * l->cap);
    }
    for (i = l->len; i > 0; i--) {
        l->items[i] = l->items[i - 1];
    }
    l->items[0] = item;
    l->len++;
    return l;
}

void *dog_list_pop(DogList *l) {
    if (!l || l->len == 0) return NULL;
    return l->items[--l->len];
}

DogList *dog_list_remove(DogList *l, void *item) {
    size_t i, w = 0;
    if (!l) return NULL;
    for (i = 0; i < l->len; i++) {
        if (!value_equal(l->items[i], item)) {
            l->items[w++] = l->items[i];
        }
    }
    l->len = w;
    return l;
}

int dog_list_index(const DogList *l, void *item) {
    size_t i;
    if (!l) return -1;
    for (i = 0; i < l->len; i++) {
        if (value_equal(l->items[i], item)) return (int)i;
    }
    return -1;
}

int dog_list_contains(const DogList *l, void *item) {
    return dog_list_index(l, item) >= 0;
}

int dog_list_len(const DogList *l) {
    return l ? (int)l->len : 0;
}

DogList *dog_list_reverse(const DogList *l) {
    size_t i;
    DogList *out = dog_list_new();
    if (!l || !out) return out;
    for (i = l->len; i > 0; i--) {
        dog_list_push(out, l->items[i - 1]);
    }
    return out;
}

DogList *dog_list_sort(const DogList *l) {
    size_t i, j;
    DogList *out = dog_list_new();
    if (!l || !out) return out;
    for (i = 0; i < l->len; i++) dog_list_push(out, l->items[i]);
    for (i = 0; i < out->len; i++) {
        for (j = i + 1; j < out->len; j++) {
            if (out->items[i] > out->items[j]) {
                void *tmp = out->items[i];
                out->items[i] = out->items[j];
                out->items[j] = tmp;
            }
        }
    }
    return out;
}

DogList *dog_list_slice(const DogList *l, int start, int end) {
    int i, n;
    DogList *out = dog_list_new();
    if (!l || !out) return out;
    n = (int)l->len;
    if (start < 0) start = n + start;
    if (end < 0) end = n + end;
    if (start < 0) start = 0;
    if (end > n) end = n;
    if (end < start) end = start;
    for (i = start; i < end; i++) {
        dog_list_push(out, l->items[i]);
    }
    return out;
}

DogList *dog_list_concat(const DogList *a, const DogList *b) {
    size_t i;
    DogList *out = dog_list_new();
    if (!out) return NULL;
    if (a) for (i = 0; i < a->len; i++) dog_list_push(out, a->items[i]);
    if (b) for (i = 0; i < b->len; i++) dog_list_push(out, b->items[i]);
    return out;
}

DogList *dog_list_unique(const DogList *l) {
    size_t i, j;
    DogList *out = dog_list_new();
    if (!l || !out) return out;
    for (i = 0; i < l->len; i++) {
        int seen = 0;
        for (j = 0; j < out->len; j++) {
            if (value_equal(out->items[j], l->items[i])) {
                seen = 1;
                break;
            }
        }
        if (!seen) dog_list_push(out, l->items[i]);
    }
    return out;
}

DogList *dog_list_flatten(const DogList *l) {
    size_t i, j;
    DogList *out = dog_list_new();
    if (!l || !out) return out;
    for (i = 0; i < l->len; i++) {
        DogList *sub = (DogList *)l->items[i];
        if (sub) {
            for (j = 0; j < sub->len; j++) dog_list_push(out, sub->items[j]);
        }
    }
    return out;
}

DogList *dog_list_map(const DogList *l, void *fn) {
    (void)fn;
    if (!l) return dog_list_new();
    return dog_list_concat(l, NULL);
}

DogList *dog_list_filter(const DogList *l, void *fn) {
    (void)fn;
    if (!l) return dog_list_new();
    return dog_list_concat(l, NULL);
}

void *dog_list_reduce(const DogList *l, void *fn, void *init) {
    (void)l;
    (void)fn;
    return init;
}
