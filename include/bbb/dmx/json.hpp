#pragma once

#include <cctype>
#include <cstdlib>
#include <cmath>
#include <map>
#include <string>
#include <vector>


namespace bbb::dmx {

enum class json_type {
    null_value,
    boolean,
    number,
    string,
    array,
    object,
};

struct json_value {
public:
    json_type type{json_type::null_value};
    bool boolean_value{false};
    double number_value{0.0};
    std::string string_value{};
    std::vector<json_value> array_value{};
    std::map<std::string, json_value> object_value{};

    const json_value *find(const std::string &key) const {
        if(type != json_type::object) {
            return nullptr;
        }
        const auto found = object_value.find(key);
        if(found == object_value.end()) {
            return nullptr;
        }
        return &found->second;
    }
};

struct json_parse_result {
public:
    bool ok{false};
    std::string message{};
    json_value value{};
};

class json_parser {
public:
    explicit json_parser(const std::string &text)
    : text_{text}
    {}

    json_parse_result parse() {
        skip_space();
        json_value value{};
        if(!parse_value(value)) {
            return json_parse_result{false, error_, {}};
        }
        skip_space();
        if(position_ != text_.size()) {
            return json_parse_result{false, "trailing content after JSON value", {}};
        }
        return json_parse_result{true, "", value};
    }

private:
    bool parse_value(json_value &value) {
        skip_space();
        if(position_ >= text_.size()) {
            return fail("unexpected end of JSON");
        }
        const char character{text_[position_]};
        if(character == '{') {
            return parse_object(value);
        }
        if(character == '[') {
            return parse_array(value);
        }
        if(character == '"') {
            std::string parsed_string{};
            if(!parse_string(parsed_string)) {
                return false;
            }
            value.type = json_type::string;
            value.string_value = parsed_string;
            return true;
        }
        if(character == '-' || std::isdigit((unsigned char)character)) {
            return parse_number(value);
        }
        if(match_literal("true")) {
            value.type = json_type::boolean;
            value.boolean_value = true;
            return true;
        }
        if(match_literal("false")) {
            value.type = json_type::boolean;
            value.boolean_value = false;
            return true;
        }
        if(match_literal("null")) {
            value.type = json_type::null_value;
            return true;
        }
        return fail("unexpected JSON token");
    }

    bool parse_object(json_value &value) {
        value.type = json_type::object;
        value.object_value.clear();
        position_++;
        skip_space();
        if(consume('}')) {
            return true;
        }
        while(position_ < text_.size()) {
            std::string key{};
            if(!parse_string(key)) {
                return false;
            }
            skip_space();
            if(!consume(':')) {
                return fail("expected ':' after object key");
            }
            json_value child{};
            if(!parse_value(child)) {
                return false;
            }
            value.object_value[key] = child;
            skip_space();
            if(consume('}')) {
                return true;
            }
            if(!consume(',')) {
                return fail("expected ',' or '}' in object");
            }
            skip_space();
        }
        return fail("unterminated object");
    }

    bool parse_array(json_value &value) {
        value.type = json_type::array;
        value.array_value.clear();
        position_++;
        skip_space();
        if(consume(']')) {
            return true;
        }
        while(position_ < text_.size()) {
            json_value child{};
            if(!parse_value(child)) {
                return false;
            }
            value.array_value.push_back(child);
            skip_space();
            if(consume(']')) {
                return true;
            }
            if(!consume(',')) {
                return fail("expected ',' or ']' in array");
            }
            skip_space();
        }
        return fail("unterminated array");
    }

    bool parse_string(std::string &value) {
        skip_space();
        if(position_ >= text_.size() || text_[position_] != '"') {
            return fail("expected JSON string");
        }
        position_++;
        value.clear();
        while(position_ < text_.size()) {
            const char character{text_[position_++]};
            if(character == '"') {
                return true;
            }
            if(character == '\\') {
                if(position_ >= text_.size()) {
                    return fail("unterminated string escape");
                }
                const char escaped{text_[position_++]};
                switch(escaped) {
                    case '"': value.push_back('"'); break;
                    case '\\': value.push_back('\\'); break;
                    case '/': value.push_back('/'); break;
                    case 'b': value.push_back('\b'); break;
                    case 'f': value.push_back('\f'); break;
                    case 'n': value.push_back('\n'); break;
                    case 'r': value.push_back('\r'); break;
                    case 't': value.push_back('\t'); break;
                    default: return fail("unsupported string escape");
                }
            } else {
                value.push_back(character);
            }
        }
        return fail("unterminated string");
    }

    bool parse_number(json_value &value) {
        const char *start{text_.c_str() + position_};
        char *end{nullptr};
        const double parsed{std::strtod(start, &end)};
        if(end == start) {
            return fail("invalid number");
        }
        position_ += (std::size_t)(end - start);
        value.type = json_type::number;
        value.number_value = parsed;
        return true;
    }

    bool consume(char expected) {
        skip_space();
        if(position_ < text_.size() && text_[position_] == expected) {
            position_++;
            return true;
        }
        return false;
    }

    bool match_literal(const char *literal) {
        const std::string literal_string{literal};
        if(text_.compare(position_, literal_string.size(), literal_string) == 0) {
            position_ += literal_string.size();
            return true;
        }
        return false;
    }

    void skip_space() {
        while(position_ < text_.size() && std::isspace((unsigned char)text_[position_])) {
            position_++;
        }
    }

    bool fail(const std::string &message) {
        error_ = message + " at byte " + std::to_string(position_);
        return false;
    }

    const std::string &text_;
    std::size_t position_{0};
    std::string error_{};
};

inline json_parse_result parse_json_text(const std::string &text) {
    json_parser parser{text};
    return parser.parse();
}

inline bool json_string(const json_value &object, const std::string &key, std::string &value, bool required, std::string &error) {
    const json_value *child{object.find(key)};
    if(!child) {
        if(required) {
            error = "missing string: " + key;
            return false;
        }
        return true;
    }
    if(child->type != json_type::string) {
        error = "expected string: " + key;
        return false;
    }
    value = child->string_value;
    return true;
}

inline bool json_int(const json_value &object, const std::string &key, int &value, bool required, std::string &error) {
    const json_value *child{object.find(key)};
    if(!child) {
        if(required) {
            error = "missing number: " + key;
            return false;
        }
        return true;
    }
    if(child->type != json_type::number) {
        error = "expected number: " + key;
        return false;
    }
    value = (int)std::round(child->number_value);
    return true;
}

inline bool json_double(const json_value &object, const std::string &key, double &value, bool required, std::string &error) {
    const json_value *child{object.find(key)};
    if(!child) {
        if(required) {
            error = "missing number: " + key;
            return false;
        }
        return true;
    }
    if(child->type != json_type::number) {
        error = "expected number: " + key;
        return false;
    }
    value = child->number_value;
    return true;
}

inline bool json_bool(const json_value &object, const std::string &key, bool &value, bool required, std::string &error) {
    const json_value *child{object.find(key)};
    if(!child) {
        if(required) {
            error = "missing bool: " + key;
            return false;
        }
        return true;
    }
    if(child->type != json_type::boolean) {
        error = "expected bool: " + key;
        return false;
    }
    value = child->boolean_value;
    return true;
}

} // namespace bbb::dmx
