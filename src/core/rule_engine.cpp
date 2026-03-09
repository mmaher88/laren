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
} // namespace ar

RuleEngine::RuleEngine() {
    // Define mappings. Order doesn't matter here — we sort longest-first.
    // Multi-character tokens (longest first in matching)
    mappings_ = {
        // 3-char tokens
        {"3'",  {ar::GHAIN}},
        {"7'",  {ar::KHA}},
        {"6'",  {ar::DHAA}},
        {"9'",  {ar::DAD}},
        {"d'",  {ar::DHAL}},
        {"t'",  {ar::THA}},
        {"s'",  {ar::SHEEN}},

        // Digraphs
        {"sh",  {ar::SHEEN}},
        {"kh",  {ar::KHA}},
        {"gh",  {ar::GHAIN}},
        {"th",  {ar::THA, ar::DHAL}},
        {"dh",  {ar::DHAL, ar::DAD}},
        {"aa",  {ar::ALEF, ar::ALEF_MADDA}},
        {"oo",  {ar::WAW}},
        {"ee",  {ar::YA}},
        {"ou",  {ar::WAW}},
        {"3h",  {ar::GHAIN}},

        // Doubled consonants — Arabizi often doubles letters for emphasis
        // but Arabic uses a single letter (with optional shadda diacritical)
        {"bb",  {ar::BA}},
        {"tt",  {ar::TA, ar::TAA}},
        {"dd",  {ar::DAL, ar::DAD}},
        {"rr",  {ar::RA}},
        {"ss",  {ar::SEEN, ar::SAD}},
        {"ff",  {ar::FA}},
        {"kk",  {ar::KAF}},
        {"ll",  {ar::LAM}},
        {"mm",  {ar::MEEM}},
        {"nn",  {ar::NOON}},
        {"yy",  {ar::YA}},
        {"ww",  {ar::WAW}},
        {"zz",  {ar::ZAYN}},
        {"gg",  {ar::JEEM}},
        {"hh",  {ar::HA}},

        // Single numbers
        {"2",   {ar::HAMZA, ar::ALEF_HAMZA_ABOVE, ar::ALEF_HAMZA_BELOW, ar::QAF}},
        {"3",   {ar::AIN}},
        {"5",   {ar::KHA}},
        {"6",   {ar::TAA}},
        {"7",   {ar::HAA}},
        {"8",   {ar::QAF}},
        {"9",   {ar::SAD}},

        // Single letters — primary mapping first, alternates after
        // Vowels include SKIP (0) to represent short vowels that aren't
        // written in Arabic script. SKIP is tried first since short vowels
        // are more common than explicit alef/waw/ya in medial position.
        {"a",   {ar::SKIP, ar::ALEF, ar::ALEF_HAMZA_ABOVE, ar::ALEF_MAQSURA, ar::HA, ar::TA_MARBUTA}},
        {"b",   {ar::BA}},
        {"t",   {ar::TA, ar::TAA}},
        {"g",   {ar::JEEM}},
        {"j",   {ar::JEEM}},
        {"d",   {ar::DAL, ar::DAD}},
        {"r",   {ar::RA}},
        {"z",   {ar::ZAYN, ar::DHAA, ar::DHAL}},
        {"s",   {ar::SEEN, ar::SAD}},
        {"f",   {ar::FA}},
        {"q",   {ar::QAF}},
        {"k",   {ar::QAF, ar::KAF}},
        {"l",   {ar::LAM}},
        {"m",   {ar::MEEM}},
        {"n",   {ar::NOON}},
        {"h",   {ar::HA, ar::HAA}},
        {"w",   {ar::WAW}},
        {"y",   {ar::YA}},
        {"i",   {ar::SKIP, ar::YA, ar::ALEF, ar::ALEF_HAMZA_ABOVE, ar::ALEF_HAMZA_BELOW}},
        {"u",   {ar::SKIP, ar::WAW, ar::ALEF_HAMZA_ABOVE}},
        {"o",   {ar::SKIP, ar::WAW, ar::ALEF_HAMZA_ABOVE}},
        {"e",   {ar::SKIP, ar::YA, ar::ALEF, ar::ALEF_HAMZA_ABOVE, ar::ALEF_HAMZA_BELOW}},
        {"p",   {ar::BA}},
        {"v",   {ar::FA}},
        {"x",   {ar::KHA}},
        {"c",   {ar::KAF}},
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
            for (char32_t ar_char : entry.arabic) {
                if (ar_char == 0) {
                    // SKIP: vowel is short, don't append anything
                    expand_dfs(input, pos + len, current, results, max_results);
                } else {
                    current.push_back(ar_char);
                    expand_dfs(input, pos + len, current, results, max_results);
                    current.pop_back();
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
