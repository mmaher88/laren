#include "core/emoji_map.h"

#include <algorithm>
#include <fstream>

namespace laren::core {

void EmojiMap::load_tsv(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto tab = line.find('\t');
        if (tab == std::string::npos) continue;

        EmojiEntry entry;
        entry.shortcode = line.substr(0, tab);
        entry.emoji = line.substr(tab + 1);
        entries_.push_back(std::move(entry));
    }

    // Sort by shortcode for binary search prefix matching
    std::sort(entries_.begin(), entries_.end(),
              [](const EmojiEntry& a, const EmojiEntry& b) {
                  return a.shortcode < b.shortcode;
              });

    // Build exact lookup map
    for (size_t i = 0; i < entries_.size(); ++i) {
        exact_[entries_[i].shortcode] = i;
    }
}

std::vector<const EmojiEntry*> EmojiMap::find_by_prefix(
    std::string_view prefix, size_t max_results) const {

    std::vector<const EmojiEntry*> results;
    if (prefix.empty() || entries_.empty()) return results;

    // Binary search for the first entry >= prefix
    std::string prefix_str(prefix);
    auto it = std::lower_bound(entries_.begin(), entries_.end(), prefix_str,
                               [](const EmojiEntry& e, const std::string& p) {
                                   return e.shortcode < p;
                               });

    // Collect all entries that start with the prefix
    for (; it != entries_.end() && results.size() < max_results; ++it) {
        if (it->shortcode.compare(0, prefix.size(), prefix) != 0) break;
        results.push_back(&*it);
    }

    // Sort results: exact match first, then by shortcode length (shorter = more relevant)
    std::sort(results.begin(), results.end(),
              [&prefix](const EmojiEntry* a, const EmojiEntry* b) {
                  bool a_exact = (a->shortcode.size() == prefix.size());
                  bool b_exact = (b->shortcode.size() == prefix.size());
                  if (a_exact != b_exact) return a_exact > b_exact;
                  if (a->shortcode.size() != b->shortcode.size())
                      return a->shortcode.size() < b->shortcode.size();
                  return a->shortcode < b->shortcode;
              });

    return results;
}

const EmojiEntry* EmojiMap::find_exact(std::string_view shortcode) const {
    auto it = exact_.find(std::string(shortcode));
    if (it != exact_.end()) {
        return &entries_[it->second];
    }
    return nullptr;
}

} // namespace laren::core
