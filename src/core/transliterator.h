#pragma once

#include "core/candidate.h"
#include "core/rule_engine.h"
#include "core/ranker.h"
#include "dict/dictionary.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace laren::core {

// The main transliteration pipeline: normalize -> expand -> filter -> rank
class Transliterator {
public:
    struct Config {
        size_t max_candidates = 7;       // Max candidates to return
        size_t max_expansions = 64;      // Max skeletal forms from rule engine
        bool use_dictionary = true;      // Filter through dictionary
    };

    Transliterator() = default;
    explicit Transliterator(Config config);

    // Set the dictionary (ownership shared)
    void set_dictionary(std::shared_ptr<dict::Dictionary> dict);

    // Main entry point: convert Arabizi input to ranked Arabic candidates
    std::vector<Candidate> transliterate(std::string_view input) const;

    // Normalize the input (lowercase, collapse repeats)
    static std::string normalize(std::string_view input);

private:
    Config config_;
    RuleEngine rule_engine_;
    Ranker ranker_;
    std::shared_ptr<dict::Dictionary> dictionary_;

    // Deduplicate candidates by Arabic text
    static void deduplicate(std::vector<Candidate>& candidates);
};

} // namespace laren::core
