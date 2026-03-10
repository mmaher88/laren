#include "core/word_history.h"
#include "util/unicode.h"

#include <algorithm>
#include <fstream>
#include <filesystem>

namespace laren::core {

WordHistory::WordHistory() : config_{5, 5000} {}
WordHistory::WordHistory(Config config) : config_(config) {}

void WordHistory::record(const std::string& input, const std::u32string& arabic) {
    ++counter_;
    dirty_ = true;

    auto& entries = entries_[input];

    // Check if this arabic word already exists for this input
    for (auto& entry : entries) {
        if (entry.arabic == arabic) {
            entry.counter = counter_;
            entry.use_count++;
            // Re-sort: most recent first
            std::sort(entries.begin(), entries.end(),
                      [](const HistoryEntry& a, const HistoryEntry& b) {
                          return a.counter > b.counter;
                      });
            return;
        }
    }

    // New entry
    entries.push_back({arabic, counter_, 1});
    std::sort(entries.begin(), entries.end(),
              [](const HistoryEntry& a, const HistoryEntry& b) {
                  return a.counter > b.counter;
              });

    // Trim if over per-key limit
    if (entries.size() > config_.max_entries_per_key) {
        entries.resize(config_.max_entries_per_key);
    }

    evict_if_full();
}

std::vector<HistoryEntry> WordHistory::get(const std::string& input) const {
    auto it = entries_.find(input);
    if (it != entries_.end()) {
        return it->second;
    }
    return {};
}

void WordHistory::evict_if_full() {
    if (entries_.size() <= config_.max_total_keys) return;

    // Find the key with the oldest max counter
    std::string oldest_key;
    uint64_t oldest_counter = UINT64_MAX;

    for (const auto& [key, entries] : entries_) {
        if (!entries.empty() && entries[0].counter < oldest_counter) {
            oldest_counter = entries[0].counter;
            oldest_key = key;
        }
    }

    if (!oldest_key.empty()) {
        entries_.erase(oldest_key);
    }
}

bool WordHistory::load(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return false;

    entries_.clear();
    counter_ = 0;

    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;

        // Parse: latin\tarabic[\tcounter\tuse_count]
        auto tab1 = line.find('\t');
        if (tab1 == std::string::npos) continue;

        std::string latin = line.substr(0, tab1);
        std::string rest = line.substr(tab1 + 1);

        auto tab2 = rest.find('\t');
        std::string arabic_utf8;
        uint64_t entry_counter = 0;
        uint32_t use_count = 1;

        if (tab2 == std::string::npos) {
            // Old 2-column format: latin\tarabic
            arabic_utf8 = rest;
            entry_counter = ++counter_;
        } else {
            arabic_utf8 = rest.substr(0, tab2);
            std::string rest2 = rest.substr(tab2 + 1);

            auto tab3 = rest2.find('\t');
            if (tab3 != std::string::npos) {
                entry_counter = std::stoull(rest2.substr(0, tab3));
                use_count = static_cast<uint32_t>(std::stoul(rest2.substr(tab3 + 1)));
            } else {
                entry_counter = std::stoull(rest2);
            }

            if (entry_counter > counter_) {
                counter_ = entry_counter;
            }
        }

        auto arabic = util::utf8_to_utf32(arabic_utf8);
        auto& entries = entries_[latin];

        // Avoid duplicates
        bool found = false;
        for (auto& e : entries) {
            if (e.arabic == arabic) {
                if (entry_counter > e.counter) e.counter = entry_counter;
                e.use_count += use_count;
                found = true;
                break;
            }
        }
        if (!found) {
            entries.push_back({arabic, entry_counter, use_count});
        }
    }

    // Sort each key's entries by recency
    for (auto& [key, entries] : entries_) {
        std::sort(entries.begin(), entries.end(),
                  [](const HistoryEntry& a, const HistoryEntry& b) {
                      return a.counter > b.counter;
                  });
        // Trim
        if (entries.size() > config_.max_entries_per_key) {
            entries.resize(config_.max_entries_per_key);
        }
    }

    dirty_ = false;
    return true;
}

bool WordHistory::save(const std::string& path) const {
    std::filesystem::create_directories(
        std::filesystem::path(path).parent_path());

    std::ofstream f(path);
    if (!f.is_open()) return false;

    for (const auto& [latin, entries] : entries_) {
        for (const auto& entry : entries) {
            f << latin << '\t'
              << util::utf32_to_utf8(entry.arabic) << '\t'
              << entry.counter << '\t'
              << entry.use_count << '\n';
        }
    }

    return true;
}

} // namespace laren::core
