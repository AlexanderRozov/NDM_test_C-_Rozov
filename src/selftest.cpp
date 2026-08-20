#include "selftest.hpp"
#include "dict.hpp"
#include "match.hpp"
#include "modem.hpp"
#include "tty.hpp"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>

static int g_failed;

static void expect_true(const char* name, bool cond)
{
    if (cond) {
        std::printf("  OK  %s\n", name);
    } else {
        std::printf("  FAIL %s\n", name);
        ++g_failed;
    }
}

static void test_match()
{
    std::printf("match:\n");
    expect_true("AT == AT", match_pattern("AT", "AT"));
    expect_true("AT != ATI", !match_pattern("AT", "ATI"));
    expect_true("ATE. == ATE0", match_pattern("ATE.", "ATE0"));
    expect_true("ATE. == ATE1", match_pattern("ATE.", "ATE1"));
    expect_true("ATE. != ATE", !match_pattern("ATE.", "ATE"));
    expect_true("AT+CPIN* == AT+CPIN?", match_pattern("AT+CPIN*", "AT+CPIN?"));
    expect_true("AT+CPIN* == AT+CPIN=\"0000\"", match_pattern("AT+CPIN*", "AT+CPIN=\"0000\""));
    expect_true("* == FOO", match_pattern("*", "FOO"));
    expect_true("* == empty", match_pattern("*", ""));
    expect_true("A.T == AXT", match_pattern("A.T", "AXT"));
    expect_true(".*. == ABC", match_pattern(".*.", "ABC"));
    expect_true("empty == empty", match_pattern("", ""));
}

static void test_modem(const std::string& dict_path)
{
    Dictionary dict;
    std::printf("modem/dict:\n");
    if (!dict.load(dict_path)) {
        expect_true("load dictionary", false);
        return;
    }
    expect_true("load dictionary", true);
    Modem modem(dict);

    expect_true("AT -> OK", modem.handle("at") == "OK");
    const std::string ati = modem.handle("ATI");
    expect_true("ATI contains FakeModem", ati.find("FakeModem") != std::string::npos);
    expect_true("ATI ends with OK", ati.find("OK") != std::string::npos);
    expect_true("ATE0 -> OK", modem.handle("ATE0") == "OK");
    expect_true("echo off after ATE0", !modem.echo_on());
    modem.handle("ATE1");
    expect_true("echo on after ATE1", modem.echo_on());
    expect_true("AT+COPS? has operator",
                modem.handle("AT+COPS?").find("Test Operator") != std::string::npos);
    expect_true("AT+CPIN? starts as SIM PIN",
                modem.handle("AT+CPIN?").find("SIM PIN") != std::string::npos);
    expect_true("AT+CPIN=\"0000\" -> OK", modem.handle("AT+CPIN=\"0000\"") == "OK");
    expect_true("AT+CPIN? becomes READY",
                modem.handle("AT+CPIN?").find("READY") != std::string::npos);
    expect_true("unknown -> ERROR", modem.handle("AT+FOO") == "ERROR");
}

static bool wait_reply(int fd, std::string& buf, int timeout_ms)
{
    buf.clear();
    for (;;) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        timeval tv{};
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        const int r = select(fd + 1, &rfds, nullptr, nullptr, &tv);
        switch (r) {
        case 0:
            return !buf.empty();
        case -1:
            if (errno == EINTR) {
                continue;
            }
            return false;
        default:
            break;
        }
        char c = 0;
        const ssize_t n = ::read(fd, &c, 1);
        if (n <= 0) {
            break;
        }
        buf.push_back(c);
        if (buf.find("OK\r\n") != std::string::npos ||
            buf.find("ERROR\r\n") != std::string::npos ||
            buf.find("+CME ERROR") != std::string::npos) {
            return true;
        }
        if (buf.size() >= 1023) {
            break;
        }
    }
    return !buf.empty();
}

static bool send_expect(Tty& tty, const char* cmd, const char* needle, const char* name)
{
    std::string wire = cmd;
    wire.push_back('\r');
    if (!tty.write_all(wire)) {
        expect_true(name, false);
        return false;
    }
    std::string buf;
    if (!wait_reply(tty.fd(), buf, 1500)) {
        std::printf("  FAIL %s (timeout, got %s)\n", name, buf.c_str());
        ++g_failed;
        return false;
    }
    if (buf.find(needle) == std::string::npos) {
        std::printf("  FAIL %s (need '%s', got '%s')\n", name, needle, buf.c_str());
        ++g_failed;
        return false;
    }
    std::printf("  OK  %s\n", name);
    return true;
}

static void test_pty(const std::string& dict_path)
{
    Dictionary dict;
    std::printf("pty:\n");
    if (!dict.load(dict_path)) {
        expect_true("pty dict load", false);
        return;
    }

    std::string slave;
    Tty master = Tty::open_pty(slave);
    if (!master.valid()) {
        expect_true("open pty", false);
        return;
    }
    expect_true("open pty", true);

    const pid_t pid = fork();
    if (pid < 0) {
        expect_true("fork", false);
        return;
    }

    if (pid == 0) {
        Modem modem(dict);
        modem.serve(master);
        _exit(0);
    }

    master.close();
    usleep(100 * 1000);
    Tty client = Tty::open_device(slave);
    if (!client.valid()) {
        expect_true("open slave", false);
        kill(pid, SIGTERM);
        waitpid(pid, nullptr, 0);
        return;
    }

    send_expect(client, "AT", "OK", "PTY AT");
    send_expect(client, "ATI", "FakeModem", "PTY ATI");
    send_expect(client, "ATE0", "OK", "PTY ATE0");
    send_expect(client, "AT+COPS?", "Test Operator", "PTY AT+COPS?");
    send_expect(client, "AT+CPIN?", "SIM PIN", "PTY AT+CPIN?");
    send_expect(client, "AT+CPIN=\"0000\"", "OK", "PTY AT+CPIN=");
    send_expect(client, "AT+CPIN?", "READY", "PTY AT+CPIN? READY");
    send_expect(client, "ATXYZ", "ERROR", "PTY unknown");
    client.close();
    waitpid(pid, nullptr, 0);
}

int run_self_test(const std::string& dict_path)
{
    g_failed = 0;
    std::printf("self-test (%s)\n", dict_path.c_str());
    test_match();
    test_modem(dict_path);
    test_pty(dict_path);
    if (g_failed == 0) {
        std::printf("all tests passed\n");
        return 0;
    }
    std::printf("%d test(s) failed\n", g_failed);
    return 1;
}
