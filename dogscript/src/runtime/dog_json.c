#include "dog_runtime.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *s;
    size_t pos;
} JsonP;

static void jp_ws(JsonP *p) {
    while (isspace((unsigned char)p->s[p->pos])) p->pos++;
}

static int jp_peek(JsonP *p) { return p->s[p->pos]; }
static int jp_take(JsonP *p) { return p->s[p->pos] ? p->s[p->pos++] : 0; }

static DogValue *json_parse_value(JsonP *p);

static DogString *json_parse_string_raw(JsonP *p) {
    char *buf;
    size_t cap = 64, len = 0;
    if (jp_take(p) != '"') return NULL;
    buf = (char *)malloc(cap);
    while (jp_peek(p) && jp_peek(p) != '"') {
        int c = jp_take(p);
        if (c == '\\') {
            c = jp_take(p);
            if (c == 'n') c = '\n';
            else if (c == 't') c = '\t';
            else if (c == 'r') c = '\r';
            else if (c == '"' || c == '\\' || c == '/') {}
            else c = '?';
        }
        if (len + 2 > cap) {
            cap *= 2;
            buf = (char *)realloc(buf, cap);
        }
        buf[len++] = (char)c;
    }
    if (jp_take(p) != '"') { free(buf); return NULL; }
    buf[len] = '\0';
    {
        DogString *out = dog_string_new_len(buf, len);
        free(buf);
        return out;
    }
}

static DogValue *json_parse_number(JsonP *p) {
    size_t start = p->pos;
    while (strchr("-+0123456789.eE", jp_peek(p))) p->pos++;
    {
        size_t n = p->pos - start;
        char *tmp = dog_strndup(p->s + start, n);
        DogValue *v = (DogValue *)malloc(sizeof(DogValue));
        if (strchr(tmp, '.') || strchr(tmp, 'e') || strchr(tmp, 'E')) *v = dog_v_float(strtod(tmp, NULL));
        else *v = dog_v_int((int)strtol(tmp, NULL, 10));
        free(tmp);
        return v;
    }
}

static DogValue *json_parse_array(JsonP *p) {
    DogList *arr = dog_list_new();
    DogValue *v = (DogValue *)malloc(sizeof(DogValue));
    jp_take(p);
    jp_ws(p);
    if (jp_peek(p) == ']') { jp_take(p); *v = dog_v_list(arr); return v; }
    while (jp_peek(p)) {
        DogValue *item = json_parse_value(p);
        if (!item) return NULL;
        dog_list_push(arr, item);
        jp_ws(p);
        if (jp_peek(p) == ',') { jp_take(p); jp_ws(p); continue; }
        if (jp_peek(p) == ']') { jp_take(p); break; }
        return NULL;
    }
    *v = dog_v_list(arr);
    return v;
}

static DogValue *json_parse_object_val(JsonP *p) {
    DogMap *obj = dog_map_new();
    DogValue *v = (DogValue *)malloc(sizeof(DogValue));
    jp_take(p);
    jp_ws(p);
    if (jp_peek(p) == '}') { jp_take(p); *v = dog_v_map(obj); return v; }
    while (jp_peek(p)) {
        DogString *k;
        DogValue *item;
        jp_ws(p);
        k = json_parse_string_raw(p);
        if (!k) return NULL;
        jp_ws(p);
        if (jp_take(p) != ':') return NULL;
        jp_ws(p);
        item = json_parse_value(p);
        if (!item) return NULL;
        dog_map_set(obj, k->data, item);
        dog_string_free(k);
        jp_ws(p);
        if (jp_peek(p) == ',') { jp_take(p); jp_ws(p); continue; }
        if (jp_peek(p) == '}') { jp_take(p); break; }
        return NULL;
    }
    *v = dog_v_map(obj);
    return v;
}

static DogValue *json_parse_value(JsonP *p) {
    jp_ws(p);
    if (jp_peek(p) == '"') {
        DogString *s = json_parse_string_raw(p);
        DogValue *v = (DogValue *)malloc(sizeof(DogValue));
        *v = dog_v_null();
        v->type = DOG_STR;
        v->as.s = s;
        return v;
    }
    if (jp_peek(p) == '{') return json_parse_object_val(p);
    if (jp_peek(p) == '[') return json_parse_array(p);
    if (!strncmp(p->s + p->pos, "true", 4)) { p->pos += 4; { DogValue *v = (DogValue *)malloc(sizeof(DogValue)); *v = dog_v_bool(1); return v; } }
    if (!strncmp(p->s + p->pos, "false", 5)) { p->pos += 5; { DogValue *v = (DogValue *)malloc(sizeof(DogValue)); *v = dog_v_bool(0); return v; } }
    if (!strncmp(p->s + p->pos, "null", 4)) { p->pos += 4; { DogValue *v = (DogValue *)malloc(sizeof(DogValue)); *v = dog_v_null(); return v; } }
    return json_parse_number(p);
}

DogMap *dog_json_parse(const char *input) {
    JsonP p;
    DogValue *v;
    p.s = input ? input : "";
    p.pos = 0;
    v = json_parse_value(&p);
    if (!v) return dog_map_new();
    if (v->type == DOG_MAP) return v->as.m;
    {
        DogMap *m = dog_map_new();
        dog_map_set(m, "value", v);
        return m;
    }
}

static void json_emit_value(const DogValue *v, int pretty, int depth, FILE *f);

static void emit_indent(int n, FILE *f) {
    int i;
    for (i = 0; i < n; i++) fputs("  ", f);
}

static void json_emit_string(const char *s, FILE *f) {
    fputc('"', f);
    while (s && *s) {
        if (*s == '"' || *s == '\\') fputc('\\', f);
        if (*s == '\n') { fputs("\\n", f); s++; continue; }
        fputc(*s++, f);
    }
    fputc('"', f);
}

static void json_emit_map(const DogMap *m, int pretty, int depth, FILE *f) {
    size_t i;
    fputc('{', f);
    if (pretty && m && m->len) fputc('\n', f);
    for (i = 0; m && i < m->len; i++) {
        DogString *k = m->keys[i];
        DogValue *v = (DogValue *)m->values[i];
        if (pretty) emit_indent(depth + 1, f);
        json_emit_string(k ? k->data : "", f);
        fputc(':', f);
        if (pretty) fputc(' ', f);
        json_emit_value(v, pretty, depth + 1, f);
        if (i + 1 < m->len) fputc(',', f);
        if (pretty) fputc('\n', f);
    }
    if (pretty && m && m->len) emit_indent(depth, f);
    fputc('}', f);
}

static void json_emit_list(const DogList *l, int pretty, int depth, FILE *f) {
    size_t i;
    fputc('[', f);
    if (pretty && l && l->len) fputc('\n', f);
    for (i = 0; l && i < l->len; i++) {
        DogValue *v = (DogValue *)l->items[i];
        if (pretty) emit_indent(depth + 1, f);
        json_emit_value(v, pretty, depth + 1, f);
        if (i + 1 < l->len) fputc(',', f);
        if (pretty) fputc('\n', f);
    }
    if (pretty && l && l->len) emit_indent(depth, f);
    fputc(']', f);
}

static void json_emit_value(const DogValue *v, int pretty, int depth, FILE *f) {
    if (!v) { fputs("null", f); return; }
    switch (v->type) {
        case DOG_INT: fprintf(f, "%d", v->as.i); break;
        case DOG_FLOAT: fprintf(f, "%g", v->as.f); break;
        case DOG_STR: json_emit_string(v->as.s ? v->as.s->data : "", f); break;
        case DOG_BOOL: fputs(v->as.b ? "true" : "false", f); break;
        case DOG_LIST: json_emit_list(v->as.l, pretty, depth, f); break;
        case DOG_MAP: json_emit_map(v->as.m, pretty, depth, f); break;
        default: fputs("null", f); break;
    }
}

static DogString *json_stringify_map(const DogMap *m, int pretty) {
    FILE *mem;
    char *buf = NULL;
    size_t size = 0;
#if defined(__linux__)
    mem = open_memstream(&buf, &size);
    if (!mem) return dog_string_new("{}");
    json_emit_map(m, pretty, 0, mem);
    fclose(mem);
    {
        DogString *s = dog_string_new_len(buf ? buf : "", size);
        free(buf);
        return s;
    }
#else
    (void)m;
    (void)pretty;
    return dog_string_new("{}");
#endif
}

DogString *dog_json_stringify(const DogMap *data) { return json_stringify_map(data, 0); }
DogString *dog_json_pretty(const DogMap *data) { return json_stringify_map(data, 1); }

int dog_json_valid(const char *input) {
    JsonP p;
    DogValue *v;
    p.s = input ? input : "";
    p.pos = 0;
    v = json_parse_value(&p);
    if (!v) return 0;
    jp_ws(&p);
    return p.s[p.pos] == '\0';
}
