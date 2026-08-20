#ifndef TTY_HPP
#define TTY_HPP

#include <cstddef>
#include <string>
#include <string_view>

class Tty {
public:
    Tty() = default;
    explicit Tty(int fd);
    ~Tty();

    Tty(const Tty&) = delete;
    Tty& operator=(const Tty&) = delete;
    Tty(Tty&& other) noexcept;
    Tty& operator=(Tty&& other) noexcept;

    static Tty open_device(const std::string& path);
    static Tty open_pty(std::string& slave);

    bool valid() const { return fd_ >= 0; }
    int fd() const { return fd_; }
    void close();

    bool read_line(std::string& line, bool echo);
    bool write_all(std::string_view data);
    bool write_response(std::string_view answer);

private:
    int fd_ = -1;

    bool configure();
};

#endif
