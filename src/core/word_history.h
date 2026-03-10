#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace laren::core {

struct HistoryEntry {
    std::u32string arabic;
    uint64_t counter;       // Monotonic recency counter (higher = more recent)
    uint32_t use_count;     // Times selected for this input
};

class WordHistory {
public:
    struct Config {
        size_t max_entries_per_key = 5;     // Remember top N per Arabizi input
        size_t max_total_keys = 5000;       // Cap total distinct inputs tracked
    };

    WordHistory();
    explicit WordHistory(Config config);

    // Record that the user selected 'arabic' for Latin 'input'
    void record(const std::string& input, const std::u32string& arabic);

    // Get history entries for an exact Latin input, most recent first
    std::vector<HistoryEntry> get(const std::string& input) const;

    // Persistence
    bool load(const std::string& path);
    bool save(const std::string& path) const;

    bool dirty() const { return dirty_; }
    size_t size() const { return entries_.size(); }

private:
    Config config_;
    std::unordered_map<std::string, std::vector<HistoryEntry>> entries_;
    uint64_t counter_ = 0;
    bool dirty_ = false;

    void evict_if_full();
};

} // namespace laren::core
