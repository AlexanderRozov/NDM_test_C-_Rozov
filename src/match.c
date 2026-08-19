#include "match.h"

#include <stdlib.h>
#include <string.h>

int pattern_is_glob(const char *s)
{
    for (; *s != '\0'; s++) {
        switch (*s) {
        case '.':
        case '*':
            return 1;
        default:
            break;
        }
    }
    return 0;
}

int match_pattern(const char *pattern, const char *text)
{
    size_t plen;
    size_t tlen;
    size_t cells;
    size_t i;
    size_t j;
    unsigned char stack[65 * 65];
    unsigned char *dp;
    int heap = 0;
    int ok;

    if (pattern == NULL || text == NULL) {
        return 0;
    }
    if (!pattern_is_glob(pattern)) {
        return strcmp(pattern, text) == 0;
    }

    plen = strlen(pattern);
    tlen = strlen(text);
    cells = (plen + 1) * (tlen + 1);
    if (cells <= sizeof stack) {
        dp = stack;
        memset(dp, 0, cells);
    } else {
        dp = calloc(cells, 1);
        if (dp == NULL) {
            return 0;
        }
        heap = 1;
    }

#define AT(pi, tj) dp[(pi) * (tlen + 1) + (tj)]

    AT(0, 0) = 1;
    for (i = 1; i <= plen; i++) {
        if (pattern[i - 1] != '*') {
            break;
        }
        AT(i, 0) = AT(i - 1, 0);
    }

    for (i = 1; i <= plen; i++) {
        char p = pattern[i - 1];
        for (j = 1; j <= tlen; j++) {
            switch (p) {
            case '*':
                AT(i, j) = (unsigned char)(AT(i - 1, j) || AT(i, j - 1));
                break;
            case '.':
                AT(i, j) = AT(i - 1, j - 1);
                break;
            default:
                AT(i, j) = (unsigned char)(p == text[j - 1] ? AT(i - 1, j - 1) : 0);
                break;
            }
        }
    }

    ok = AT(plen, tlen);
#undef AT
    if (heap) {
        free(dp);
    }
    return ok;
}
