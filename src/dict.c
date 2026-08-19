#include "dict.h"
#include "match.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dup_str(const char *s)
{

}

static char *trim(char *s)
{

}

static void str_toupper(char *s)
{

}

static void unescape(char *s)
{

}

static int dict_push(Dict *dict, char *expect, char *answer)
{

}

static int csv_field(const char **pp, char *out, size_t out_sz)
{

}

int dict_load(Dict *dict, const char *path)
{

}

const char *dict_lookup(const Dict *dict, const char *command)
{

}

void dict_free(Dict *dict)
{

}
