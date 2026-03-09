#include <gtest/gtest.h>
#include "core/transliterator.h"
#include "util/unicode.h"

using namespace laren::core;
using namespace laren::util;

class TransliteratorTest : public ::testing::Test {
protected:
    Transliterator translit;
    std::shared_ptr<laren::dict::Dictionary> dict;

    void SetUp() override {
        dict = std::make_shared<laren::dict::Dictionary>();
        dict->load_tsv("test_data/small_dictionary.tsv");
        translit.set_dictionary(dict);
    }
};

TEST(TransliteratorNormalize, Lowercase) {
    EXPECT_EQ(Transliterator::normalize("HELLO"), "hello");
}

TEST(TransliteratorNormalize, CollapseRepeats) {
    EXPECT_EQ(Transliterator::normalize("shhh"), "shh");
    EXPECT_EQ(Transliterator::normalize("hhhh"), "hh");
}

TEST(TransliteratorNormalize, PreserveDouble) {
    EXPECT_EQ(Transliterator::normalize("shh"), "shh");
}

TEST(TransliteratorNormalize, EmptyInput) {
    EXPECT_EQ(Transliterator::normalize(""), "");
}

TEST_F(TransliteratorTest, BasicTransliteration) {
    auto candidates = translit.transliterate("salam");
    ASSERT_GT(candidates.size(), 0u);

    // Should contain سلام as a candidate (from dictionary)
    bool found = false;
    for (const auto& c : candidates) {
        if (c.arabic == utf8_to_utf32("سلام")) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "Expected سلام in candidates";
}

TEST_F(TransliteratorTest, EmptyInput) {
    auto candidates = translit.transliterate("");
    EXPECT_TRUE(candidates.empty());
}

TEST_F(TransliteratorTest, NumberInput_7abibi) {
    auto candidates = translit.transliterate("7abibi");
    ASSERT_GT(candidates.size(), 0u);

    // Check that some Arabic output is generated
    for (const auto& c : candidates) {
        EXPECT_FALSE(c.arabic.empty());
    }
}

TEST_F(TransliteratorTest, MaxCandidatesRespected) {
    Transliterator::Config config;
    config.max_candidates = 3;
    Transliterator limited(config);
    limited.set_dictionary(dict);

    auto candidates = limited.transliterate("salam");
    EXPECT_LE(candidates.size(), 3u);
}

TEST_F(TransliteratorTest, CandidatesAreSorted) {
    auto candidates = translit.transliterate("ktb");
    if (candidates.size() >= 2) {
        // Scores should be descending
        EXPECT_GE(candidates[0].score, candidates[1].score);
    }
}

TEST_F(TransliteratorTest, NoDictionaryFallback) {
    Transliterator::Config config;
    config.use_dictionary = false;
    Transliterator no_dict(config);

    auto candidates = no_dict.transliterate("salam");
    ASSERT_GT(candidates.size(), 0u);
    // Should return raw skeletal forms
}
