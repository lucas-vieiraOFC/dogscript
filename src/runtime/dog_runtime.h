#ifndef DOG_RUNTIME_H
#define DOG_RUNTIME_H

#include <stddef.h>

typedef struct DogString { char *data; size_t len; } DogString;
typedef struct DogList { void **items; size_t len; size_t cap; } DogList;
typedef struct DogMap { DogString **keys; void **values; size_t len; size_t cap; } DogMap;
typedef struct DogValue {
    int type;
    union {
        int i;
        double f;
        DogString *s;
        DogList *l;
        DogMap *m;
        int b;
        void *p;
    } as;
} DogValue;

#define DOG_INT 0
#define DOG_FLOAT 1
#define DOG_STR 2
#define DOG_BOOL 3
#define DOG_LIST 4
#define DOG_MAP 5
#define DOG_NULL 6

extern long dog_argc;
extern char **dog_argv;

DogString *dog_string_new(const char *src);
DogString *dog_string_new_len(const char *src, size_t len);
void dog_string_free(DogString *s);

DogList *dog_list_new(void);
void dog_list_free(DogList *l, int free_items);
int dog_list_push(DogList *l, void *item);
void *dog_list_get(const DogList *l, size_t idx);

DogMap *dog_map_new(void);
void dog_map_free(DogMap *m, int free_values);
int dog_map_set(DogMap *m, const char *key, void *value);
void *dog_map_get(const DogMap *m, const char *key);
DogString *dog_map_get_key(const DogMap *m, size_t idx);
void *dog_map_get_value(const DogMap *m, size_t idx);

DogValue dog_v_int(int v);
DogValue dog_v_float(double v);
DogValue dog_v_str(const char *v);
DogValue dog_v_bool(int v);
DogValue dog_v_list(DogList *v);
DogValue dog_v_map(DogMap *v);
DogValue dog_v_null(void);

char *dog_strdup(const char *s);
char *dog_strndup(const char *s, size_t n);
void dog_runtime_init(void);
void dog_runtime_cleanup(void);

#endif
