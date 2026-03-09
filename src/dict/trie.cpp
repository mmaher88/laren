#include "dict/trie.h"
#include <algorithm>

namespace laren::dict {

Trie::Trie() {
    // Node 0 is root
    nodes_.emplace_back();
}

Trie::~Trie() = default;

Trie::Trie(Trie&&) noexcept = default;
Trie& Trie::operator=(Trie&&) noexcept = default;

uint32_t Trie::Node::find_child(char32_t ch) const {
    // Binary search since children are sorted
    auto it = std::lower_bound(children.begin(), children.end(), ch,
        [](const Child& child, char32_t c) { return child.ch < c; });

    if (it != children.end() && it->ch == ch) {
        return it->node_index;
    }
    return UINT32_MAX;
}

uint32_t Trie::get_or_create_child(uint32_t node_idx, char32_t ch) {
    auto& node = nodes_[node_idx];

    auto it = std::lower_bound(node.children.begin(), node.children.end(), ch,
        [](const Node::Child& child, char32_t c) { return child.ch < c; });

    if (it != node.children.end() && it->ch == ch) {
        return it->node_index;
    }

    uint32_t new_idx = static_cast<uint32_t>(nodes_.size());
    nodes_.emplace_back();

    // Re-fetch reference since emplace_back may have reallocated
    auto& node_ref = nodes_[node_idx];
    auto insert_it = std::lower_bound(node_ref.children.begin(), node_ref.children.end(), ch,
        [](const Node::Child& child, char32_t c) { return child.ch < c; });
    node_ref.children.insert(insert_it, Node::Child{ch, new_idx});

    return new_idx;
}

void Trie::insert(const std::u32string& word, uint32_t frequency) {
    uint32_t current = 0; // root
    for (char32_t ch : word) {
        current = get_or_create_child(current, ch);
    }

    if (!nodes_[current].is_terminal) {
        word_count_++;
    }
    nodes_[current].is_terminal = true;
    nodes_[current].frequency = frequency;
}

std::optional<uint32_t> Trie::lookup(const std::u32string& word) const {
    const Node* node = find_node(word);
    if (node && node->is_terminal) {
        return node->frequency;
    }
    return std::nullopt;
}

bool Trie::has_prefix(const std::u32string& prefix) const {
    return find_node(prefix) != nullptr;
}

const Trie::Node* Trie::find_node(const std::u32string& prefix) const {
    uint32_t current = 0; // root
    for (char32_t ch : prefix) {
        uint32_t child = nodes_[current].find_child(ch);
        if (child == UINT32_MAX) {
            return nullptr;
        }
        current = child;
    }
    return &nodes_[current];
}

std::vector<Trie::PrefixMatch> Trie::prefix_search(
    const std::u32string& prefix, size_t max_results) const {

    std::vector<PrefixMatch> results;
    const Node* node = find_node(prefix);
    if (!node) {
        return results;
    }

    // Find the index of this node
    uint32_t node_idx = static_cast<uint32_t>(node - nodes_.data());

    std::u32string current = prefix;
    collect_words(node_idx, current, results, max_results);

    // Sort by frequency descending
    std::sort(results.begin(), results.end(),
              [](const PrefixMatch& a, const PrefixMatch& b) {
                  return a.frequency > b.frequency;
              });

    if (results.size() > max_results) {
        results.resize(max_results);
    }

    return results;
}

void Trie::collect_words(uint32_t node_idx, std::u32string& current,
                          std::vector<PrefixMatch>& results,
                          size_t max_results) const {
    if (results.size() >= max_results * 2) { // collect extra, then sort+trim
        return;
    }

    const auto& node = nodes_[node_idx];

    if (node.is_terminal) {
        results.push_back({current, node.frequency});
    }

    for (const auto& child : node.children) {
        current.push_back(child.ch);
        collect_words(child.node_index, current, results, max_results);
        current.pop_back();
    }
}

} // namespace laren::dict
