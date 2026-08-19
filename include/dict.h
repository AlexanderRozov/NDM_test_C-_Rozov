#ifndef DICT_H
#define DICT_H

#include <stddef.h>

typedef struct {
    char *expect;
    char *answer;
    unsigned char glob; /* 1 if expect contains '.' or '*' */
} DictEntry;

typedef struct {
    DictEntry *items;
    size_t count;
    size_t cap;
} Dict;

int dict_load(Dict *dict, const char *path);
const char *dict_lookup(const Dict *dict, const char *command);
void dict_free(Dict *dict);

#endif