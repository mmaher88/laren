#include <gtest/gtest.h>
#include "core/ranker.h"

using namespace laren::core;

TEST(Ranker, SortsByFrequency) {
    Ranker ranker;

    std::vector<Candidate> candidates = {
        {U"word1", 0.0, 100, false},
        {U"word2", 0.0, 5000, false},
        {U"word3", 0.0, 1000, false},
    };

    ranker.rank(candidates);

    EXPECT_EQ(candidates[0].arabic, U"word2"); // highest freq
    EXPECT_EQ(candidates[1].arabic, U"word3");
    EXPECT_EQ(candidates[2].arabic, U"word1"); // lowest freq
}

TEST(Ranker, ExactMatchBonus) {
    Ranker ranker;

    std::vector<Candidate> candidates = {
        {U"word1", 0.0, 5000, false},
        {U"word2", 0.0, 100, true},  // low freq but exact match
    };

    ranker.rank(candidates);

    // exact match bonus (100.0) should overcome the frequency difference
    EXPECT_EQ(candidates[0].arabic, U"word2");
}

TEST(Ranker, EmptyCandidates) {
    Ranker ranker;
    std::vector<Candidate> candidates;
    ranker.rank(candidates);
    EXPECT_TRUE(candidates.empty());
}
