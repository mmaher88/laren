#include "core/ranker.h"
#include <algorithm>
#include <cmath>

namespace laren::core {

Ranker::Ranker(Weights weights) : weights_(std::move(weights)) {}

void Ranker::rank(std::vector<Candidate>& candidates) const {
    for (auto& c : candidates) {
        c.score = 0.0;

        // Log-scaled frequency
        if (c.frequency > 0) {
            c.score += weights_.frequency_weight * std::log(1.0 + c.frequency);
        }

        // Exact match bonus
        if (c.exact_match) {
            c.score += weights_.exact_match_bonus;
        }
    }

    // Sort descending by score
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) {
                  return a.score > b.score;
              });
}

} // namespace laren::core
