#include "core/rule_engine.h"
#include <algorithm>

namespace laren::core {

// Arabic Unicode code points
namespace ar {
    constexpr char32_t SKIP        = 0;         // sentinel: vowel is short, omit from output
    constexpr char32_t HAMZA       = U'\u0621'; // ء
    constexpr char32_t ALEF_MADDA  = U'\u0622'; // آ
    constexpr char32_t ALEF        = U'\u0627'; // ا
    constexpr char32_t BA          = U'\u0628'; // ب
    constexpr char32_t TA_MARBUTA  = U'\u0629'; // ة
    constexpr char32_t TA          = U'\u062A'; // ت
    constexpr char32_t THA         = U'\u062B'; // ث
    constexpr char32_t JEEM        = U'\u062C'; // ج
    constexpr char32_t HAA         = U'\u062D'; // ح
    constexpr char32_t KHA         = U'\u062E'; // خ
    constexpr char32_t DAL         = U'\u062F'; // د
    constexpr char32_t DHAL        = U'\u0630'; // ذ
    constexpr char32_t RA          = U'\u0631'; // ر
    constexpr char32_t ZAYN        = U'\u0632'; // ز
    constexpr char32_t SEEN        = U'\u0633'; // س
    constexpr char32_t SHEEN       = U'\u0634'; // ش
    constexpr char32_t SAD         = U'\u0635'; // ص
    constexpr char32_t DAD         = U'\u0636'; // ض
    constexpr char32_t TAA         = U'\u0637'; // ط
    constexpr char32_t DHAA        = U'\u0638'; // ظ
    constexpr char32_t AIN         = U'\u0639'; // ع
    constexpr char32_t GHAIN       = U'\u063A'; // غ
    constexpr char32_t FA          = U'\u0641'; // ف
    constexpr char32_t QAF         = U'\u0642'; // ق
    constexpr char32_t KAF         = U'\u0643'; // ك
    constexpr char32_t LAM         = U'\u0644'; // ل
    constexpr char32_t MEEM       = U'\u0645'; // م
    constexpr char32_t NOON        = U'\u0646'; // ن
    constexpr char32_t HA          = U'\u0647'; // ه
    constexpr char32_t WAW         = U'\u0648'; // و
    constexpr char32_t YA          = U'\u064A'; // ي
    constexpr char32_t FATHA       = U'\u064E'; // َ
    constexpr char32_t KASRA       = U'\u0650'; // ِ
    constexpr char32_t DAMMA       = U'\u064F'; // ُ
    constexpr char32_t ALEF_HAMZA_ABOVE = U'\u0623'; // أ
    constexpr char32_t ALEF_HAMZA_BELOW = U'\u0625'; // إ
    constexpr char32_t ALEF_MAQSURA    = U'\u0649'; // ى
    constexpr char32_t WAW_HAMZA       = U'\u0624'; // ؤ
    constexpr char32_t YA_HAMZA        = U'\u0626'; // ئ
} // namespace ar

RuleEngine::RuleEngine() {
    // Helper: single-char options
    auto C = [](std::initializer_list<char32_t> chars) -> std::vector<std::u32string> {
        std::vector<std::u32string> v;
        for (char32_t c : chars) {
            if (c == 0) v.push_back({}); // SKIP = empty string
            else v.push_back({c});
        }
        return v;
    };

    // Define mappings. Order doesn't matter here — we sort longest-first.
    // Multi-character tokens (longest first in matching)
    mappings_ = {
        // 3-char tokens
        {"3'",  C({ar::GHAIN})},
        {"7'",  C({ar::KHA})},
        {"6'",  C({ar::DHAA})},
        {"9'",  C({ar::DAD})},
        {"d'",  C({ar::DHAL})},
        {"t'",  C({ar::THA})},
        {"s'",  C({ar::SHEEN})},

        // Digraphs
        {"sh",  C({ar::SHEEN})},
        {"kh",  C({ar::KHA})},
        {"gh",  C({ar::GHAIN})},
        {"th",  C({ar::THA, ar::DHAL})},
        {"dh",  C({ar::DHAL, ar::DAD})},
        {"aa",  C({ar::ALEF, ar::ALEF_MADDA})},
        {"oo",  C({ar::WAW})},
        {"ee",  C({ar::YA})},
        {"ou",  C({ar::WAW})},
        {"3h",  C({ar::GHAIN})},

        // Doubled consonants — Arabizi often doubles letters for emphasis
        // but Arabic uses a single letter (with optional shadda diacritical)
        {"bb",  C({ar::BA})},
        {"tt",  C({ar::TA, ar::TAA})},
        {"dd",  C({ar::DAL, ar::DAD})},
        {"rr",  C({ar::RA})},
        {"ss",  C({ar::SEEN, ar::SAD, ar::THA})},
        {"ff",  C({ar::FA})},
        {"kk",  C({ar::KAF})},
        {"ll",  C({ar::LAM})},
        {"mm",  C({ar::MEEM})},
        {"nn",  C({ar::NOON})},
        {"yy",  C({ar::YA})},
        {"ww",  C({ar::WAW})},
        {"zz",  C({ar::ZAYN})},
        {"gg",  C({ar::JEEM})},
        {"hh",  C({ar::HA})},

        // Single numbers
        {"2",   C({ar::HAMZA, ar::ALEF_HAMZA_ABOVE, ar::ALEF_HAMZA_BELOW, ar::WAW_HAMZA, ar::YA_HAMZA, ar::QAF})},
        {"3",   C({ar::AIN})},
        {"5",   C({ar::KHA})},
        {"6",   C({ar::TAA})},
        {"7",   C({ar::HAA})},
        {"8",   C({ar::QAF})},
        {"9",   C({ar::SAD})},

        // Single letters — primary mapping first, alternates after
        // Vowels include SKIP (0) to represent short vowels that aren't
        // written in Arabic script. SKIP is tried first since short vowels
        // are more common than explicit alef/waw/ya in medial position.
        {"a",   C({ar::SKIP, ar::ALEF, ar::ALEF_HAMZA_ABOVE, ar::ALEF_MAQSURA, ar::HA, ar::TA_MARBUTA})},
        {"b",   C({ar::BA})},
        {"t",   C({ar::TA, ar::TAA})},
        {"g",   C({ar::JEEM})},
        {"j",   C({ar::JEEM})},
        {"d",   C({ar::DAL, ar::DAD})},
        {"r",   C({ar::RA})},
        {"z",   C({ar::ZAYN, ar::DHAA, ar::DHAL})},
        {"s",   C({ar::SEEN, ar::SAD, ar::THA})},
        {"f",   C({ar::FA})},
        {"q",   C({ar::QAF})},
        {"k",   C({ar::QAF, ar::KAF})},
        {"l",   C({ar::LAM})},
        {"m",   C({ar::MEEM})},
        {"n",   C({ar::NOON})},
        {"h",   C({ar::HA, ar::HAA})},
        {"w",   C({ar::WAW})},
        {"y",   C({ar::YA})},
        {"i",   C({ar::SKIP, ar::YA, ar::ALEF, ar::ALEF_HAMZA_ABOVE, ar::ALEF_HAMZA_BELOW})},
        {"u",   C({ar::SKIP, ar::WAW, ar::ALEF_HAMZA_ABOVE})},
        {"o",   C({ar::SKIP, ar::WAW, ar::ALEF_HAMZA_ABOVE})},
        {"e",   {{}, {ar::YA}, {ar::ALEF}, {ar::ALEF_HAMZA_ABOVE}, {ar::ALEF_HAMZA_BELOW},
                 {ar::ALEF_HAMZA_BELOW, ar::YA}}},  // last: إي for long "ee" at word start
        {"p",   C({ar::BA})},
        {"v",   C({ar::FA})},
        {"x",   C({ar::KHA})},
        {"c",   C({ar::KAF})},
    };

    // Sort longest latin string first for greedy matching
    std::sort(mappings_.begin(), mappings_.end(),
              [](const MappingEntry& a, const MappingEntry& b) {
                  return a.latin.size() > b.latin.size();
              });
}

std::vector<std::u32string> RuleEngine::expand(std::string_view input, size_t max_results) const {
    std::vector<std::u32string> results;
    std::u32string current;
    current.reserve(input.size());
    expand_dfs(input, 0, current, results, max_results);
    return results;
}

void RuleEngine::expand_dfs(std::string_view input, size_t pos,
                             std::u32string& current,
                             std::vector<std::u32string>& results,
                             size_t max_results) const {
    if (results.size() >= max_results) {
        return;
    }

    if (pos >= input.size()) {
        results.push_back(current);
        return;
    }

    bool matched = false;

    // Try each mapping, longest first (already sorted)
    for (const auto& entry : mappings_) {
        size_t len = entry.latin.size();
        if (pos + len > input.size()) continue;

        // Check if this mapping matches at current position
        bool match = true;
        for (size_t i = 0; i < len; ++i) {
            if (std::tolower(static_cast<unsigned char>(input[pos + i])) !=
                entry.latin[i]) {
                match = false;
                break;
            }
        }

        if (match) {
            matched = true;
            // Try each possible Arabic expansion
            for (const auto& ar_option : entry.arabic) {
                if (ar_option.empty()) {
                    // SKIP: vowel is short, don't append anything.
                    // But never skip at position 0 — Arabic words always
                    // start with a consonant letter (alif/hamza for vowels).
                    if (pos == 0) continue;
                    expand_dfs(input, pos + len, current, results, max_results);
                } else {
                    current.append(ar_option);
                    expand_dfs(input, pos + len, current, results, max_results);
                    current.resize(current.size() - ar_option.size());
                }

                if (results.size() >= max_results) return;
            }
            // For greedy matching: once we've tried the longest match,
            // we still try shorter matches. This allows "th" to be tried
            // as "t" + "h" by continuing the loop.
        }
    }

    // If nothing matched, skip this character (punctuation, unknown chars)
    if (!matched) {
        expand_dfs(input, pos + 1, current, results, max_results);
    }
}

} // namespace laren::core
