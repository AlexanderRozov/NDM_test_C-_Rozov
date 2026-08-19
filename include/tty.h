#ifndef TTY_H
#define TTY_H

#include <stddef.h>

int tty_open_device(const char *path);
int tty_open_pty(char *slave, size_t slave_sz);
int tty_configure(int fd);
int tty_read_line(int fd, char *buf, size_t buflen, int echo);
int tty_write_all(int fd, const char *data, size_t len);
int tty_write_response(int fd, const char *answer);
void tty_close(int fd);

#endif