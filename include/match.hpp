#ifndef MATCH_HPP
#define MATCH_HPP

#include <string_view>

/*
 * Limited pattern syntax (not POSIX regex, no library, no std::regex):
 *   '.'  — exactly one any character
 *   '*'  — any sequence, including empty
 */
bool match_pattern(std::string_view pattern, std::string_view text);
bool pattern_is_glob(std::string_view pattern);

#endif
