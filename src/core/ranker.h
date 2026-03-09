#pragma once

#include "core/candidate.h"
#include <vector>

namespace laren::core {

class Ranker {
public:
    // Weight configuration
    struct Weights {
        double frequency_weight = 1.0;
        double exact_match_bonus = 100.0;
    };

    Ranker() = default;
    explicit Ranker(Weights weights);

    // Score and sort candidates in-place. Best candidate first.
    void rank(std::vector<Candidate>& candidates) const;

private:
    Weights weights_;
};

} // namespace laren::core
