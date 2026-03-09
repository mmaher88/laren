#include <gtest/gtest.h>
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
