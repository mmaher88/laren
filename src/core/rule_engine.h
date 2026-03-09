#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <span>

namespace laren::core {

// A single mapping rule: Latin token -> possible Arabic expansions
struct MappingRule {
    std::string_view latin;                     // e.g., "sh", "3'", "a"
    std::span<const char32_t> arabic_options;   // e.g., {ش} or {س, ص}
};

// The rule engine applies Arabizi mapping rules to expand a Latin string
// into all plausible Arabic skeletal forms.
class RuleEngine {
public:
    RuleEngine();

    // Expand a normalized Latin input into Arabic skeletal forms.
    // Returns up to max_results candidates (beam-pruned).
    std::vector<std::u32string> expand(std::string_view input, size_t max_results = 32) const;

private:
    struct MappingEntry {
        std::string latin;
        std::vector<std::u32string> arabic;  // each option can be multi-char
    };

    // Sorted longest-first for greedy matching
    std::vector<MappingEntry> mappings_;

    // Recursive DFS expansion
    void expand_dfs(std::string_view input, size_t pos,
                    std::u32string& current,
                    std::vector<std::u32string>& results,
                    size_t max_results) const;
};

} // namespace laren::core
