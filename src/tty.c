#include "tty.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static int tty_fail(int fd, const char *what)
{
    perror(what);
    close(fd);
    return -1;
}

int tty_configure(int fd)
{
    struct termios t;

    if (tcgetattr(fd, &t) < 0) {
        return -1;
    }
    cfmakeraw(&t);
    cfsetispeed(&t, B115200);
    cfsetospeed(&t, B115200);
    t.c_cflag |= (CLOCAL | CREAD);
    t.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
    t.c_cflag |= CS8;
    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0;
    if (tcsetattr(fd, TCSANOW, &t) < 0) {
        return -1;
    }
    tcflush(fd, TCIOFLUSH);
    return 0;
}

int tty_open_device(const char *path)
{
    int fd = open(path, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror(path);
        return -1;
    }
    if (tty_configure(fd) < 0) {
        return tty_fail(fd, "tcsetattr");
    }
    return fd;
}

int tty_open_pty(char *slave, size_t slave_sz)
{
    int fd;
    const char *name;

    fd = posix_openpt(O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror("posix_openpt");
        return -1;
    }
    if (grantpt(fd) < 0 || unlockpt(fd) < 0) {
        return tty_fail(fd, "grantpt/unlockpt");
    }
    name = ptsname(fd);
    if (name == NULL) {
        return tty_fail(fd, "ptsname");
    }
    snprintf(slave, slave_sz, "%s", name);
    if (tty_configure(fd) < 0) {
        return tty_fail(fd, "tcsetattr");
    }
    return fd;
}

int tty_write_all(int fd, const char *data, size_t len)
{
    size_t off = 0;

    while (off < len) {
        ssize_t n = write(fd, data + off, len - off);
        if (n < 0) {
            switch (errno) {
            case EINTR:
                continue;
            default:
                return -1;
            }
        }
        if (n == 0) {
            return -1;
        }
        off += (size_t)n;
    }
    return 0;
}

static int echo_bytes(int fd, int echo, const char *data, size_t len)
{
    return echo ? tty_write_all(fd, data, len) : 0;
}

int tty_read_line(int fd, char *buf, size_t buflen, int echo)
{
    size_t i = 0;

    if (buflen == 0) {
        return -1;
    }

    for (;;) {
        char c;
        ssize_t n = read(fd, &c, 1);

        if (n < 0) {
            switch (errno) {
            case EINTR:
                continue;
            default:
                return -1;
            }
        }
        if (n == 0) {
            buf[i] = '\0';
            return (i > 0) ? 1 : 0;
        }

        switch ((unsigned char)c) {
        case '\r':
        case '\n':
            if (i == 0) {
                continue;
            }
            buf[i] = '\0';
            if (echo_bytes(fd, echo, "\r\n", 2) < 0) {
                return -1;
            }
            return 1;
        case '\b':
        case 0x7f:
            if (i > 0) {
                i--;
                if (echo_bytes(fd, echo, "\b \b", 3) < 0) {
                    return -1;
                }
            }
            break;
        default:
            if ((unsigned char)c < 32 || i + 1 >= buflen) {
                break;
            }
            buf[i++] = c;
            if (echo_bytes(fd, echo, &c, 1) < 0) {
                return -1;
            }
            break;
        }
    }
}

int tty_write_response(int fd, const char *answer)
{
    const char *p;

    if (answer == NULL) {
        answer = "ERROR";
    }
    if (tty_write_all(fd, "\r\n", 2) < 0) {
        return -1;
    }

    p = answer;
    while (*p != '\0') {
        const char *nl = strchr(p, '\n');
        size_t n = nl ? (size_t)(nl - p) : strlen(p);

        if (tty_write_all(fd, p, n) < 0 || tty_write_all(fd, "\r\n", 2) < 0) {
            return -1;
        }
        if (nl == NULL) {
            break;
        }
        p = nl + 1;
    }
    return 0;
}

void tty_close(int fd)
{
    if (fd >= 0) {
        close(fd);
    }
}
