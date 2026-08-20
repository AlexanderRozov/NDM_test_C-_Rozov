#include "tty.hpp"

#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

Tty::Tty(int fd) : fd_(fd) {}

Tty::~Tty()
{
    close();
}

Tty::Tty(Tty&& other) noexcept : fd_(other.fd_)
{
    other.fd_ = -1;
}

Tty& Tty::operator=(Tty&& other) noexcept
{
    if (this != &other) {
        close();
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

void Tty::close()
{
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool Tty::configure()
{
    termios t{};
    if (tcgetattr(fd_, &t) < 0) {
        return false;
    }
    cfmakeraw(&t);
    cfsetispeed(&t, B115200);
    cfsetospeed(&t, B115200);
    t.c_cflag |= (CLOCAL | CREAD);
    t.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
    t.c_cflag |= CS8;
    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0;
    if (tcsetattr(fd_, TCSANOW, &t) < 0) {
        return false;
    }
    tcflush(fd_, TCIOFLUSH);
    return true;
}

Tty Tty::open_device(const std::string& path)
{
    int fd = ::open(path.c_str(), O_RDWR | O_NOCTTY);
    if (fd < 0) {
        std::perror(path.c_str());
        return Tty{};
    }
    Tty tty(fd);
    if (!tty.configure()) {
        std::perror("tcsetattr");
        return Tty{};
    }
    return tty;
}

Tty Tty::open_pty(std::string& slave)
{
    int fd = posix_openpt(O_RDWR | O_NOCTTY);
    if (fd < 0) {
        std::perror("posix_openpt");
        return Tty{};
    }
    Tty tty(fd);
    if (grantpt(fd) < 0 || unlockpt(fd) < 0) {
        std::perror("grantpt/unlockpt");
        return Tty{};
    }
    const char* name = ptsname(fd);
    if (name == nullptr) {
        std::perror("ptsname");
        return Tty{};
    }
    slave = name;
    if (!tty.configure()) {
        std::perror("tcsetattr");
        return Tty{};
    }
    return tty;
}

bool Tty::write_all(std::string_view data)
{
    std::size_t off = 0;
    while (off < data.size()) {
        ssize_t n = ::write(fd_, data.data() + off, data.size() - off);
        if (n < 0) {
            switch (errno) {
            case EINTR:
                continue;
            default:
                return false;
            }
        }
        if (n == 0) {
            return false;
        }
        off += static_cast<std::size_t>(n);
    }
    return true;
}

bool Tty::read_line(std::string& line, bool echo)
{
    line.clear();
    for (;;) {
        char c = 0;
        ssize_t n = ::read(fd_, &c, 1);
        if (n < 0) {
            switch (errno) {
            case EINTR:
                continue;
            default:
                return false;
            }
        }
        if (n == 0) {
            return !line.empty();
        }

        switch (static_cast<unsigned char>(c)) {
        case '\r':
        case '\n':
            if (line.empty()) {
                continue;
            }
            if (echo && !write_all("\r\n")) {
                return false;
            }
            return true;
        case '\b':
        case 0x7f:
            if (!line.empty()) {
                line.pop_back();
                if (echo && !write_all("\b \b")) {
                    return false;
                }
            }
            break;
        default:
            if (static_cast<unsigned char>(c) < 32 || line.size() >= 511) {
                break;
            }
            line.push_back(c);
            if (echo && !write_all(std::string_view(&c, 1))) {
                return false;
            }
            break;
        }
    }
}

bool Tty::write_response(std::string_view answer)
{
    if (answer.empty()) {
        answer = "ERROR";
    }
    if (!write_all("\r\n")) {
        return false;
    }

    std::size_t start = 0;
    while (start < answer.size()) {
        std::size_t nl = answer.find('\n', start);
        std::string_view part = answer.substr(start, nl == std::string_view::npos ? std::string_view::npos : nl - start);
        if (!write_all(part) || !write_all("\r\n")) {
            return false;
        }
        if (nl == std::string_view::npos) {
            break;
        }
        start = nl + 1;
    }
    return true;
}
