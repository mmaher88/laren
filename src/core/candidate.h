#pragma once

#include <string>
#include <cstdint>

namespace laren::core {

struct Candidate {
    std::u32string arabic;     // The Arabic text
    double score = 0.0;        // Ranking score (higher = better)
    uint32_t frequency = 0;    // Corpus frequency
    bool exact_match = false;  // Skeletal form matched dictionary exactly
};

} // namespace laren::core
