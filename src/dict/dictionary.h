#pragma once

#include "dict/trie.h"
#include "core/candidate.h"

#include <string>
#include <vector>

namespace laren::dict {

// Dictionary wraps a Trie and provides the interface used by the Transliterator.
class Dictionary {
public:
    Dictionary() = default;

    // Load dictionary from a TSV file (word\tfrequency per line)
    bool load_tsv(const std::string& path);

    // Look up an exact Arabic word. Returns frequency or nullopt.
    std::optional<uint32_t> lookup(const std::u32string& word) const;

    // Check if any word starts with this prefix
    bool has_prefix(const std::u32string& prefix) const;

    // Find words matching a skeletal form
    std::vector<core::Candidate> find_candidates(
        const std::u32string& skeletal_form, size_t max_results = 16) const;

    size_t size() const { return trie_.size(); }

private:
    Trie trie_;
};

} // namespace laren::dict
