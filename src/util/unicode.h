#pragma once

#include <string>
#include <string_view>

namespace laren::util {

// Convert UTF-8 to UTF-32
std::u32string utf8_to_utf32(std::string_view utf8);

// Convert UTF-32 to UTF-8
std::string utf32_to_utf8(std::u32string_view utf32);

// Encode a single Unicode code point to UTF-8
std::string encode_codepoint(char32_t cp);

// Decode first code point from UTF-8, return bytes consumed
std::pair<char32_t, int> decode_codepoint(std::string_view utf8);

// Check if character is Arabic script (U+0600..U+06FF, U+0750..U+077F, U+FB50..U+FDFF, U+FE70..U+FEFF)
bool is_arabic(char32_t cp);

} // namespace laren::util
