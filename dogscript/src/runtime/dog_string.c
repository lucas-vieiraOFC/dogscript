#include "dog_runtime.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dup_or_empty(const char *s) {
    return dog_strdup(s ? s : "");
}

DogString *dog_str_upper(const char *s) {
    size_t i, n = s ? strlen(s) : 0;
    char *out = (char *)malloc(n + 1);
    for (i = 0; i < n; i++) out[i] = (char)toupper((unsigned char)s[i]);
    out[n] = '\0';
    {
        DogString *ret = dog_string_new_len(out, n);
        free(out);
        return ret;
    }
}

DogString *dog_str_lower(const char *s) {
    size_t i, n = s ? strlen(s) : 0;
    char *out = (char *)malloc(n + 1);
    for (i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)s[i]);
    out[n] = '\0';
    {
        DogString *ret = dog_string_new_len(out, n);
        free(out);
        return ret;
    }
}

static char *trim_left_ptr(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

DogString *dog_str_trimleft(const char *s) {
    char *dup = dup_or_empty(s);
    char *p = trim_left_ptr(dup);
    DogString *out = dog_string_new(p);
    free(dup);
    return out;
}

DogString *dog_str_trimright(const char *s) {
    size_t n;
    char *dup = dup_or_empty(s);
    n = strlen(dup);
    while (n > 0 && isspace((unsigned char)dup[n - 1])) {
        dup[n - 1] = '\0';
        n--;
    }
    return dog_string_new(dup);
}

DogString *dog_str_trim(const char *s) {
    DogString *left = dog_str_trimleft(s);
    DogString *right = dog_str_trimright(left->data);
    dog_string_free(left);
    return right;
}

DogList *dog_str_split(const char *s, const char *delim) {
    char *copy;
    char *tok;
    size_t dlen;
    DogList *l = dog_list_new();
    if (!l) return NULL;
    if (!s || !*s) return l;
    if (!delim || !*delim) {
        dog_list_push(l, dog_string_new(s));
        return l;
    }
    dlen = strlen(delim);
    copy = dog_strdup(s);
    tok = copy;
    while (tok) {
        char *next = strstr(tok, delim);
        if (!next) {
            dog_list_push(l, dog_string_new(tok));
            break;
        }
        *next = '\0';
        dog_list_push(l, dog_string_new(tok));
        tok = next + dlen;
    }
    free(copy);
    return l;
}

DogString *dog_str_join(const DogList *parts, const char *delim) {
    size_t i, dlen = delim ? strlen(delim) : 0, total = 0;
    char *out, *p;
    if (!parts || parts->len == 0) return dog_string_new("");
    for (i = 0; i < parts->len; i++) {
        DogString *s = (DogString *)parts->items[i];
        total += s ? s->len : 0;
        if (i + 1 < parts->len) total += dlen;
    }
    out = (char *)malloc(total + 1);
    p = out;
    for (i = 0; i < parts->len; i++) {
        DogString *s = (DogString *)parts->items[i];
        if (s && s->data) {
            memcpy(p, s->data, s->len);
            p += s->len;
        }
        if (i + 1 < parts->len && dlen > 0) {
            memcpy(p, delim, dlen);
            p += dlen;
        }
    }
    *p = '\0';
    {
        DogString *ret = dog_string_new_len(out, total);
        free(out);
        return ret;
    }
}

DogString *dog_str_replace(const char *s, const char *old, const char *newv) {
    const char *cur;
    size_t oldn, newn, count = 0, outn;
    char *out, *p;
    if (!s || !old || !*old) return dog_string_new(s ? s : "");
    oldn = strlen(old);
    newn = newv ? strlen(newv) : 0;
    cur = s;
    while ((cur = strstr(cur, old))) {
        count++;
        cur += oldn;
    }
    outn = strlen(s) + count * (newn - oldn);
    out = (char *)malloc(outn + 1);
    p = out;
    cur = s;
    while (*cur) {
        const char *m = strstr(cur, old);
        if (!m) {
            strcpy(p, cur);
            break;
        }
        memcpy(p, cur, (size_t)(m - cur));
        p += (m - cur);
        if (newv) {
            memcpy(p, newv, newn);
            p += newn;
        }
        cur = m + oldn;
    }
    out[outn] = '\0';
    return dog_string_new_len(out, outn);
}

int dog_str_contains(const char *s, const char *sub) { return (s && sub && strstr(s, sub)) ? 1 : 0; }
int dog_str_startswith(const char *s, const char *prefix) { return (s && prefix && strncmp(s, prefix, strlen(prefix)) == 0) ? 1 : 0; }
int dog_str_endswith(const char *s, const char *suffix) {
    size_t ns, ne;
    if (!s || !suffix) return 0;
    ns = strlen(s);
    ne = strlen(suffix);
    if (ne > ns) return 0;
    return strcmp(s + ns - ne, suffix) == 0;
}
int dog_str_len(const char *s) { return (int)(s ? strlen(s) : 0); }

/* Value equality for strings. Both operands are plain C strings (a DogString*
   is normalized to its char* payload by the code generator before calling). */
int dog_str_equal(const char *a, const char *b) {
    if (!a || !b) {
        return a == b;
    }
    return strcmp(a, b) == 0 ? 1 : 0;
}

int dog_str_index(const char *s, const char *sub) {
    const char *p;
    if (!s || !sub) return -1;
    p = strstr(s, sub);
    if (!p) return -1;
    return (int)(p - s);
}

DogString *dog_str_slice(const char *s, int start, int end) {
    size_t n = s ? strlen(s) : 0;
    if (start < 0) start = (int)n + start;
    if (end < 0) end = (int)n + end;
    if (start < 0) start = 0;
    if (end > (int)n) end = (int)n;
    if (end < start) end = start;
    return dog_string_new_len(s + start, (size_t)(end - start));
}

DogString *dog_str_repeat(const char *s, int n) {
    size_t i, sn = s ? strlen(s) : 0;
    size_t total;
    char *out, *p;
    if (n <= 0 || sn == 0) return dog_string_new("");
    total = sn * (size_t)n;
    out = (char *)malloc(total + 1);
    p = out;
    for (i = 0; i < (size_t)n; i++) {
        memcpy(p, s, sn);
        p += sn;
    }
    out[total] = '\0';
    return dog_string_new_len(out, total);
}

DogString *dog_str_reverse(const char *s) {
    size_t i, n = s ? strlen(s) : 0;
    char *out = (char *)malloc(n + 1);
    for (i = 0; i < n; i++) out[i] = s[n - i - 1];
    out[n] = '\0';
    {
        DogString *ret = dog_string_new_len(out, n);
        free(out);
        return ret;
    }
}

DogString *dog_str_pad(const char *s, int width, const char *ch) {
    size_t n = s ? strlen(s) : 0;
    size_t cn = (ch && *ch) ? strlen(ch) : 1;
    const char *c = (ch && *ch) ? ch : " ";
    size_t target = (size_t)(width > 0 ? width : 0), i;
    char *out, *p;
    if (n >= target) return dog_string_new(s ? s : "");
    out = (char *)malloc(target + 1);
    memcpy(out, s ? s : "", n);
    p = out + n;
    for (i = n; i < target; i++) {
        *p++ = c[(i - n) % cn];
    }
    *p = '\0';
    {
        DogString *ret = dog_string_new_len(out, target);
        free(out);
        return ret;
    }
}

int dog_str_count(const char *s, const char *sub) {
    int c = 0;
    size_t n;
    if (!s || !sub || !*sub) return 0;
    n = strlen(sub);
    while ((s = strstr(s, sub))) {
        c++;
        s += n;
    }
    return c;
}

int dog_str_isdigit(const char *s) {
    if (!s || !*s) return 0;
    while (*s) {
        if (!isdigit((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

int dog_str_isalpha(const char *s) {
    if (!s || !*s) return 0;
    while (*s) {
        if (!isalpha((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}
