#include "dict.h"
#include "match.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dup_str(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p != NULL) {
        memcpy(p, s, n);
    }
    return p;
}

static char *trim(char *s)
{
    char *end;

    while (*s == ' ' || *s == '\t') {
        s++;
    }
    end = s + strlen(s);
    while (end > s) {
        switch (end[-1]) {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            *--end = '\0';
            continue;
        default:
            return s;
        }
    }
    return s;
}

static void str_toupper(char *s)
{
    for (; *s != '\0'; s++) {
        if (*s >= 'a' && *s <= 'z') {
            *s = (char)(*s - 'a' + 'A');
        }
    }
}

static void unescape(char *s)
{
    char *r = s;
    char *w = s;

    while (*r != '\0') {
        if (*r != '\\' || r[1] == '\0') {
            *w++ = *r++;
            continue;
        }
        r++;
        switch (*r++) {
        case 'n':
            *w++ = '\n';
            break;
        case 'r':
            *w++ = '\r';
            break;
        case 't':
            *w++ = '\t';
            break;
        default:
            *w++ = r[-1];
            break;
        }
    }
    *w = '\0';
}

static int dict_push(Dict *dict, char *expect, char *answer)
{
    DictEntry *grown;
    DictEntry *e;

    if (dict->count == dict->cap) {
        size_t ncap = dict->cap == 0 ? 8 : dict->cap * 2;
        grown = realloc(dict->items, ncap * sizeof(*grown));
        if (grown == NULL) {
            return -1;
        }
        dict->items = grown;
        dict->cap = ncap;
    }
    e = &dict->items[dict->count++];
    e->expect = expect;
    e->answer = answer;
    e->glob = (unsigned char)pattern_is_glob(expect);
    return 0;
}

static int csv_field(const char **pp, char *out, size_t out_sz)
{
    const char *s = *pp;
    size_t n = 0;

    while (*s == ' ' || *s == '\t') {
        s++;
    }

    if (*s == '"') {
        s++;
        for (;;) {
            switch (*s) {
            case '\0':
                return -1;
            case '"':
                if (s[1] == '"') {
                    if (n + 1 >= out_sz) {
                        return -1;
                    }
                    out[n++] = '"';
                    s += 2;
                    break;
                }
                s++;
                while (*s == ' ' || *s == '\t') {
                    s++;
                }
                goto field_end;
            default:
                if (n + 1 >= out_sz) {
                    return -1;
                }
                out[n++] = *s++;
                break;
            }
        }
    } else {
        while (*s != '\0' && *s != ',') {
            if (n + 1 >= out_sz) {
                return -1;
            }
            out[n++] = *s++;
        }
        while (n > 0) {
            switch (out[n - 1]) {
            case ' ':
            case '\t':
                n--;
                continue;
            default:
                break;
            }
            break;
        }
    }

field_end:
    out[n] = '\0';
    switch (*s) {
    case ',':
        *pp = s + 1;
        return 1;
    case '\0':
        *pp = s;
        return 0;
    default:
        return -1;
    }
}

int dict_load(Dict *dict, const char *path)
{
    FILE *fp;
    char line[1024];
    unsigned int lineno = 0;
    char *expect = NULL;
    char *answer = NULL;

    dict->items = NULL;
    dict->count = 0;
    dict->cap = 0;

    fp = fopen(path, "r");
    if (fp == NULL) {
        perror(path);
        return -1;
    }

    while (fgets(line, sizeof line, fp) != NULL) {
        const char *p;
        char expect_buf[256];
        char answer_buf[768];
        int more;

        lineno++;
        p = trim(line);
        switch (p[0]) {
        case '\0':
        case '#':
            continue;
        default:
            break;
        }

        more = csv_field(&p, expect_buf, sizeof expect_buf);
        if (more != 1) {
            fprintf(stderr, "%s:%u: expected two CSV columns (expect,answer)\n",
                    path, lineno);
            goto fail;
        }
        more = csv_field(&p, answer_buf, sizeof answer_buf);
        if (more != 0) {
            fprintf(stderr, "%s:%u: expected exactly two CSV columns\n", path, lineno);
            goto fail;
        }

        str_toupper(expect_buf);
        if (strcmp(expect_buf, "EXPECT") == 0) {
            continue;
        }

        unescape(answer_buf);
        expect = dup_str(expect_buf);
        answer = dup_str(answer_buf);
        if (expect == NULL || answer == NULL || dict_push(dict, expect, answer) != 0) {
            goto fail;
        }
        expect = NULL;
        answer = NULL;
    }

    fclose(fp);
    return 0;

fail:
    free(expect);
    free(answer);
    fclose(fp);
    dict_free(dict);
    return -1;
}

const char *dict_lookup(const Dict *dict, const char *command)
{
    size_t i;

    for (i = 0; i < dict->count; i++) {
        const DictEntry *e = &dict->items[i];
        if (e->glob) {
            if (match_pattern(e->expect, command)) {
                return e->answer;
            }
        } else if (strcmp(e->expect, command) == 0) {
            return e->answer;
        }
    }
    return NULL;
}

void dict_free(Dict *dict)
{
    size_t i;

    if (dict->items == NULL) {
        return;
    }
    for (i = 0; i < dict->count; i++) {
        free(dict->items[i].expect);
        free(dict->items[i].answer);
    }
    free(dict->items);
    dict->items = NULL;
    dict->count = 0;
    dict->cap = 0;
}