#ifndef MODEM_H
#define MODEM_H

#include "dict.h"

#include <stddef.h>

typedef struct {
    Dict *dict;
    int echo_on;
    int pin_ready;
    char pin[16];
} Modem;

void modem_init(Modem *modem, Dict *dict);
int modem_handle(Modem *modem, const char *command, char *answer, size_t answer_sz);
int modem_serve(int fd, Modem *modem);

#endif
