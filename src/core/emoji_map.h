#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace laren::core {

struct EmojiEntry {
    std::string shortcode;
    std::string emoji; // UTF-8 emoji string
};

class EmojiMap {
public:
    void load_tsv(const std::string& path);
    size_t size() const { return entries_.size(); }

    // Find emojis whose shortcode starts with the given prefix.
    // Returns up to max_results matches, sorted by shortcode length (shortest first).
    std::vector<const EmojiEntry*> find_by_prefix(std::string_view prefix,
                                                   size_t max_results = 50) const;

    // Exact shortcode lookup
    const EmojiEntry* find_exact(std::string_view shortcode) const;

private:
    std::vector<EmojiEntry> entries_; // sorted by shortcode
    std::unordered_map<std::string, size_t> exact_; // shortcode -> index
};

} // namespace laren::core
