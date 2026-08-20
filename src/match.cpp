#include "match.hpp"

#include <vector>

bool pattern_is_glob(std::string_view pattern)
{
    for (char c : pattern) {
        switch (c) {
        case '.':
        case '*':
            return true;
        default:
            break;
        }
    }
    return false;
}

bool match_pattern(std::string_view pattern, std::string_view text)
{
    if (!pattern_is_glob(pattern)) {
        return pattern == text;
    }

    const std::size_t plen = pattern.size();
    const std::size_t tlen = text.size();
    const std::size_t cols = tlen + 1;
    std::vector<unsigned char> dp((plen + 1) * cols, 0);

    auto at = [&](std::size_t i, std::size_t j) -> unsigned char& {
        return dp[i * cols + j];
    };

    at(0, 0) = 1;
    for (std::size_t i = 1; i <= plen; ++i) {
        if (pattern[i - 1] != '*') {
            break;
        }
        at(i, 0) = at(i - 1, 0);
    }

    for (std::size_t i = 1; i <= plen; ++i) {
        const char p = pattern[i - 1];
        for (std::size_t j = 1; j <= tlen; ++j) {
            switch (p) {
            case '*':
                at(i, j) = static_cast<unsigned char>(at(i - 1, j) || at(i, j - 1));
                break;
            case '.':
                at(i, j) = at(i - 1, j - 1);
                break;
            default:
                at(i, j) = static_cast<unsigned char>(p == text[j - 1] ? at(i - 1, j - 1) : 0);
                break;
            }
        }
    }

    return at(plen, tlen) != 0;
}
