#include "util/unicode.h"
#include <stdexcept>

namespace laren::util {

std::string encode_codepoint(char32_t cp) {
    std::string result;
    if (cp <= 0x7F) {
        result += static_cast<char>(cp);
    } else if (cp <= 0x7FF) {
        result += static_cast<char>(0xC0 | (cp >> 6));
        result += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp <= 0xFFFF) {
        result += static_cast<char>(0xE0 | (cp >> 12));
        result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp <= 0x10FFFF) {
        result += static_cast<char>(0xF0 | (cp >> 18));
        result += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        throw std::invalid_argument("Invalid Unicode code point");
    }
    return result;
}

std::pair<char32_t, int> decode_codepoint(std::string_view utf8) {
    if (utf8.empty()) {
        throw std::invalid_argument("Empty UTF-8 string");
    }

    auto byte = static_cast<unsigned char>(utf8[0]);

    if (byte <= 0x7F) {
        return {byte, 1};
    }

    int len;
    char32_t cp;

    if ((byte & 0xE0) == 0xC0) {
        len = 2;
        cp = byte & 0x1F;
    } else if ((byte & 0xF0) == 0xE0) {
        len = 3;
        cp = byte & 0x0F;
    } else if ((byte & 0xF8) == 0xF0) {
        len = 4;
        cp = byte & 0x07;
    } else {
        throw std::invalid_argument("Invalid UTF-8 start byte");
    }

    if (static_cast<int>(utf8.size()) < len) {
        throw std::invalid_argument("Truncated UTF-8 sequence");
    }

    for (int i = 1; i < len; ++i) {
        auto cont = static_cast<unsigned char>(utf8[i]);
        if ((cont & 0xC0) != 0x80) {
            throw std::invalid_argument("Invalid UTF-8 continuation byte");
        }
        cp = (cp << 6) | (cont & 0x3F);
    }

    return {cp, len};
}

std::u32string utf8_to_utf32(std::string_view utf8) {
    std::u32string result;
    result.reserve(utf8.size()); // overestimate is fine

    size_t pos = 0;
    while (pos < utf8.size()) {
        auto [cp, len] = decode_codepoint(utf8.substr(pos));
        result += cp;
        pos += len;
    }

    return result;
}

std::string utf32_to_utf8(std::u32string_view utf32) {
    std::string result;
    result.reserve(utf32.size() * 2); // rough estimate

    for (char32_t cp : utf32) {
        result += encode_codepoint(cp);
    }

    return result;
}

bool is_arabic(char32_t cp) {
    return (cp >= 0x0600 && cp <= 0x06FF) ||  // Arabic
           (cp >= 0x0750 && cp <= 0x077F) ||  // Arabic Supplement
           (cp >= 0xFB50 && cp <= 0xFDFF) ||  // Arabic Presentation Forms-A
           (cp >= 0xFE70 && cp <= 0xFEFF);    // Arabic Presentation Forms-B
}

} // namespace laren::util
