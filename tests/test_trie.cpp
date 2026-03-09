#include <gtest/gtest.h>
#include "dict/trie.h"
#include "util/unicode.h"

using namespace laren::dict;
using namespace laren::util;

class TrieTest : public ::testing::Test {
protected:
    Trie trie;

    void SetUp() override {
        trie.insert(utf8_to_utf32("سلام"), 9500);
        trie.insert(utf8_to_utf32("سلم"), 7200);
        trie.insert(utf8_to_utf32("سلاح"), 3800);
        trie.insert(utf8_to_utf32("كتاب"), 6500);
        trie.insert(utf8_to_utf32("كتب"), 5800);
    }
};

TEST_F(TrieTest, ExactLookup) {
    auto freq = trie.lookup(utf8_to_utf32("سلام"));
    ASSERT_TRUE(freq.has_value());
    EXPECT_EQ(*freq, 9500u);
}

TEST_F(TrieTest, LookupMissing) {
    auto freq = trie.lookup(utf8_to_utf32("قلب"));
    EXPECT_FALSE(freq.has_value());
}

TEST_F(TrieTest, HasPrefix) {
    EXPECT_TRUE(trie.has_prefix(utf8_to_utf32("سل")));
    EXPECT_TRUE(trie.has_prefix(utf8_to_utf32("كت")));
    EXPECT_FALSE(trie.has_prefix(utf8_to_utf32("قل")));
}

TEST_F(TrieTest, PrefixSearch) {
    auto results = trie.prefix_search(utf8_to_utf32("سل"));
    ASSERT_GE(results.size(), 3u);

    // Should contain سلام, سلم, سلاح
    bool has_salam = false, has_sulam = false, has_silah = false;
    for (const auto& r : results) {
        if (r.word == utf8_to_utf32("سلام")) has_salam = true;
        if (r.word == utf8_to_utf32("سلم")) has_sulam = true;
        if (r.word == utf8_to_utf32("سلاح")) has_silah = true;
    }
    EXPECT_TRUE(has_salam);
    EXPECT_TRUE(has_sulam);
    EXPECT_TRUE(has_silah);

    // Should be sorted by frequency descending
    EXPECT_GE(results[0].frequency, results[1].frequency);
}

TEST_F(TrieTest, Size) {
    EXPECT_EQ(trie.size(), 5u);
}

TEST_F(TrieTest, InsertDuplicate) {
    trie.insert(utf8_to_utf32("سلام"), 10000);
    EXPECT_EQ(trie.size(), 5u); // count shouldn't change

    auto freq = trie.lookup(utf8_to_utf32("سلام"));
    ASSERT_TRUE(freq.has_value());
    EXPECT_EQ(*freq, 10000u); // frequency should update
}

TEST_F(TrieTest, EmptyPrefix) {
    auto results = trie.prefix_search(U"", 100);
    EXPECT_EQ(results.size(), 5u); // all words
}
