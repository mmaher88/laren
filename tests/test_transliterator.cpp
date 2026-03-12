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

TEST_F(TransliteratorTest, RepeatedTransliterationConsistency) {
    // Transliterating the same input twice must produce identical results.
    // Guards against stateful bugs where internal state leaks between calls.
    auto candidates1 = translit.transliterate("salam");
    auto candidates2 = translit.transliterate("salam");

    ASSERT_EQ(candidates1.size(), candidates2.size())
        << "Repeated transliteration of 'salam' returned different counts";
    for (size_t i = 0; i < candidates1.size(); ++i) {
        EXPECT_EQ(candidates1[i].arabic, candidates2[i].arabic)
            << "Candidate " << i << " differs on repeated transliteration";
        EXPECT_EQ(candidates1[i].score, candidates2[i].score)
            << "Score " << i << " differs on repeated transliteration";
    }
}

TEST_F(TransliteratorTest, SequentialDifferentWords) {
    // Transliterating different words in sequence should each produce
    // correct candidates. Guards against state leaking between words.
    auto c1 = translit.transliterate("salam");
    auto c2 = translit.transliterate("ktb");
    auto c3 = translit.transliterate("salam"); // repeat first word

    // Each should have candidates
    ASSERT_GT(c1.size(), 0u) << "No candidates for 'salam' (first call)";
    ASSERT_GT(c2.size(), 0u) << "No candidates for 'ktb'";
    ASSERT_GT(c3.size(), 0u) << "No candidates for 'salam' (after 'ktb')";

    // سلام should be in first and third results
    auto has_salam = [](const std::vector<Candidate>& cands) {
        for (const auto& c : cands) {
            if (c.arabic == utf8_to_utf32("سلام")) return true;
        }
        return false;
    };
    EXPECT_TRUE(has_salam(c1)) << "سلام missing from first 'salam' call";
    EXPECT_TRUE(has_salam(c3)) << "سلام missing after intervening 'ktb' call";

    // كتب or كتاب should be in second result
    bool found_ktb = false;
    for (const auto& c : c2) {
        auto utf8 = utf32_to_utf8(c.arabic);
        if (utf8.find("\xd9\x83\xd8\xaa\xd8\xa8") != std::string::npos ||
            utf8.find("\xd9\x83\xd8\xaa\xd8\xa7\xd8\xa8") != std::string::npos) {
            found_ktb = true;
            break;
        }
    }
    EXPECT_TRUE(found_ktb) << "Neither كتاب nor كتب found for 'ktb'";

    // Results for 'salam' should be identical regardless of intervening calls
    ASSERT_EQ(c1.size(), c3.size())
        << "'salam' produced different candidate counts before/after 'ktb'";
    for (size_t i = 0; i < c1.size(); ++i) {
        EXPECT_EQ(c1[i].arabic, c3[i].arabic)
            << "Candidate " << i << " differs for 'salam' before/after 'ktb'";
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
