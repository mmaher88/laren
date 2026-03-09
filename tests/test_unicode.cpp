#include <gtest/gtest.h>
#include "util/unicode.h"

using namespace laren::util;

TEST(Unicode, Utf8ToUtf32_Ascii) {
    auto result = utf8_to_utf32("hello");
    ASSERT_EQ(result.size(), 5);
    EXPECT_EQ(result[0], U'h');
    EXPECT_EQ(result[4], U'o');
}

TEST(Unicode, Utf8ToUtf32_Arabic) {
    // سلام = 4 characters
    auto result = utf8_to_utf32("سلام");
    ASSERT_EQ(result.size(), 4);
    EXPECT_EQ(result[0], U'\u0633'); // س
    EXPECT_EQ(result[1], U'\u0644'); // ل
    EXPECT_EQ(result[2], U'\u0627'); // ا
    EXPECT_EQ(result[3], U'\u0645'); // م
}

TEST(Unicode, Utf32ToUtf8_Roundtrip) {
    std::string original = "مرحبا";
    auto utf32 = utf8_to_utf32(original);
    auto back = utf32_to_utf8(utf32);
    EXPECT_EQ(original, back);
}

TEST(Unicode, Utf32ToUtf8_Empty) {
    EXPECT_EQ(utf8_to_utf32("").size(), 0);
    EXPECT_EQ(utf32_to_utf8(U""), "");
}

TEST(Unicode, IsArabic) {
    EXPECT_TRUE(is_arabic(U'\u0633'));  // س
    EXPECT_TRUE(is_arabic(U'\u0639'));  // ع
    EXPECT_FALSE(is_arabic(U'a'));
    EXPECT_FALSE(is_arabic(U'3'));
}
