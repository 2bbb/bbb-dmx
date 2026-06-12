#pragma once

#include <string>

namespace bbb::dmx {

inline bool wildcard_match(const std::string &pattern, const std::string &text) {
    std::size_t pattern_index{0};
    std::size_t text_index{0};
    std::size_t star_index{std::string::npos};
    std::size_t match_index{0};

    while(text_index < text.size()) {
        if(pattern_index < pattern.size() && (pattern[pattern_index] == '?' || pattern[pattern_index] == text[text_index])) {
            pattern_index++;
            text_index++;
        } else if(pattern_index < pattern.size() && pattern[pattern_index] == '*') {
            star_index = pattern_index++;
            match_index = text_index;
        } else if(star_index != std::string::npos) {
            pattern_index = star_index + 1;
            text_index = ++match_index;
        } else {
            return false;
        }
    }

    while(pattern_index < pattern.size() && pattern[pattern_index] == '*') {
        pattern_index++;
    }
    return pattern_index == pattern.size();
}

} // namespace bbb::dmx
