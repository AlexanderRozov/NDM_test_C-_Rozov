#include "selftest.h"
#include "dict.h"
#include "match.h"
#include "modem.h"
#include "tty.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>

static int g_failed;

static void expect_true(const char *name, int cond)
{
    if (cond) {
        printf("  OK  %s\n", name);
    } else {
        printf("  FAIL %s\n", name);
        g_failed++;
    }
}

static int test_match(void)
{
    printf("match:\n");
    expect_true("AT == AT", match_pattern("AT", "AT"));
    expect_true("AT != ATI", !match_pattern("AT", "ATI"));
    expect_true("ATE. == ATE0", match_pattern("ATE.", "ATE0"));
    expect_true("ATE. == ATE1", match_pattern("ATE.", "ATE1"));
    expect_true("ATE. != ATE", !match_pattern("ATE.", "ATE"));
    expect_true("AT+CPIN* == AT+CPIN?", match_pattern("AT+CPIN*", "AT+CPIN?"));
    expect_true("AT+CPIN* == AT+CPIN=\"0000\"",
                match_pattern("AT+CPIN*", "AT+CPIN=\"0000\""));
    expect_true("* == FOO", match_pattern("*", "FOO"));
    expect_true("* == empty", match_pattern("*", ""));
    expect_true("A.T == AXT", match_pattern("A.T", "AXT"));
    expect_true(".*. == ABC", match_pattern(".*.", "ABC"));
    expect_true("empty == empty", match_pattern("", ""));
    return g_failed == 0 ? 0 : -1;
}

static int test_modem(const char *dict_path)
{
    Dict dict;
    Modem modem;
    char ans[512];
    int before = g_failed;

    printf("modem/dict:\n");
    if (dict_load(&dict, dict_path) != 0) {
        expect_true("load dictionary", 0);
        return -1;
    }
    expect_true("load dictionary", 1);
    modem_init(&modem, &dict);

    modem_handle(&modem, "at", ans, sizeof ans);
    expect_true("AT -> OK", strcmp(ans, "OK") == 0);

    modem_handle(&modem, "ATI", ans, sizeof ans);
    expect_true("ATI contains FakeModem", strstr(ans, "FakeModem") != NULL);
    expect_true("ATI ends with OK", strstr(ans, "OK") != NULL);

    modem_handle(&modem, "ATE0", ans, sizeof ans);
    expect_true("ATE0 -> OK", strcmp(ans, "OK") == 0);
    expect_true("echo off after ATE0", modem.echo_on == 0);

    modem_handle(&modem, "ATE1", ans, sizeof ans);
    expect_true("echo on after ATE1", modem.echo_on == 1);

    modem_handle(&modem, "AT+COPS?", ans, sizeof ans);
    expect_true("AT+COPS? has operator", strstr(ans, "Test Operator") != NULL);

    modem_handle(&modem, "AT+CPIN?", ans, sizeof ans);
    expect_true("AT+CPIN? starts as SIM PIN", strstr(ans, "SIM PIN") != NULL);

    modem_handle(&modem, "AT+CPIN=\"0000\"", ans, sizeof ans);
    expect_true("AT+CPIN=\"0000\" -> OK", strcmp(ans, "OK") == 0);

    modem_handle(&modem, "AT+CPIN?", ans, sizeof ans);
    expect_true("AT+CPIN? becomes READY", strstr(ans, "READY") != NULL);

    modem_handle(&modem, "AT+FOO", ans, sizeof ans);
    expect_true("unknown -> ERROR", strcmp(ans, "ERROR") == 0);

    dict_free(&dict);
    return g_failed == before ? 0 : -1;
}

static int wait_reply(int fd, char *buf, size_t buflen, int timeout_ms)
{
    size_t got = 0;

    buf[0] = '\0';
    while (got + 1 < buflen) {
        fd_set rfds;
        struct timeval tv;
        int r;
        char c;
        ssize_t n;

        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        r = select(fd + 1, &rfds, NULL, NULL, &tv);
        switch (r) {
        case 0:
            goto done;
        case -1:
            if (errno == EINTR) {
                continue;
            }
            return -1;
        default:
            break;
        }
        n = read(fd, &c, 1);
        if (n <= 0) {
            break;
        }
        buf[got++] = c;
        buf[got] = '\0';
        if (strstr(buf, "OK\r\n") != NULL || strstr(buf, "ERROR\r\n") != NULL ||
            strstr(buf, "+CME ERROR") != NULL) {
            return 0;
        }
    }
done:
    return got > 0 ? 0 : -1;
}

static int send_expect(int fd, const char *cmd, const char *needle, const char *name)
{
    char buf[1024];

    if (tty_write_all(fd, cmd, strlen(cmd)) < 0 ||
        tty_write_all(fd, "\r", 1) < 0) {
        expect_true(name, 0);
        return -1;
    }
    if (wait_reply(fd, buf, sizeof buf, 1500) < 0) {
        printf("  FAIL %s (timeout, got %s)\n", name, buf);
        g_failed++;
        return -1;
    }
    if (strstr(buf, needle) == NULL) {
        printf("  FAIL %s (need '%s', got '%s')\n", name, needle, buf);
        g_failed++;
        return -1;
    }
    printf("  OK  %s\n", name);
    return 0;
}

static int test_pty(const char *dict_path)
{
    Dict dict;
    char slave[128];
    int master;
    pid_t pid;
    int status;
    int before = g_failed;

    printf("pty:\n");
    if (dict_load(&dict, dict_path) != 0) {
        expect_true("pty dict load", 0);
        return -1;
    }

    master = tty_open_pty(slave, sizeof slave);
    if (master < 0) {
        expect_true("open pty", 0);
        dict_free(&dict);
        return -1;
    }
    expect_true("open pty", 1);

    pid = fork();
    if (pid < 0) {
        expect_true("fork", 0);
        tty_close(master);
        dict_free(&dict);
        return -1;
    }

    if (pid == 0) {
        Modem modem;

        modem_init(&modem, &dict);
        modem_serve(master, &modem);
        _exit(0);
    }

    tty_close(master);
    usleep(100 * 1000);
    {
        int slave_fd = tty_open_device(slave);
        if (slave_fd < 0) {
            expect_true("open slave", 0);
            kill(pid, SIGTERM);
            waitpid(pid, &status, 0);
            dict_free(&dict);
            return -1;
        }
        send_expect(slave_fd, "AT", "OK", "PTY AT");
        send_expect(slave_fd, "ATI", "FakeModem", "PTY ATI");
        send_expect(slave_fd, "ATE0", "OK", "PTY ATE0");
        send_expect(slave_fd, "AT+COPS?", "Test Operator", "PTY AT+COPS?");
        send_expect(slave_fd, "AT+CPIN?", "SIM PIN", "PTY AT+CPIN?");
        send_expect(slave_fd, "AT+CPIN=\"0000\"", "OK", "PTY AT+CPIN=");
        send_expect(slave_fd, "AT+CPIN?", "READY", "PTY AT+CPIN? READY");
        send_expect(slave_fd, "ATXYZ", "ERROR", "PTY unknown");
        tty_close(slave_fd);
    }
    waitpid(pid, &status, 0);
    dict_free(&dict);

    return g_failed == before ? 0 : -1;
}

int run_self_test(const char *dict_path)
{
    g_failed = 0;
    printf("self-test (%s)\n", dict_path);
    test_match();
    test_modem(dict_path);
    test_pty(dict_path);
    if (g_failed == 0) {
        printf("all tests passed\n");
        return 0;
    }
    printf("%d test(s) failed\n", g_failed);
    return 1;
}
