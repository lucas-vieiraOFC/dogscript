#include "dog_runtime.h"

#include <string.h>

typedef struct {
    const char *extern_name;
    const char *c_symbol;
    void (*fn)(void);
} DogExternEntry;

/* Forward declarations for exported runtime entry points used by @extern. */
DogMap *dog_http_get(const char *url, const DogMap *headers);
DogMap *dog_http_post(const char *url, const char *body, const DogMap *headers);
DogMap *dog_http_put(const char *url, const char *body, const DogMap *headers);
DogMap *dog_http_patch(const char *url, const char *body, const DogMap *headers);
DogMap *dog_http_delete(const char *url, const DogMap *headers);
DogMap *dog_json_parse(const char *input);
DogString *dog_json_stringify(const DogMap *data);
DogString *dog_json_pretty(const DogMap *data);
int dog_json_valid(const char *input);

static DogExternEntry g_externs[] = {
    {"__http_get", "dog_http_get", (void (*)(void))dog_http_get},
    {"__http_post", "dog_http_post", (void (*)(void))dog_http_post},
    {"__http_put", "dog_http_put", (void (*)(void))dog_http_put},
    {"__http_patch", "dog_http_patch", (void (*)(void))dog_http_patch},
    {"__http_delete", "dog_http_delete", (void (*)(void))dog_http_delete},
    {"__json_parse", "dog_json_parse", (void (*)(void))dog_json_parse},
    {"__json_stringify", "dog_json_stringify", (void (*)(void))dog_json_stringify},
    {"__json_pretty", "dog_json_pretty", (void (*)(void))dog_json_pretty},
    {"__json_valid", "dog_json_valid", (void (*)(void))dog_json_valid},
};

void (*dog_extern_resolve(const char *extern_name))(void) {
    size_t i;
    for (i = 0; i < sizeof(g_externs) / sizeof(g_externs[0]); i++) {
        if (strcmp(g_externs[i].extern_name, extern_name) == 0) {
            return g_externs[i].fn;
        }
    }
    return NULL;
}

const char *dog_extern_symbol_name(const char *extern_name) {
    size_t i;
    for (i = 0; i < sizeof(g_externs) / sizeof(g_externs[0]); i++) {
        if (strcmp(g_externs[i].extern_name, extern_name) == 0) {
            return g_externs[i].c_symbol;
        }
    }
    return NULL;
}
