#include "dict.hpp"
#include "match.hpp"

#include <cstdio>
#include <fstream>
#include <string>
#include <utility>

static std::string trim_copy(std::string s)
{
    std::size_t b = 0;
    while (b < s.size() && (s[b] == ' ' || s[b] == '\t')) {
        ++b;
    }
    std::size_t e = s.size();
    while (e > b) {
        switch (s[e - 1]) {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            --e;
            continue;
        default:
            break;
        }
        break;
    }
    return s.substr(b, e - b);
}

static void ascii_upper(std::string& s)
{
    for (char& c : s) {
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
        }
    }
}

static void unescape(std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '\\' || i + 1 >= s.size()) {
            out.push_back(s[i]);
            continue;
        }
        ++i;
        switch (s[i]) {
        case 'n':
            out.push_back('\n');
            break;
        case 'r':
            out.push_back('\r');
            break;
        case 't':
            out.push_back('\t');
            break;
        default:
            out.push_back(s[i]);
            break;
        }
    }
    s = std::move(out);
}

/* Returns 1 if a comma follows, 0 at end, -1 on error. */
static int csv_field(std::string_view line, std::size_t& pos, std::string& out)
{
    out.clear();
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) {
        ++pos;
    }

    if (pos < line.size() && line[pos] == '"') {
        ++pos;
        for (;;) {
            if (pos >= line.size()) {
                return -1;
            }
            if (line[pos] == '"') {
                if (pos + 1 < line.size() && line[pos + 1] == '"') {
                    out.push_back('"');
                    pos += 2;
                    continue;
                }
                ++pos;
                while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) {
                    ++pos;
                }
                break;
            }
            out.push_back(line[pos++]);
        }
    } else {
        while (pos < line.size() && line[pos] != ',') {
            out.push_back(line[pos++]);
        }
        while (!out.empty() && (out.back() == ' ' || out.back() == '\t')) {
            out.pop_back();
        }
    }

    if (pos < line.size() && line[pos] == ',') {
        ++pos;
        return 1;
    }
    if (pos >= line.size()) {
        return 0;
    }
    return -1;
}

bool Dictionary::load(const std::string& path)
{
    items_.clear();
    std::ifstream in(path);
    if (!in) {
        std::perror(path.c_str());
        return false;
    }

    std::string line;
    unsigned lineno = 0;
    while (std::getline(in, line)) {
        ++lineno;
        line = trim_copy(std::move(line));
        switch (line.empty() ? '\0' : line[0]) {
        case '\0':
        case '#':
            continue;
        default:
            break;
        }

        std::size_t pos = 0;
        std::string expect;
        std::string answer;
        if (csv_field(line, pos, expect) != 1 || csv_field(line, pos, answer) != 0) {
            std::fprintf(stderr, "%s:%u: expected two CSV columns (expect,answer)\n",
                         path.c_str(), lineno);
            items_.clear();
            return false;
        }

        ascii_upper(expect);
        if (expect == "EXPECT") {
            continue;
        }
        unescape(answer);
        const bool glob = pattern_is_glob(expect);
        items_.push_back(DictEntry{std::move(expect), std::move(answer), glob});
    }
    return true;
}

const std::string* Dictionary::lookup(std::string_view command) const
{
    for (const DictEntry& e : items_) {
        if (e.glob) {
            if (match_pattern(e.expect, command)) {
                return &e.answer;
            }
        } else if (e.expect == command) {
            return &e.answer;
        }
    }
    return nullptr;
}
