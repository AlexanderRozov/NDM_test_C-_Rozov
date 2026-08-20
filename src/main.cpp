#include "dict.hpp"
#include "modem.hpp"
#include "selftest.hpp"
#include "tty.hpp"

#include <cstdio>
#include <cstring>
#include <string>

static void usage(const char* argv0)
{
    std::fprintf(stderr,
                 "Usage: %s [-d DEVICE] [-f FILE] [--self-test]\n"
                 "  -d DEVICE    TTY to listen on (default: allocate a PTY)\n"
                 "  -f FILE      CSV dictionary expect,answer (default: data/responses.csv)\n"
                 "  --self-test  run matcher, dictionary and PTY checks\n",
                 argv0);
}

int main(int argc, char** argv)
{
    const char* device = nullptr;
    const char* dict_path = "data/responses.csv";
    bool self_test = false;

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
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
            if (std::strcmp(arg, "--self-test") == 0) {
                self_test = true;
                break;
            }
            if (std::strcmp(arg, "--help") == 0) {
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

    Dictionary dict;
    if (!dict.load(dict_path)) {
        return 1;
    }
    Modem modem(dict);

    Tty tty;
    std::string slave;
    const char* listen_on = nullptr;
    if (device != nullptr) {
        tty = Tty::open_device(device);
        listen_on = device;
    } else {
        tty = Tty::open_pty(slave);
        listen_on = slave.c_str();
    }
    if (!tty.valid()) {
        return 1;
    }

    std::fprintf(stderr, "Listening on %s\n", listen_on);
    if (device == nullptr) {
        std::fprintf(stderr, "Example: minicom -D %s\n", listen_on);
    }

    return modem.serve(tty) == 0 ? 0 : 1;
}
