#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace laren::dict {

// A simple hash-map trie for Arabic word lookup.
// Stores words with associated frequency data.
// For v1 we use a straightforward in-memory trie.
// Can be replaced with a double-array trie + mmap for v2.
class Trie {
public:
    struct LookupResult {
        uint32_t frequency = 0;
        bool is_terminal = false;   // This node represents a complete word
        bool has_children = false;  // There are words with this prefix
    };

    Trie();
    ~Trie();

    Trie(const Trie&) = delete;
    Trie& operator=(const Trie&) = delete;
    Trie(Trie&&) noexcept;
    Trie& operator=(Trie&&) noexcept;

    // Insert a word with its frequency
    void insert(const std::u32string& word, uint32_t frequency);

    // Look up exact word
    std::optional<uint32_t> lookup(const std::u32string& word) const;

    // Check if any word in the trie starts with this prefix
    bool has_prefix(const std::u32string& prefix) const;

    // Find all words matching this prefix, up to max_results
    struct PrefixMatch {
        std::u32string word;
        uint32_t frequency;
    };
    std::vector<PrefixMatch> prefix_search(const std::u32string& prefix,
                                           size_t max_results = 16) const;

    // Number of words stored
    size_t size() const { return word_count_; }

private:
    struct Node {
        // Children indexed by char32_t. Using a sorted vector for
        // memory efficiency (Arabic has ~50 chars, not 65536).
        struct Child {
            char32_t ch;
            uint32_t node_index;
        };
        std::vector<Child> children;
        uint32_t frequency = 0;
        bool is_terminal = false;

        // Find child for character, returns node index or UINT32_MAX
        uint32_t find_child(char32_t ch) const;
    };

    std::vector<Node> nodes_;
    size_t word_count_ = 0;

    uint32_t get_or_create_child(uint32_t node_idx, char32_t ch);
    const Node* find_node(const std::u32string& prefix) const;

    void collect_words(uint32_t node_idx, std::u32string& current,
                       std::vector<PrefixMatch>& results,
                       size_t max_results) const;
};

} // namespace laren::dict
