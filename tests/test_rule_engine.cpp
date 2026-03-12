#include <gtest/gtest.h>
#include <algorithm>
#include "core/rule_engine.h"
#include "util/unicode.h"

using namespace laren::core;
using namespace laren::util;

class RuleEngineTest : public ::testing::Test {
protected:
    RuleEngine engine;
};

// Helper to check if a specific Arabic string is among the expansions
bool contains_expansion(const std::vector<std::u32string>& results,
                        const std::u32string& expected) {
    for (const auto& r : results) {
        if (r == expected) return true;
    }
    return false;
}

TEST_F(RuleEngineTest, SimpleConsonants) {
    auto results = engine.expand("ktb");
    // k=ك, t=ت|ط, b=ب
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("كتب")));
}

TEST_F(RuleEngineTest, NumberMappings_3_Ain) {
    auto results = engine.expand("3rb");
    // 3=ع, r=ر, b=ب
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("عرب")));
}

TEST_F(RuleEngineTest, NumberMappings_7_Haa) {
    auto results = engine.expand("7bb");
    // 7=ح, b=ب, b=ب
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("حبب")));
}

TEST_F(RuleEngineTest, Digraph_Sh) {
    auto results = engine.expand("shkr");
    // sh=ش, k=ك, r=ر
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("شكر")));
}

TEST_F(RuleEngineTest, Digraph_Kh) {
    auto results = engine.expand("khr");
    // kh=خ, r=ر
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("خر")));
}

TEST_F(RuleEngineTest, ApostropheVariant_3Prime) {
    auto results = engine.expand("3'rb");
    // 3'=غ, r=ر, b=ب
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("غرب")));
}

TEST_F(RuleEngineTest, VowelsIncluded) {
    auto results = engine.expand("salam");
    // s=س|ص, a=ا, l=ل, a=ا, m=م
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("سالام")));
}

TEST_F(RuleEngineTest, Number9_Sad) {
    auto results = engine.expand("9ba7");
    // 9=ص, b=ب, a=ا, 7=ح
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("صباح")));
}

TEST_F(RuleEngineTest, CaseInsensitive) {
    auto results_lower = engine.expand("salam");
    auto results_upper = engine.expand("SALAM");
    EXPECT_EQ(results_lower.size(), results_upper.size());
}

TEST_F(RuleEngineTest, EmptyInput) {
    auto results = engine.expand("");
    ASSERT_EQ(results.size(), 1); // Empty string is a valid expansion
    EXPECT_TRUE(results[0].empty());
}

TEST_F(RuleEngineTest, MaxResultsRespected) {
    // An ambiguous input should still respect max_results
    auto results = engine.expand("salam", 4);
    EXPECT_LE(results.size(), 4u);
}

TEST_F(RuleEngineTest, LongWord) {
    auto results = engine.expand("7abibi");
    // 7=ح, a=ا, b=ب, i=ي, b=ب, i=ي
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("حابيبي")));
}

TEST_F(RuleEngineTest, DoubleVowel_Ee) {
    auto results = engine.expand("been");
    // ee=ي, n=ن  OR  b=ب, ee=ي, n=ن
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("بين")));
}

TEST_F(RuleEngineTest, Th_Ambiguous) {
    auto results = engine.expand("th");
    // th -> ث or ذ
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("ث")));
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("ذ")));
}

// --- Initial vowel skip tests ---
// Arabic words always start with a consonant letter (alif/hamza for vowels),
// so the SKIP option must never be used at position 0.

TEST_F(RuleEngineTest, InitialVowel_A_NeverSkipped) {
    auto results = engine.expand("ahlan");
    // Every expansion must start with an Arabic letter, never be empty at pos 0
    for (const auto& r : results) {
        EXPECT_FALSE(r.empty()) << "Expansion of 'ahlan' produced empty string";
        // First char must be an Arabic letter (alif variant, ha, ta marbuta),
        // not a consonant that would result from skipping 'a'
    }
    // Specifically: if 'a' is skipped, we'd get expansions starting with h->ه or h->ح
    // without any 'a' mapping. Count of results should be less than if skip were allowed.
    // More directly: "ahlan" with skip at pos 0 would produce forms like "هلان"
    // which should NOT appear (no alif/hamza prefix).
    // Check that all results start with a char from 'a' mapping set
    std::vector<char32_t> a_chars = {U'\u0627', U'\u0623', U'\u0649', U'\u0647', U'\u0629'};
    for (const auto& r : results) {
        ASSERT_FALSE(r.empty());
        bool starts_with_a = std::find(a_chars.begin(), a_chars.end(), r[0]) != a_chars.end();
        EXPECT_TRUE(starts_with_a)
            << "Expansion of 'ahlan' starts with unexpected char (initial vowel may have been skipped)";
    }
}

TEST_F(RuleEngineTest, InitialVowel_I_NeverSkipped) {
    auto results = engine.expand("ibn");
    std::vector<char32_t> i_chars = {U'\u064A', U'\u0627', U'\u0623', U'\u0625'};
    for (const auto& r : results) {
        ASSERT_FALSE(r.empty()) << "Expansion of 'ibn' produced empty string";
        bool starts_with_i = std::find(i_chars.begin(), i_chars.end(), r[0]) != i_chars.end();
        EXPECT_TRUE(starts_with_i)
            << "Expansion of 'ibn' starts with unexpected char (initial vowel may have been skipped)";
    }
}

TEST_F(RuleEngineTest, InitialVowel_U_NeverSkipped) {
    auto results = engine.expand("umm");
    std::vector<char32_t> u_chars = {U'\u0648', U'\u0623'};
    for (const auto& r : results) {
        ASSERT_FALSE(r.empty()) << "Expansion of 'umm' produced empty string";
        bool starts_with_u = std::find(u_chars.begin(), u_chars.end(), r[0]) != u_chars.end();
        EXPECT_TRUE(starts_with_u)
            << "Expansion of 'umm' starts with unexpected char (initial vowel may have been skipped)";
    }
}

TEST_F(RuleEngineTest, InitialVowel_E_NeverSkipped) {
    auto results = engine.expand("el");
    std::vector<char32_t> e_chars = {U'\u064A', U'\u0627', U'\u0623', U'\u0625'};
    for (const auto& r : results) {
        ASSERT_FALSE(r.empty()) << "Expansion of 'el' produced empty string";
        bool starts_with_e = std::find(e_chars.begin(), e_chars.end(), r[0]) != e_chars.end();
        EXPECT_TRUE(starts_with_e)
            << "Expansion of 'el' starts with unexpected char (initial vowel may have been skipped)";
    }
}

TEST_F(RuleEngineTest, InitialVowel_O_NeverSkipped) {
    auto results = engine.expand("omr");
    std::vector<char32_t> o_chars = {U'\u0648', U'\u0623'};
    for (const auto& r : results) {
        ASSERT_FALSE(r.empty()) << "Expansion of 'omr' produced empty string";
        bool starts_with_o = std::find(o_chars.begin(), o_chars.end(), r[0]) != o_chars.end();
        EXPECT_TRUE(starts_with_o)
            << "Expansion of 'omr' starts with unexpected char (initial vowel may have been skipped)";
    }
}

TEST_F(RuleEngineTest, MidWordVowelSkipStillWorks) {
    // Verify that SKIP still works for non-initial vowels
    auto results = engine.expand("salam");
    // سلام (with 'a' skipped in middle) should be present
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("سلام")))
        << "Mid-word vowel skip should still produce سلام for 'salam'";
    // سالام (no skips) should also be present
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("سالام")))
        << "Full vowel form سالام should also be present for 'salam'";
}
