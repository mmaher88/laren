#include <gtest/gtest.h>
#include <algorithm>
#include "core/rule_engine.h"
#include "util/unicode.h"

using namespace laren::core;
using namespace laren::util;

class RuleEngineTest : public ::testing::Test {
protected:
    RuleEngine engine;
};

// Helper to check if a specific Arabic string is among the expansions
bool contains_expansion(const std::vector<std::u32string>& results,
                        const std::u32string& expected) {
    for (const auto& r : results) {
        if (r == expected) return true;
    }
    return false;
}

TEST_F(RuleEngineTest, SimpleConsonants) {
    auto results = engine.expand("ktb");
    // k=ك, t=ت|ط, b=ب
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("كتب")));
}

TEST_F(RuleEngineTest, NumberMappings_3_Ain) {
    auto results = engine.expand("3rb");
    // 3=ع, r=ر, b=ب
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("عرب")));
}

TEST_F(RuleEngineTest, NumberMappings_7_Haa) {
    auto results = engine.expand("7bb");
    // 7=ح, b=ب, b=ب
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("حبب")));
}

TEST_F(RuleEngineTest, Digraph_Sh) {
    auto results = engine.expand("shkr");
    // sh=ش, k=ك, r=ر
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("شكر")));
}

TEST_F(RuleEngineTest, Digraph_Kh) {
    auto results = engine.expand("khr");
    // kh=خ, r=ر
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("خر")));
}

TEST_F(RuleEngineTest, ApostropheVariant_3Prime) {
    auto results = engine.expand("3'rb");
    // 3'=غ, r=ر, b=ب
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("غرب")));
}

TEST_F(RuleEngineTest, VowelsIncluded) {
    auto results = engine.expand("salam");
    // s=س|ص, a=ا, l=ل, a=ا, m=م
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("سالام")));
}

TEST_F(RuleEngineTest, Number9_Sad) {
    auto results = engine.expand("9ba7");
    // 9=ص, b=ب, a=ا, 7=ح
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("صباح")));
}

TEST_F(RuleEngineTest, CaseInsensitive) {
    auto results_lower = engine.expand("salam");
    auto results_upper = engine.expand("SALAM");
    EXPECT_EQ(results_lower.size(), results_upper.size());
}

TEST_F(RuleEngineTest, EmptyInput) {
    auto results = engine.expand("");
    ASSERT_EQ(results.size(), 1); // Empty string is a valid expansion
    EXPECT_TRUE(results[0].empty());
}

TEST_F(RuleEngineTest, MaxResultsRespected) {
    // An ambiguous input should still respect max_results
    auto results = engine.expand("salam", 4);
    EXPECT_LE(results.size(), 4u);
}

TEST_F(RuleEngineTest, LongWord) {
    auto results = engine.expand("7abibi");
    // 7=ح, a=ا, b=ب, i=ي, b=ب, i=ي
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("حابيبي")));
}

TEST_F(RuleEngineTest, DoubleVowel_Ee) {
    auto results = engine.expand("been");
    // ee=ي, n=ن  OR  b=ب, ee=ي, n=ن
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("بين")));
}

TEST_F(RuleEngineTest, Th_Ambiguous) {
    auto results = engine.expand("th");
    // th -> ث or ذ
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("ث")));
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("ذ")));
}

// --- Initial vowel skip tests ---
// Arabic words always start with a consonant letter (alif/hamza for vowels),
// so the SKIP option must never be used at position 0.

TEST_F(RuleEngineTest, InitialVowel_A_NeverSkipped) {
    auto results = engine.expand("ahlan");
    // Every expansion must start with an Arabic letter, never be empty at pos 0
    for (const auto& r : results) {
        EXPECT_FALSE(r.empty()) << "Expansion of 'ahlan' produced empty string";
        // First char must be an Arabic letter (alif variant, ha, ta marbuta),
        // not a consonant that would result from skipping 'a'
    }
    // Specifically: if 'a' is skipped, we'd get expansions starting with h->ه or h->ح
    // without any 'a' mapping. Count of results should be less than if skip were allowed.
    // More directly: "ahlan" with skip at pos 0 would produce forms like "هلان"
    // which should NOT appear (no alif/hamza prefix).
    // Check that all results start with a char from 'a' mapping set
    std::vector<char32_t> a_chars = {U'\u0627', U'\u0623', U'\u0649', U'\u0647', U'\u0629'};
    for (const auto& r : results) {
        ASSERT_FALSE(r.empty());
        bool starts_with_a = std::find(a_chars.begin(), a_chars.end(), r[0]) != a_chars.end();
        EXPECT_TRUE(starts_with_a)
            << "Expansion of 'ahlan' starts with unexpected char (initial vowel may have been skipped)";
    }
}

TEST_F(RuleEngineTest, InitialVowel_I_NeverSkipped) {
    auto results = engine.expand("ibn");
    std::vector<char32_t> i_chars = {U'\u064A', U'\u0627', U'\u0623', U'\u0625'};
    for (const auto& r : results) {
        ASSERT_FALSE(r.empty()) << "Expansion of 'ibn' produced empty string";
        bool starts_with_i = std::find(i_chars.begin(), i_chars.end(), r[0]) != i_chars.end();
        EXPECT_TRUE(starts_with_i)
            << "Expansion of 'ibn' starts with unexpected char (initial vowel may have been skipped)";
    }
}

TEST_F(RuleEngineTest, InitialVowel_U_NeverSkipped) {
    auto results = engine.expand("umm");
    std::vector<char32_t> u_chars = {U'\u0648', U'\u0623'};
    for (const auto& r : results) {
        ASSERT_FALSE(r.empty()) << "Expansion of 'umm' produced empty string";
        bool starts_with_u = std::find(u_chars.begin(), u_chars.end(), r[0]) != u_chars.end();
        EXPECT_TRUE(starts_with_u)
            << "Expansion of 'umm' starts with unexpected char (initial vowel may have been skipped)";
    }
}

TEST_F(RuleEngineTest, InitialVowel_E_NeverSkipped) {
    auto results = engine.expand("el");
    std::vector<char32_t> e_chars = {U'\u064A', U'\u0627', U'\u0623', U'\u0625'};
    for (const auto& r : results) {
        ASSERT_FALSE(r.empty()) << "Expansion of 'el' produced empty string";
        bool starts_with_e = std::find(e_chars.begin(), e_chars.end(), r[0]) != e_chars.end();
        EXPECT_TRUE(starts_with_e)
            << "Expansion of 'el' starts with unexpected char (initial vowel may have been skipped)";
    }
}

TEST_F(RuleEngineTest, InitialVowel_O_NeverSkipped) {
    auto results = engine.expand("omr");
    std::vector<char32_t> o_chars = {U'\u0648', U'\u0623'};
    for (const auto& r : results) {
        ASSERT_FALSE(r.empty()) << "Expansion of 'omr' produced empty string";
        bool starts_with_o = std::find(o_chars.begin(), o_chars.end(), r[0]) != o_chars.end();
        EXPECT_TRUE(starts_with_o)
            << "Expansion of 'omr' starts with unexpected char (initial vowel may have been skipped)";
    }
}

TEST_F(RuleEngineTest, MidWordVowelSkipStillWorks) {
    // Verify that SKIP still works for non-initial vowels
    auto results = engine.expand("salam");
    // سلام (with 'a' skipped in middle) should be present
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("سلام")))
        << "Mid-word vowel skip should still produce سلام for 'salam'";
    // سالام (no skips) should also be present
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("سالام")))
        << "Full vowel form سالام should also be present for 'salam'";
}

// ============================================================
// Parameterized dictionary word expansion tests
// ============================================================
// These verify that common Arabic words from the dictionary appear
// in the rule engine's expansions for their Arabizi spellings.
// Organized by mapping feature to ensure full coverage.

struct ExpansionCase {
    std::string arabizi;
    std::string arabic;   // UTF-8
    std::string feature;
};

class RuleEngineExpansionTest
    : public ::testing::TestWithParam<ExpansionCase> {
protected:
    RuleEngine engine;
};

TEST_P(RuleEngineExpansionTest, DictionaryWordExpands) {
    const auto& tc = GetParam();
    auto results = engine.expand(tc.arabizi, 256);
    auto expected = utf8_to_utf32(tc.arabic);
    bool found = contains_expansion(results, expected);
    EXPECT_TRUE(found)
        << "expand(\"" << tc.arabizi << "\") should produce "
        << tc.arabic << " [" << tc.feature << "]";
}

INSTANTIATE_TEST_SUITE_P(DictionaryWords, RuleEngineExpansionTest,
    ::testing::Values(
    // ── A. Basic consonants + vowel skip ──
    ExpansionCase{"ktb",     "كتب",     "pure consonants b/k/t"},
    ExpansionCase{"qalb",    "قلب",     "q→ق + vowel skip"},
    ExpansionCase{"waqt",    "وقت",     "w/q/t consonants"},
    ExpansionCase{"bayt",    "بيت",     "b + vowel skip + y→ي"},
    ExpansionCase{"rajul",   "رجل",     "r/j/l + double vowel skip"},
    ExpansionCase{"walad",   "ولد",     "w/l/d + double vowel skip"},
    ExpansionCase{"yawm",    "يوم",     "y→ي + vowel skip + w→و"},
    ExpansionCase{"nafs",    "نفس",     "n/f/s + vowel skip"},
    ExpansionCase{"malik",   "ملك",     "m/l/k + double vowel skip"},
    ExpansionCase{"jamal",   "جمل",     "j→ج + double vowel skip"},
    ExpansionCase{"zaman",   "زمن",     "z→ز + double vowel skip"},
    ExpansionCase{"fajr",    "فجر",     "f→ف + vowel skip"},
    ExpansionCase{"safar",   "سفر",     "s→س + double vowel skip"},
    ExpansionCase{"layl",    "ليل",     "l→ل + vowel skip + y→ي"},
    ExpansionCase{"dar",     "دار",     "d→د + long a→ا"},
    ExpansionCase{"bab",     "باب",     "b→ب + long a→ا"},
    ExpansionCase{"maktab",  "مكتب",    "5 consonants + vowel skips"},
    ExpansionCase{"masjid",  "مسجد",    "4 consonants + vowel skips"},

    // ── B. Long vowels (aa, ee, oo) ──
    ExpansionCase{"noor",    "نور",     "oo→و digraph"},
    ExpansionCase{"kabeer",  "كبير",    "ee→ي digraph + vowel skip"},
    ExpansionCase{"jadeed",  "جديد",    "ee→ي digraph + vowel skip"},
    ExpansionCase{"jameel",  "جميل",    "ee→ي digraph"},
    ExpansionCase{"ktab",    "كتاب",    "a→ا long vowel"},
    ExpansionCase{"salam",   "سلام",    "mid-vowel skip + long a→ا"},
    ExpansionCase{"kitab",   "كتاب",    "i→skip + a→ا long vowel"},

    // ── C. Number 3 → AIN (ع) ──
    ExpansionCase{"3lm",     "علم",     "3→ع knowledge"},
    ExpansionCase{"3nd",     "عند",     "3→ع at"},
    ExpansionCase{"ba3d",    "بعد",     "3→ع medial"},
    ExpansionCase{"na3am",   "نعم",     "3→ع + double vowel skip"},
    ExpansionCase{"3amal",   "عمل",     "3→ع + double vowel skip"},
    ExpansionCase{"3arabi",  "عربي",    "3→ع + i→ي"},
    ExpansionCase{"sam3",    "سمع",     "3→ع final"},
    ExpansionCase{"gamee3",  "جميع",    "3→ع final + ee→ي"},

    // ── D. Number 7 → HAA (ح) ──
    ExpansionCase{"7aq",     "حق",      "7→ح + q→ق"},
    ExpansionCase{"7awl",    "حول",     "7→ح + w→و"},
    ExpansionCase{"sa7ee7",  "صحيح",    "7→ح medial+final + s→ص"},
    ExpansionCase{"sba7",    "صباح",    "7→ح final + s→ص"},
    ExpansionCase{"7abibi",  "حبيبي",   "7→ح + repeated b/i"},
    ExpansionCase{"7arb",    "حرب",     "7→ح + vowel skip"},
    ExpansionCase{"7asan",   "حسن",     "7→ح + double vowel skip"},

    // ── E. Number 2 → HAMZA variants / QAF ──
    ExpansionCase{"2alb",    "قلب",     "2→ق"},
    ExpansionCase{"ma2",     "ماء",     "2→ء hamza final"},
    ExpansionCase{"2amar",   "قمر",     "2→ق + double vowel skip"},
    ExpansionCase{"so2al",   "سؤال",    "2→ؤ waw-hamza"},

    // ── F. Number 5 → KHA (خ) ──
    ExpansionCase{"5ayr",    "خير",     "5→خ + vowel skip + y→ي"},
    ExpansionCase{"5abar",   "خبر",     "5→خ + double vowel skip"},

    // ── G. Number 6 → TAA (ط) ──
    ExpansionCase{"6aree2",  "طريق",    "6→ط + ee→ي + 2→ق"},
    ExpansionCase{"6alab",   "طلب",     "6→ط + double vowel skip"},

    // ── H. Number 8 → QAF (ق) ──
    ExpansionCase{"8alb",    "قلب",     "8→ق"},
    ExpansionCase{"8amar",   "قمر",     "8→ق + double vowel skip"},

    // ── I. Number 9 → SAD (ص) ──
    ExpansionCase{"9ba7",    "صباح",    "9→ص + 7→ح"},
    ExpansionCase{"9a7ee7",  "صحيح",    "9→ص + 7→ح + ee→ي"},

    // ── J. Digraph sh → SHEEN (ش) ──
    ExpansionCase{"shams",   "شمس",     "sh→ش + vowel skip"},
    ExpansionCase{"shkr",    "شكر",     "sh→ش pure consonants"},
    ExpansionCase{"sharq",   "شرق",     "sh→ش + vowel skip + q→ق"},
    ExpansionCase{"shajara", "شجرة",    "sh→ش + a→ة ta marbuta"},

    // ── K. Digraph kh → KHA (خ) ──
    ExpansionCase{"khayr",   "خير",     "kh→خ + vowel skip"},
    ExpansionCase{"khlal",   "خلال",    "kh→خ + long a→ا"},

    // ── L. Digraph gh → GHAIN (غ) ──
    ExpansionCase{"ghayr",   "غير",     "gh→غ + vowel skip + y→ي"},
    ExpansionCase{"ghurfa",  "غرفة",    "gh→غ + u→skip + a→ة"},

    // ── M. Digraph th → THA (ث) / DHAL (ذ) ──
    ExpansionCase{"thawb",   "ثوب",     "th→ث + vowel skip + w→و"},
    ExpansionCase{"thaman",  "ثمن",     "th→ث + double vowel skip"},
    ExpansionCase{"dhahab",  "ذهب",     "th/dh→ذ + h→ه"},

    // ── N. Digraph dh → DHAL (ذ) / DAD (ض) ──
    ExpansionCase{"dharb",   "ضرب",     "dh→ض + vowel skip"},

    // ── O. Apostrophe variants ──
    ExpansionCase{"3'rb",    "غرب",     "3'→غ"},
    ExpansionCase{"7'br",    "خبر",     "7'→خ"},
    ExpansionCase{"6'lm",    "ظلم",     "6'→ظ"},
    ExpansionCase{"9'rb",    "ضرب",     "9'→ض"},
    ExpansionCase{"d'kr",    "ذكر",     "d'→ذ"},
    ExpansionCase{"t'awb",   "ثوب",     "t'→ث"},
    ExpansionCase{"s'ms",    "شمس",     "s'→ش"},

    // ── P. Doubled consonants ──
    ExpansionCase{"allah",   "الله",    "l+l (not ll digraph)"},
    ExpansionCase{"umma",    "أمة",     "mm→م + a→ة"},

    // ── Q. Ta marbuta (a→ة at word end) ──
    ExpansionCase{"risala",  "رسالة",   "final a→ة + i→skip"},
    ExpansionCase{"madina",  "مدينة",   "final a→ة + i→ي"},
    ExpansionCase{"maktaba", "مكتبة",   "final a→ة + vowel skips"},
    ExpansionCase{"jameela", "جميلة",   "final a→ة + ee→ي"},

    // ── R. Multi-feature words ──
    ExpansionCase{"mo7amed", "محمد",    "o→skip + 7→ح + e→skip"},
    ExpansionCase{"3abdallah","عبدالله", "3→ع + l+l + vowel skips"},
    ExpansionCase{"funduq",  "فندق",    "u→skip twice + q→ق"},

    // ── S. Alternative Arabizi spellings for same word ──
    ExpansionCase{"saba7",   "صباح",    "s→ص alt spelling for صباح"},
    ExpansionCase{"khabar",  "خبر",     "kh→خ alt for خبر"},
    ExpansionCase{"ghayb",   "غيب",     "gh→غ + vowel skip + y→ي"},
    ExpansionCase{"3alam",   "علم",     "3→ع alt for علم"}
));

// Verify that ambiguous letters produce multiple valid words
TEST_F(RuleEngineTest, AmbiguousS_ProducesBothSeenAndSad) {
    auto results = engine.expand("salam", 256);
    // s→SEEN: سلام
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("سلام")));
    // s→SAD: صلام
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("صلام")));
}

TEST_F(RuleEngineTest, AmbiguousK_ProducesBothKafAndQaf) {
    auto results = engine.expand("kalb", 256);
    // k→KAF: كلب
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("كلب")));
    // k→QAF: قلب
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("قلب")));
}

TEST_F(RuleEngineTest, AmbiguousT_ProducesBothTaAndTaa) {
    auto results = engine.expand("talab", 256);
    // t→TA(ت): تلب
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("تلب")));
    // t→TAA(ط): طلب
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("طلب")));
}

TEST_F(RuleEngineTest, AmbiguousD_ProducesBothDalAndDad) {
    auto results = engine.expand("darb", 256);
    // d→DAL: درب
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("درب")));
    // d→DAD: ضرب
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("ضرب")));
}

TEST_F(RuleEngineTest, AmbiguousH_ProducesBothHaAndHaa) {
    auto results = engine.expand("har", 256);
    // h→HA(ه): هر
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("هر")));
    // h→HAA(ح): حر
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("حر")));
}

TEST_F(RuleEngineTest, AmbiguousZ_ProducesMultiple) {
    auto results = engine.expand("zahr", 256);
    // z→ZAYN: زهر (flower)
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("زهر")));
    // z→DHAA: ظهر (back/noon)
    EXPECT_TRUE(contains_expansion(results, utf8_to_utf32("ظهر")));
}
