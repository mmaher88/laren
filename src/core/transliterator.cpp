#include "core/transliterator.h"
#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace laren::core {

Transliterator::Transliterator(Config config)
    : config_(config) {}

void Transliterator::set_dictionary(std::shared_ptr<dict::Dictionary> dict) {
    dictionary_ = std::move(dict);
}

std::string Transliterator::normalize(std::string_view input) {
    std::string result;
    result.reserve(input.size());

    char prev = '\0';
    int repeat_count = 0;

    for (char ch : input) {
        char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

        // Collapse runs of 3+ identical characters to 2
        if (lower == prev) {
            repeat_count++;
            if (repeat_count >= 2) {
                continue;
            }
        } else {
            repeat_count = 0;
        }

        prev = lower;
        result += lower;
    }

    return result;
}

std::vector<Candidate> Transliterator::transliterate(std::string_view input) const {
    if (input.empty()) {
        return {};
    }

    // Stage 1: Normalize
    std::string normalized = normalize(input);

    // Stage 2: Expand via rule engine
    auto skeletal_forms = rule_engine_.expand(normalized, config_.max_expansions);

    std::vector<Candidate> all_candidates;

    // Stage 3a: Dictionary lookup — exact matches (real words)
    if (dictionary_ && config_.use_dictionary) {
        for (const auto& form : skeletal_forms) {
            auto freq = dictionary_->lookup(form);
            if (freq) {
                Candidate c;
                c.arabic = form;
                c.frequency = *freq;
                c.exact_match = true;
                all_candidates.push_back(std::move(c));
            }
        }
    }

    // Stage 3b: Fill with top skeletal forms as fallback
    // Dictionary matches come first (ranked by frequency), then
    // skeletal forms fill remaining slots for words not in dictionary.
    size_t dict_count = all_candidates.size();
    if (dict_count < config_.max_candidates) {
        // Collect arabic strings already in candidates for dedup
        struct U32Hash {
            size_t operator()(const std::u32string& s) const {
                size_t h = 0;
                for (char32_t c : s) h = h * 31 + c;
                return h;
            }
        };
        std::unordered_set<std::u32string, U32Hash> seen;
        for (const auto& c : all_candidates) {
            seen.insert(c.arabic);
        }

        size_t remaining = config_.max_candidates - dict_count;
        for (const auto& form : skeletal_forms) {
            if (remaining == 0) break;
            if (!form.empty() && !seen.count(form)) {
                Candidate c;
                c.arabic = form;
                c.frequency = 0;
                c.exact_match = false;
                all_candidates.push_back(std::move(c));
                seen.insert(form);
                remaining--;
            }
        }
    }

    // Deduplicate
    deduplicate(all_candidates);

    // Stage 4: Rank
    ranker_.rank(all_candidates);

    // Trim to max candidates
    if (all_candidates.size() > config_.max_candidates) {
        all_candidates.resize(config_.max_candidates);
    }

    return all_candidates;
}

void Transliterator::deduplicate(std::vector<Candidate>& candidates) {
    struct U32Hash {
        size_t operator()(const std::u32string& s) const {
            size_t hash = 0;
            for (char32_t c : s) {
                hash = hash * 31 + c;
            }
            return hash;
        }
    };

    std::unordered_set<std::u32string, U32Hash> seen;
    auto it = std::remove_if(candidates.begin(), candidates.end(),
        [&seen](const Candidate& c) {
            if (seen.count(c.arabic)) {
                return true;
            }
            seen.insert(c.arabic);
            return false;
        });
    candidates.erase(it, candidates.end());
}

} // namespace laren::core
