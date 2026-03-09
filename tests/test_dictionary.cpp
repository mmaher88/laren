#include <gtest/gtest.h>
#include "dict/dictionary.h"
#include "util/unicode.h"

using namespace laren::dict;
using namespace laren::util;

class DictionaryTest : public ::testing::Test {
protected:
    Dictionary dict;

    void SetUp() override {
        ASSERT_TRUE(dict.load_tsv("test_data/small_dictionary.tsv"));
    }
};

TEST_F(DictionaryTest, LoadAndLookup) {
    auto freq = dict.lookup(utf8_to_utf32("سلام"));
    ASSERT_TRUE(freq.has_value());
    EXPECT_EQ(*freq, 9500u);
}

TEST_F(DictionaryTest, LookupMissing) {
    auto freq = dict.lookup(utf8_to_utf32("xyz"));
    EXPECT_FALSE(freq.has_value());
}

TEST_F(DictionaryTest, HasPrefix) {
    EXPECT_TRUE(dict.has_prefix(utf8_to_utf32("سل")));
}

TEST_F(DictionaryTest, FindCandidates) {
    auto candidates = dict.find_candidates(utf8_to_utf32("سلام"));
    ASSERT_GE(candidates.size(), 1u);

    // First should be the exact match
    bool found_exact = false;
    for (const auto& c : candidates) {
        if (c.arabic == utf8_to_utf32("سلام") && c.exact_match) {
            found_exact = true;
        }
    }
    EXPECT_TRUE(found_exact);
}

TEST_F(DictionaryTest, Size) {
    EXPECT_GT(dict.size(), 0u);
}
