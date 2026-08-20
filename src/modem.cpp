#include "modem.hpp"
#include "tty.hpp"

#include <string>

static std::string ascii_upper_copy(std::string_view src)
{
    std::string dst;
    dst.reserve(src.size());
    for (char c : src) {
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
        }
        dst.push_back(c);
    }
    return dst;
}

static std::string parse_pin(std::string_view value)
{
    if (!value.empty() && value.front() == '"') {
        value.remove_prefix(1);
    }
    if (!value.empty() && value.back() == '"') {
        value.remove_suffix(1);
    }
    return std::string(value);
}

Modem::Modem(Dictionary& dict) : dict_(&dict) {}

std::string Modem::handle_cpin_set(std::string_view value)
{
    const std::string got = parse_pin(value);
    if (got.empty()) {
        return "ERROR";
    }
    if (got == pin_) {
        pin_ready_ = true;
        return "OK";
    }
    return "+CME ERROR: 16";
}

std::string Modem::handle(std::string_view command)
{
    const std::string cmd = ascii_upper_copy(command);
    if (cmd.empty()) {
        return {};
    }

    if (cmd.size() >= 2 && cmd[0] == 'A' && cmd[1] == 'T') {
        const char third = cmd.size() > 2 ? cmd[2] : '\0';
        switch (third) {
        case 'E': {
            const char fourth = cmd.size() > 3 ? cmd[3] : '\0';
            switch (fourth) {
            case '\0':
                echo_on_ = false;
                return "OK";
            case '0':
                if (cmd.size() == 4) {
                    echo_on_ = false;
                    return "OK";
                }
                break;
            case '1':
                if (cmd.size() == 4) {
                    echo_on_ = true;
                    return "OK";
                }
                break;
            default:
                break;
            }
            break;
        }
        case '+':
            if (cmd.size() >= 7 && cmd.compare(3, 4, "CPIN") == 0) {
                const char kind = cmd.size() > 7 ? cmd[7] : '\0';
                switch (kind) {
                case '?':
                    if (cmd.size() == 8) {
                        return pin_ready_ ? "+CPIN: READY\nOK" : "+CPIN: SIM PIN\nOK";
                    }
                    break;
                case '=':
                    return handle_cpin_set(cmd.substr(8));
                default:
                    break;
                }
            }
            break;
        default:
            break;
        }
    }

    const std::string* found = dict_->lookup(cmd);
    return found != nullptr ? *found : "ERROR";
}

int Modem::serve(Tty& tty)
{
    std::string line;
    while (tty.read_line(line, echo_on_)) {
        if (line.empty()) {
            continue;
        }
        const std::string answer = handle(line);
        if (!answer.empty() && !tty.write_response(answer)) {
            return -1;
        }
    }
    return 0;
}
