#include "dict.h"
#include "modem.h"
#include "selftest.h"
#include "tty.h"

#include <stdio.h>
#include <string.h>

static void usage(const char *argv0)
{
    fprintf(stderr,
            "Usage: %s [-d DEVICE] [-f FILE] [--self-test]\n"
            "  -d DEVICE    TTY to listen on (default: allocate a PTY)\n"
            "  -f FILE      CSV dictionary expect,answer (default: data/responses.csv)\n"
            "  --self-test  run matcher, dictionary and PTY checks\n",
            argv0);
}

int main(int argc, char **argv)
{
    const char *device = NULL;
    const char *dict_path = "data/responses.csv";
    const char *listen_on;
    int self_test = 0;
    int i;
    Dict dict;
    Modem modem;
    int fd;
    char slave[128];
    int rc;

    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (arg[0] != '-') {
            usage(argv[0]);
            return 1;
        }
        switch (arg[1]) {
        case 'd':
            if (arg[2] != '\0' || i + 1 >= argc) {
                usage(argv[0]);
                return 1;
            }
            device = argv[++i];
            break;
        case 'f':
            if (arg[2] != '\0' || i + 1 >= argc) {
                usage(argv[0]);
                return 1;
            }
            dict_path = argv[++i];
            break;
        case 'h':
            usage(argv[0]);
            return 0;
        case '-':
            if (strcmp(arg, "--self-test") == 0) {
                self_test = 1;
                break;
            }
            if (strcmp(arg, "--help") == 0) {
                usage(argv[0]);
                return 0;
            }
            usage(argv[0]);
            return 1;
        default:
            usage(argv[0]);
            return 1;
        }
    }

    if (self_test) {
        return run_self_test(dict_path);
    }

    if (dict_load(&dict, dict_path) != 0) {
        return 1;
    }
    modem_init(&modem, &dict);

    if (device != NULL) {
        fd = tty_open_device(device);
        listen_on = device;
    } else {
        fd = tty_open_pty(slave, sizeof slave);
        listen_on = slave;
    }
    if (fd < 0) {
        dict_free(&dict);
        return 1;
    }

    fprintf(stderr, "Listening on %s\n", listen_on);
    if (device == NULL) {
        fprintf(stderr, "Example: minicom -D %s\n", listen_on);
    }

    rc = modem_serve(fd, &modem);
    tty_close(fd);
    dict_free(&dict);
    return rc == 0 ? 0 : 1;
}
