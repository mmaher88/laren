#include "dict/dictionary.h"
#include "util/unicode.h"

#include <fstream>
#include <sstream>

namespace laren::dict {

bool Dictionary::load_tsv(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;

        // Parse: word<tab>frequency
        auto tab_pos = line.find('\t');
        if (tab_pos == std::string::npos) continue;

        std::string word_utf8 = line.substr(0, tab_pos);
        uint32_t freq = 0;
        try {
            freq = static_cast<uint32_t>(std::stoul(line.substr(tab_pos + 1)));
        } catch (...) {
            continue; // skip malformed lines
        }

        auto word = util::utf8_to_utf32(word_utf8);
        trie_.insert(word, freq);
    }

    return true;
}

std::optional<uint32_t> Dictionary::lookup(const std::u32string& word) const {
    return trie_.lookup(word);
}

bool Dictionary::has_prefix(const std::u32string& prefix) const {
    return trie_.has_prefix(prefix);
}

std::vector<core::Candidate> Dictionary::find_candidates(
    const std::u32string& skeletal_form, size_t max_results) const {

    std::vector<core::Candidate> candidates;

    // Exact match
    auto freq = trie_.lookup(skeletal_form);
    if (freq) {
        core::Candidate c;
        c.arabic = skeletal_form;
        c.frequency = *freq;
        c.exact_match = true;
        candidates.push_back(std::move(c));
    }

    // Prefix matches (words that start with this form)
    auto prefix_matches = trie_.prefix_search(skeletal_form, max_results);
    for (const auto& match : prefix_matches) {
        if (match.word == skeletal_form) continue; // already added as exact
        core::Candidate c;
        c.arabic = match.word;
        c.frequency = match.frequency;
        c.exact_match = false;
        candidates.push_back(std::move(c));
    }

    return candidates;
}

} // namespace laren::dict
