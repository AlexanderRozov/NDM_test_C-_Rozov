#ifndef DICT_HPP
#define DICT_HPP

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

struct DictEntry {
    std::string expect;
    std::string answer;
    bool glob = false;
};

class Dictionary {
public:
    bool load(const std::string& path);
    const std::string* lookup(std::string_view command) const;
    std::size_t size() const { return items_.size(); }

private:
    std::vector<DictEntry> items_;
};

#endif
