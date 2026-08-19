#include "modem.h"
#include "tty.h"

#include <string.h>

static void str_toupper_copy(char *dst, const char *src, size_t dst_sz)
{
    size_t i;

    for (i = 0; i + 1 < dst_sz && src[i] != '\0'; i++) {
        char c = src[i];
        if (c >= 'a' && c <= 'z') {
            c = (char)(c - 'a' + 'A');
        }
        dst[i] = c;
    }
    dst[i] = '\0';
}

static int set_answer(char *dst, size_t dst_sz, const char *text)
{
    size_t n;

    if (text == NULL) {
        text = "ERROR";
    }
    n = strlen(text);
    if (n >= dst_sz) {
        n = dst_sz - 1;
    }
    memcpy(dst, text, n);
    dst[n] = '\0';
    return 1;
}

static int parse_pin(const char *cmd, char *pin, size_t pin_sz)
{
    const char *p = cmd;
    size_t i = 0;

    if (*p == '"') {
        p++;
    }
    while (*p != '\0' && *p != '"' && i + 1 < pin_sz) {
        pin[i++] = *p++;
    }
    pin[i] = '\0';
    return i > 0;
}

static int handle_cpin_set(Modem *modem, const char *value, char *answer, size_t answer_sz)
{
    char got[16];

    if (!parse_pin(value, got, sizeof got)) {
        return set_answer(answer, answer_sz, "ERROR");
    }
    if (strcmp(got, modem->pin) == 0) {
        modem->pin_ready = 1;
        return set_answer(answer, answer_sz, "OK");
    }
    return set_answer(answer, answer_sz, "+CME ERROR: 16");
}

void modem_init(Modem *modem, Dict *dict)
{
    modem->dict = dict;
    modem->echo_on = 1;
    modem->pin_ready = 0;
    memcpy(modem->pin, "0000", 5);
}

int modem_handle(Modem *modem, const char *command, char *answer, size_t answer_sz)
{
    char cmd[512];
    const char *found;

    str_toupper_copy(cmd, command, sizeof cmd);
    if (cmd[0] == '\0') {
        answer[0] = '\0';
        return 0;
    }

    if (cmd[0] == 'A' && cmd[1] == 'T') {
        switch (cmd[2]) {
        case 'E':
            switch (cmd[3]) {
            case '\0':
                modem->echo_on = 0;
                return set_answer(answer, answer_sz, "OK");
            case '0':
                if (cmd[4] == '\0') {
                    modem->echo_on = 0;
                    return set_answer(answer, answer_sz, "OK");
                }
                break;
            case '1':
                if (cmd[4] == '\0') {
                    modem->echo_on = 1;
                    return set_answer(answer, answer_sz, "OK");
                }
                break;
            default:
                break;
            }
            break;
        case '+':
            if (cmd[3] == 'C' && cmd[4] == 'P' && cmd[5] == 'I' && cmd[6] == 'N') {
                switch (cmd[7]) {
                case '?':
                    if (cmd[8] == '\0') {
                        return set_answer(answer, answer_sz,
                                         modem->pin_ready ? "+CPIN: READY\nOK"
                                                          : "+CPIN: SIM PIN\nOK");
                    }
                    break;
                case '=':
                    return handle_cpin_set(modem, cmd + 8, answer, answer_sz);
                default:
                    break;
                }
            }
            break;
        default:
            break;
        }
    }

    found = dict_lookup(modem->dict, cmd);
    return set_answer(answer, answer_sz, found != NULL ? found : "ERROR");
}

int modem_serve(int fd, Modem *modem)
{
    char line[512];
    char answer[1024];

    while (tty_read_line(fd, line, sizeof line, modem->echo_on) > 0) {
        if (line[0] == '\0') {
            continue;
        }
        if (modem_handle(modem, line, answer, sizeof answer) && answer[0] != '\0') {
            if (tty_write_response(fd, answer) < 0) {
                return -1;
            }
        }
    }
    return 0;
}
