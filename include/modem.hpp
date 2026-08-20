#ifndef MODEM_HPP
#define MODEM_HPP

#include "dict.hpp"

#include <string>
#include <string_view>

class Tty;

class Modem {
public:
    explicit Modem(Dictionary& dict);

    bool echo_on() const { return echo_on_; }
    std::string handle(std::string_view command);
    int serve(Tty& tty);

private:
    Dictionary* dict_;
    bool echo_on_ = true;
    bool pin_ready_ = false;
    std::string pin_{"0000"};

    std::string handle_cpin_set(std::string_view value);
};

#endif
