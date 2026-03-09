# Laren -- Architecture Document

**Arabizi-to-Arabic Transliteration Engine & Linux Input Method**

---

## 1. Context & Goals

Laren is a Linux replacement for Microsoft Maren: a system-wide input method that
lets users type Arabizi (Arabic written in Latin characters and numbers) and receive
Arabic script output. The user types phonetically -- "ezayak", "7abibi", "3ala
keefak" -- and Laren presents a ranked list of Arabic candidate words to choose from.

### Design priorities (ordered)

1. **Correct, predictable transliteration** -- rule-based core that a user can
   reason about. No black-box neural model that produces surprising output.
2. **System-wide availability** -- works in any application via the Linux input
   method framework.
3. **Low latency** -- candidate list must appear within a single frame (~16 ms)
   of a keystroke. An IME that lags is unusable.
4. **Incremental enhancement** -- the architecture must allow bolting on
   statistical ranking, dialect support, and user dictionaries without
   rewriting the core.

### Non-goals for v1

- Full NLP / machine translation.
- Handwriting or voice input.
- Android / iOS ports (though the core engine is portable).

---

## 2. Technology Choices

### 2.1 IME Framework: Fcitx5

**Recommendation: Fcitx5**, not IBus.

| Criterion              | Fcitx5                                    | IBus                                    |
|------------------------|-------------------------------------------|-----------------------------------------|
| Wayland support        | Native (zwp_input_method_v1/v2)           | Supported but GNOME-centric             |
| API for custom engines | Clean C++ API (InputMethodEngineV2/V3)    | C/GObject API, more boilerplate         |
| Addon architecture     | Shared-library plugins, well-documented   | D-Bus based engines, more indirection   |
| Active development     | Very active, modern C++17 codebase        | Maintained but slower pace              |
| Desktop coverage       | KDE, GNOME, Sway, Hyprland, wlroots       | GNOME default, good X11                 |
| IBus compatibility     | Fcitx5 can emulate IBus protocol          | N/A                                     |

Fcitx5 provides a clean C++ plugin API where the engine is a `.so` loaded into the
Fcitx5 process. This eliminates IPC overhead (no D-Bus round-trip per keystroke)
and gives us direct access to `InputContext`, `InputPanel`, and `CandidateList`.
The IBus-emulation layer means Fcitx5-based engines also work in GNOME sessions
that expect IBus.

### 2.2 Language: C++20

The Fcitx5 plugin API is C++. Writing the engine in C++ avoids a foreign-function
boundary (unlike fcitx5-bamboo which bridges C++ to Go via CGO). C++20 gives us:

- `std::u32string` / `char32_t` for clean Unicode handling.
- `constexpr` mapping tables that are baked into the binary at compile time.
- `std::span`, structured bindings, ranges -- cleaner code without external deps.
- Zero-overhead abstraction for the hot path (keystroke -> candidates).

### 2.3 Build System: CMake

Fcitx5 provides CMake config files (`Fcitx5Core`, `Fcitx5Utils`). CMake is the
only first-class option.

### 2.4 Data Format: Custom binary trie + plain-text source

The dictionary ships as a plain-text TSV file (easy to edit, version-control,
contribute to) and is compiled into a binary trie at build time or first launch.
The binary trie is memory-mapped at runtime for zero-copy lookup.

---

## 3. Architecture Overview

```
+------------------------------------------------------------------+
|                        Fcitx5 Framework                          |
|  +------------------+  +---------------+  +-------------------+  |
|  | WaylandIM / XIM  |  | InputContext   |  | InputPanel + UI   |  |
|  | (frontend)       |->| Manager        |->| (candidate list)  |  |
|  +------------------+  +---------------+  +-------------------+  |
+--------------|-------------------------------------------^--------+
               | keyEvent()                                | candidates
               v                                          |
+------------------------------------------------------------------+
|                     laren (Fcitx5 addon .so)                     |
|                                                                  |
|  +---------------------+    +-----------------------------+      |
|  | LarenEngine         |    | LarenState                  |      |
|  | (InputMethodEngineV2|    | (per-InputContext property)  |      |
|  |  singleton addon)   |    |                             |      |
|  |                     |    |  - inputBuffer: Latin text   |      |
|  |  - config           |--->|  - generate candidates      |      |
|  |  - dictionary       |    |  - manage preedit           |      |
|  |  - transliterator   |    |  - handle selection         |      |
|  +---------------------+    +-----------------------------+      |
|            |                          |                          |
|            v                          v                          |
|  +---------------------+    +-----------------------------+      |
|  | Dictionary          |    | Transliterator              |      |
|  |                     |    |                             |      |
|  |  - binary trie      |    |  - RuleEngine (mappings)    |      |
|  |  - frequency data   |    |  - CandidateGenerator      |      |
|  |  - user dictionary  |    |  - Ranker                   |      |
|  +---------------------+    +-----------------------------+      |
+------------------------------------------------------------------+
```

### Component responsibilities

| Component          | Responsibility                                                |
|--------------------|---------------------------------------------------------------|
| **LarenEngine**    | Fcitx5 addon singleton. Owns shared resources (dictionary,   |
|                    | config). Creates `LarenState` for each `InputContext`.        |
| **LarenState**     | Per-input-field state. Buffers Latin keystrokes, drives       |
|                    | transliteration on each keystroke, pushes candidates to       |
|                    | `InputPanel`, commits selected Arabic text.                   |
| **Transliterator** | Pure logic, no Fcitx5 dependency. Takes a Latin string,      |
|                    | returns ranked `vector<Candidate>`. Testable in isolation.   |
| **RuleEngine**     | Applies Arabizi mapping rules to expand a Latin string into  |
|                    | all plausible Arabic skeletal forms.                          |
| **Dictionary**     | Trie-backed Arabic word store with frequency data. Used to   |
|                    | filter and rank skeletal forms into real words.               |
| **Ranker**         | Scores candidates by: dictionary frequency, edit distance    |
|                    | from skeletal form, recency of user selection, bigram score. |

---

## 4. The Transliteration Engine

This is the core intellectual problem. The design uses a **generate-and-rank**
pipeline:

```
Latin input ("7abibi")
        |
        v
  [1] Normalize
        |
        v
  [2] Expand (rule-based)  -->  multiple Arabic skeletal forms
        |
        v
  [3] Dictionary lookup    -->  filter to real words
        |
        v
  [4] Rank                 -->  ordered candidate list
        |
        v
  [5] Present to user via InputPanel
```

### 4.1 Stage 1: Normalization

Lowercase the input. Collapse repeated characters (`shhh` -> `sh`). Strip
leading/trailing whitespace. This runs in O(n) on the input length.

### 4.2 Stage 2: Rule-Based Expansion

The core mapping table. This is where Arabizi conventions are encoded.

**Multi-character tokens are matched greedily, longest-first.** The input is
scanned left-to-right. At each position, the engine tries the longest possible
match first, then falls back to shorter ones.

#### 4.2.1 Primary Mapping Table

```
NUMBERS (emphatic/guttural consonants):
  2     -> [ء, ق]          hamza or qaf (context/dialect dependent)
  3     -> [ع]             ain
  3'    -> [غ]             ghain  (also: 3h, gh)
  5     -> [خ]             kha    (also: kh, 7')
  6     -> [ط]             emphatic ta
  6'    -> [ظ]             emphatic za (also: dh in some dialects)
  7     -> [ح]             ha
  7'    -> [خ]             kha    (alternate for 5)
  8     -> [ق]             qaf    (alternate, Egyptian convention)
  9     -> [ص]             emphatic sad
  9'    -> [ض]             emphatic dad

DIGRAPHS (apostrophe variants):
  d'    -> [ذ]             dhal   (also: dh, th)
  t'    -> [ث]             tha    (also: th)
  s'    -> [ش]             shin   (also: sh)

DIGRAPHS (two-letter):
  sh    -> [ش]             shin
  kh    -> [خ]             kha
  gh    -> [غ]             ghain
  th    -> [ث, ذ]          tha or dhal (ambiguous)
  dh    -> [ذ, ض]          dhal or dad (ambiguous)
  aa    -> [ا, آ]          alef / alef madda
  oo    -> [و]             long u
  ee    -> [ي]             long i
  ou    -> [و]             waw

SINGLE LETTERS:
  a     -> [ا, ـَ]         alef / fatha (short a)
  b     -> [ب]
  t     -> [ت, ط]          ta or emphatic ta
  g     -> [ج]             (Egyptian gim)
  j     -> [ج]             (Levantine/Gulf jim)
  d     -> [د, ض]          dal or dad
  r     -> [ر]
  z     -> [ز, ظ]          za or emphatic za
  s     -> [س, ص]          sin or sad
  f     -> [ف]
  q     -> [ق]
  k     -> [ك]
  l     -> [ل]
  m     -> [م]
  n     -> [ن]
  h     -> [ه, ح]          ha or haa
  w     -> [و]
  y     -> [ي]
  i     -> [ي, ـِ]         ya / kasra
  u     -> [و, ـُ]         waw / damma
  o     -> [و, ـُ]         waw / damma
  e     -> [ـَ, ـِ, ي]     context-dependent
```

**Key design decisions:**

- **Ambiguous mappings produce multiple branches.** When `s` could be sin or
  sad, the expander generates both. The dictionary stage prunes impossible
  combinations.
- **Numbers always take priority over letters.** `3` is always ain, never the
  digit 3 in Arabic context. To type the digit, the user switches to direct
  input mode.
- **Apostrophe variants are tried before the base.** `3'` is checked before
  `3` at any position.

#### 4.2.2 Expansion Algorithm

The expansion is a depth-first search over the input string, building Arabic
candidates character by character:

```
function expand(input, pos, current_arabic, results):
    if pos >= len(input):
        results.add(current_arabic)
        return

    for match_len in [4, 3, 2, 1]:          // longest-first greedy
        token = input[pos : pos + match_len]
        if token in mapping_table:
            for arabic_char in mapping_table[token]:
                expand(input, pos + match_len, current_arabic + arabic_char, results)
```

**Combinatorial explosion control:**

With ambiguous mappings, a 6-letter input could theoretically produce hundreds
of skeletal forms. Controls:

1. **Beam width**: Keep only the top-N partial candidates at each expansion
   step, scored by a simple bigram model over Arabic character pairs.
2. **Early dictionary pruning**: After expanding 2-3 characters, check if the
   prefix exists in the dictionary trie. If no word starts with that prefix,
   prune the branch.
3. **Practical observation**: Most characters have only 1 mapping. Ambiguity
   is concentrated in a few characters (s, t, d, h, th, dh). Real-world
   branching factor is low.

### 4.3 Stage 3: Dictionary Lookup

Each skeletal form from stage 2 is looked up in the dictionary trie. The
lookup is a **prefix match** -- we find all dictionary words that start with
the skeletal form, because the user may still be mid-word.

The dictionary stores:

```
struct DictionaryEntry {
    std::u32string word;        // Arabic word in Unicode
    uint32_t       frequency;   // corpus frequency (log-scaled)
    uint16_t       flags;       // part-of-speech, dialect tags
};
```

#### 4.3.1 Dictionary Sources

For v1, the dictionary is built from:

1. **Open-source Arabic word frequency lists** -- the `wordfreq` project
   provides frequency data covering Arabic, derived from Wikipedia, OpenSubtitles,
   and other corpora.
2. **Quranic Arabic Corpus** -- well-structured, freely available, covers
   classical vocabulary.
3. **Wiktionary Arabic entries** -- scrape or export for additional coverage.
4. **Manual additions** -- common Arabizi slang and dialect words that appear
   in informal chat.

Target: 200,000-500,000 entries for v1. This is sufficient for conversational
Arabic.

#### 4.3.2 Trie Design

A **double-array trie** (DARTS variant) for the dictionary:

- O(1) per character lookup (array indexing, not pointer chasing).
- Memory-mappable: the trie is a flat array of integers, loaded via `mmap()`.
- Build once at install time from the TSV source; rebuild when dictionary
  updates.
- Stores frequency data in leaf/terminal nodes.

Alternative considered: **MARISA-trie** (recursive LOUDS-based). More compact
but slower for prefix enumeration. The double-array trie is the better fit
for an IME where prefix queries dominate.

### 4.4 Stage 4: Ranking

Candidates that survive dictionary lookup are scored:

```
score(candidate) =
    w1 * log_frequency(candidate)         // corpus frequency
  + w2 * exact_match_bonus                // skeletal form == dictionary word exactly
  + w3 * user_frequency(candidate)        // how often THIS user picked this word
  + w4 * recency_bonus(candidate)         // picked recently in this session
  + w5 * bigram_score(prev_word, candidate) // co-occurrence with previous word
```

For v1, only `w1` (corpus frequency) and `w2` (exact match) are implemented.
The others are hooks for future enhancement.

The top 5-9 candidates are presented in the Fcitx5 candidate list.

### 4.5 Vowel Handling

Arabic is an abjad -- short vowels are usually unwritten. Arabizi, however,
typically includes vowels (`katab`, not `ktb`). The engine must handle this
asymmetry:

1. **Vowel-skipping**: When matching against the dictionary, vowels in the
   Arabizi input that correspond to short vowels (fatha, kasra, damma) are
   treated as optional. The skeletal form may or may not include them.
2. **Consonant-skeleton extraction**: As a fallback, extract only consonants
   from the input and match against consonant-only dictionary keys. This
   handles cases where the user's vowel spelling doesn't match any expansion.

---

## 5. Fcitx5 Integration

### 5.1 Addon Structure

Laren is a Fcitx5 **SharedLibrary** addon of category **InputMethod**.

Files installed:

```
/usr/lib/fcitx5/laren.so                        # compiled engine
/usr/share/fcitx5/addon/laren.conf               # addon metadata
/usr/share/fcitx5/inputmethod/laren.conf          # input method metadata
/usr/share/laren/dictionary.bin                   # compiled dictionary trie
/usr/share/laren/dictionary.tsv                   # source dictionary (optional)
/usr/share/laren/user/                            # user dictionary location
```

### 5.2 Addon Metadata

**laren-addon.conf.in.in:**
```ini
[Addon]
Name=Laren
Category=InputMethod
Version=@PROJECT_VERSION@
Library=laren
Type=SharedLibrary
OnDemand=True
Configurable=True
```

**laren.conf.in:**
```ini
[InputMethod]
Name=Laren (Arabizi)
Icon=fcitx-laren
Label=ع
LangCode=ar
Addon=laren
Configurable=True
```

### 5.3 Key Event Flow

```
User presses 'h'
       |
       v
Fcitx5 frontend captures key
       |
       v
LarenEngine::keyEvent(entry, keyEvent)
       |
       v
LarenState::processKey(keyEvent)
       |
       +-- Is it a letter, digit, or apostrophe?
       |       YES: append to inputBuffer
       |            run Transliterator on buffer
       |            update preedit (show Latin input)
       |            update candidateList (show Arabic candidates)
       |            keyEvent.filterAndAccept()
       |
       +-- Is it Space or Enter?
       |       YES: commit top candidate (or selected candidate)
       |            clear inputBuffer
       |            keyEvent.filterAndAccept()
       |
       +-- Is it a candidate selection key (1-9)?
       |       YES: commit that candidate
       |            clear inputBuffer
       |            keyEvent.filterAndAccept()
       |
       +-- Is it Backspace?
       |       YES: pop last char from inputBuffer
       |            re-run transliterator
       |            keyEvent.filterAndAccept()
       |
       +-- Is it Escape?
       |       YES: clear inputBuffer, reset state
       |            keyEvent.filterAndAccept()
       |
       +-- Otherwise: let the key pass through to application
```

### 5.4 Preedit Display

Two preedit regions are used:

1. **Client preedit** (in-application): Shows the raw Latin/Arabizi input the
   user has typed so far, with an underline. This lets the user see what they
   typed.
2. **Auxiliary text** (in the candidate panel): Shows the top Arabic candidate
   as a preview.

If the application supports `clientPreedit` (checked via `capabilityFlags`),
the Latin text is shown inline. Otherwise, it falls back to the panel preedit.

---

## 6. Module / File Structure

```
laren/
  CMakeLists.txt                          # root build file
  LICENSE
  ARCHITECTURE.md                         # this document
  data/
    dictionary.tsv                        # source dictionary (Arabic word + frequency)
    mappings.tsv                          # Arabizi mapping rules (overridable)
    build_trie.cpp                        # offline tool: TSV -> binary trie
  src/
    CMakeLists.txt                        # src build rules
    engine/
      laren_engine.h                      # LarenEngine class (Fcitx5 addon)
      laren_engine.cpp
      laren_state.h                       # LarenState class (per-InputContext)
      laren_state.cpp
      laren_config.h                      # Configuration schema (FCITX_CONFIGURATION)
    core/
      transliterator.h                    # Main transliteration API
      transliterator.cpp
      rule_engine.h                       # Arabizi -> Arabic expansion rules
      rule_engine.cpp
      candidate.h                         # Candidate struct
      ranker.h                            # Candidate ranking
      ranker.cpp
    dict/
      dictionary.h                        # Dictionary interface
      dictionary.cpp
      trie.h                              # Double-array trie implementation
      trie.cpp
      trie_builder.h                      # Build trie from TSV
      trie_builder.cpp
    util/
      unicode.h                           # Unicode helpers (UTF-8 <-> UTF-32)
      unicode.cpp
      mmap_file.h                         # Memory-mapped file wrapper
      mmap_file.cpp
  tests/
    CMakeLists.txt
    test_rule_engine.cpp                  # Unit tests for mapping rules
    test_transliterator.cpp               # End-to-end transliteration tests
    test_trie.cpp                         # Trie correctness tests
    test_dictionary.cpp                   # Dictionary lookup tests
    test_ranker.cpp                       # Ranking tests
    test_data/
      small_dictionary.tsv                # Small dictionary for tests
  resources/
    laren-addon.conf.in.in                # Fcitx5 addon metadata template
    laren.conf.in                         # Fcitx5 input method metadata template
    icons/
      fcitx-laren.svg                     # Tray icon
```

### Dependency graph (module level)

```
  engine/        core/          dict/         util/
  +---------+   +-----------+  +----------+  +--------+
  | laren_  |-->| translit- |->| dictio-  |->| mmap   |
  | engine  |   | erator    |  | nary     |  | file   |
  |         |   |           |  |          |  +--------+
  | laren_  |-->| rule_     |  | trie     |->| unicode|
  | state   |   | engine    |  |          |  +--------+
  |         |   |           |  | trie_    |
  | laren_  |   | ranker    |  | builder  |
  | config  |   +-----------+  +----------+
  +---------+

  engine/ depends on: Fcitx5::Core, core/, dict/
  core/   depends on: dict/, util/
  dict/   depends on: util/
  util/   depends on: nothing (leaf module)
```

**Critical boundary:** `core/` and `dict/` have ZERO dependency on Fcitx5.
They are pure C++20 libraries. This means:

- They can be unit-tested with just a C++ test framework (Catch2 or
  GoogleTest), no Fcitx5 installation required.
- They can be reused in a CLI tool, a web service, or a different IME
  framework without modification.
- The Fcitx5 coupling is isolated to `engine/`.

---

## 7. Build System

### Root CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.21)
project(laren VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Fcitx5Core REQUIRED)
include("${FCITX_INSTALL_CMAKECONFIG_DIR}/Fcitx5Utils/Fcitx5CompilerSettings.cmake")

option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_TRIE_TOOL "Build dictionary compiler" ON)

add_subdirectory(src)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BUILD_TRIE_TOOL)
    add_subdirectory(data)
endif()
```

### src/CMakeLists.txt

```cmake
# Util library (no deps)
add_library(laren_util STATIC
    util/unicode.cpp
    util/mmap_file.cpp
)
target_include_directories(laren_util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Dictionary library
add_library(laren_dict STATIC
    dict/dictionary.cpp
    dict/trie.cpp
    dict/trie_builder.cpp
)
target_link_libraries(laren_dict PUBLIC laren_util)

# Core transliteration library
add_library(laren_core STATIC
    core/transliterator.cpp
    core/rule_engine.cpp
    core/ranker.cpp
)
target_link_libraries(laren_core PUBLIC laren_dict)

# Fcitx5 engine plugin (shared library)
add_library(laren SHARED
    engine/laren_engine.cpp
    engine/laren_state.cpp
)
target_link_libraries(laren PRIVATE laren_core Fcitx5::Core)
set_target_properties(laren PROPERTIES PREFIX "")

# Install targets
install(TARGETS laren DESTINATION "${FCITX_INSTALL_LIBDIR}/fcitx5")

configure_file(
    "${CMAKE_SOURCE_DIR}/resources/laren-addon.conf.in.in"
    "${CMAKE_CURRENT_BINARY_DIR}/laren.conf"
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/laren.conf"
        DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")

install(FILES "${CMAKE_SOURCE_DIR}/resources/laren.conf.in"
        RENAME laren.conf
        DESTINATION "${FCITX_INSTALL_PKGDATADIR}/inputmethod")

install(FILES "${CMAKE_SOURCE_DIR}/data/dictionary.bin"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/laren")
```

---

## 8. Configuration

Fcitx5 provides a macro-based configuration system. Laren exposes:

```cpp
FCITX_CONFIGURATION(
    LarenConfig,
    // Which dialect's conventions to prefer for ambiguous mappings
    fcitx::Option<std::string> dialect{
        this, "Dialect", "Preferred dialect", "egyptian"};

    // Max candidates to show
    fcitx::Option<int> maxCandidates{
        this, "MaxCandidates", "Maximum candidates", 7};

    // Whether to learn from user selections
    fcitx::Option<bool> adaptiveLearning{
        this, "AdaptiveLearning", "Learn from selections", true};

    // Path to user dictionary
    fcitx::Option<std::string> userDictPath{
        this, "UserDictPath", "User dictionary path", ""};
);
```

Dialect affects ambiguous mappings:

| Input | Egyptian         | Levantine        | Gulf             |
|-------|------------------|------------------|------------------|
| `g`   | ج (hard g)       | ج (j sound)      | ي (y sound)      |
| `2`   | ء (hamza)        | ء (hamza)        | ق (qaf)          |
| `8`   | ق (qaf)          | ق (qaf)          | (not used)       |
| `j`   | ج (hard g)       | ج (j sound)      | ج (j sound)      |

The dialect setting adjusts the **default ranking** of ambiguous expansions,
not the available expansions. All forms are still generated; the dialect just
changes which candidate appears first.

---

## 9. Data Flow Example

User types: `s a l a m`

```
Step 1: Input buffer = "salam"

Step 2: Rule expansion
  s -> [س, ص]
  a -> [ا, ـَ]
  l -> [ل]
  a -> [ا, ـَ]
  m -> [م]

  Skeletal forms generated (pruned by beam search):
    سلام    (sin-lam-alef-mim)     -- common
    صلام    (sad-lam-alef-mim)     -- rare but valid prefix
    سلم     (sin-lam-mim, vowels dropped)
    صلم     (sad-lam-mim)

Step 3: Dictionary lookup
  سلام  -> found, freq=9500 ("peace")
  سلم   -> found, freq=7200 ("ladder" / "to be safe")
  صلام  -> not found, pruned
  صلم   -> not found, pruned

Step 4: Ranking
  1. سلام  (score: 9500 + exact_match_bonus)
  2. سلم   (score: 7200)

Step 5: Candidate list shown to user:
  [1] سلام
  [2] سلم
```

User presses Space -> "سلام" is committed to the application.

---

## 10. Alternatives Considered

### 10.1 Engine approach: Neural/statistical vs. rule-based

| Approach        | Pros                                | Cons                                    |
|-----------------|-------------------------------------|-----------------------------------------|
| Neural (seq2seq)| Handles novel words, context-aware  | Slow inference, large model, opaque     |
| Statistical     | Good accuracy with enough data      | Needs large parallel corpus, training   |
| Rule-based      | Fast, predictable, debuggable       | Won't handle truly novel words well     |
| **Hybrid**      | Best of both worlds                 | More complex to build                   |

**Decision: Rule-based core with statistical ranking.** The rule engine generates
candidates deterministically. The ranker uses corpus statistics to order them.
This gives predictability (users can learn the rules) with good defaults (common
words rank first). A neural model can be added as an optional ranker in v2.

### 10.2 IBus vs. Fcitx5

Already covered in section 2.1. IBus was rejected primarily because its engine
model (separate D-Bus process) adds latency and complexity. Fcitx5's in-process
shared-library model is simpler and faster.

### 10.3 Rust instead of C++

Rust would provide memory safety, but:

- Fcitx5's API is C++. A Rust engine would need `cxx` or `cbindgen` FFI,
  adding a build-system layer.
- The Fcitx5 community and all reference engines are C++.
- The hot path is simple array lookups in the trie -- there is very little
  unsafe memory manipulation to worry about.
- C++20 with sanitizers and fuzzing provides adequate safety for this codebase.

Rust remains a reasonable alternative if the team prefers it, but C++ has lower
integration friction.

---

## 11. Implementation Plan

### Phase 1: Skeleton (weeks 1-2)

- Set up CMake build with Fcitx5 dependency.
- Implement `LarenEngine` and `LarenState` as a passthrough (echo typed
  characters).
- Verify the addon loads in Fcitx5, appears in settings, and receives key
  events.
- Set up GoogleTest, write first test.

### Phase 2: Rule Engine (weeks 3-4)

- Implement the mapping table as a `constexpr` data structure.
- Implement the greedy longest-match scanner.
- Implement the DFS expander with beam pruning.
- Comprehensive unit tests for all mappings and edge cases.

### Phase 3: Dictionary (weeks 5-6)

- Build the TSV dictionary from `wordfreq` + Wiktionary data.
- Implement the double-array trie builder and reader.
- Implement `mmap`-based loading.
- Implement prefix lookup and frequency retrieval.
- Unit tests for trie correctness.

### Phase 4: Integration (weeks 7-8)

- Wire the Transliterator (RuleEngine + Dictionary + Ranker) into LarenState.
- Implement preedit display (Latin text in-app, Arabic in panel).
- Implement candidate selection (number keys, Space, Enter).
- Implement Backspace, Escape, and edge cases (empty buffer, etc.).
- Manual testing across applications (Firefox, Kate, terminal).

### Phase 5: Polish (weeks 9-10)

- Configuration UI (dialect selector, candidate count).
- User dictionary (persist user selections, boost frequently picked words).
- Icon and packaging (Arch PKGBUILD, .deb, Fedora COPR).
- Write user-facing README.

### Phase 6: Enhancements (post-v1)

- Bigram/trigram language model for context-aware ranking.
- Morphological awareness (recognize common Arabic word patterns like
  maf3ool, fa3il, taf3eel and use them to validate/generate candidates).
- Dialect-specific dictionary overlays.
- CLI tool (`laren-cli`) for batch transliteration.
- IBus compatibility wrapper (or rely on Fcitx5's IBus emulation).

---

## 12. Risks & Mitigations

| Risk                                          | Impact   | Mitigation                                    |
|-----------------------------------------------|----------|-----------------------------------------------|
| Combinatorial explosion in rule expansion     | High     | Beam pruning + early dictionary prefix check  |
| Dictionary too small, missing common words    | High     | Multiple corpus sources; user dictionary       |
| Fcitx5 API changes in future versions         | Medium   | Pin to InputMethodEngineV2; minimal API surface|
| Wayland compositors with broken IME support   | Medium   | Test on Sway, Hyprland, KDE, GNOME early      |
| Dialect variation makes "correct" subjective  | Medium   | Show all candidates; let user pick; learn      |
| User expects neural-level accuracy from v1    | Low      | Set expectations; rule-based is predictable    |
| Performance: trie lookup too slow             | Low      | Double-array trie is O(n) in word length;      |
|                                               |          | likely <1ms for any word                       |

---

## 13. Testing Strategy

### Unit tests (core/ and dict/)

- **Rule engine**: Every mapping in the table has at least one test.
  Ambiguous mappings have tests verifying all expansions are generated.
- **Trie**: Insert N words, verify exact lookup, prefix lookup, and
  non-existent word lookup. Fuzz with random Unicode strings.
- **Transliterator**: End-to-end tests with known Arabizi -> Arabic pairs.
  A test fixture of ~200 common words.
- **Ranker**: Verify frequency-based ordering. Verify exact-match bonus.

### Integration tests (engine/)

- Simulate Fcitx5 key events using a mock `InputContext`.
- Verify preedit updates after each keystroke.
- Verify candidate list contents.
- Verify commit on Space/Enter.
- Verify Backspace/Escape behavior.

### Manual test matrix

| Application     | Toolkit | Protocol   | Status |
|-----------------|---------|------------|--------|
| Firefox         | GTK     | Wayland    |        |
| Kate            | Qt      | Wayland    |        |
| Alacritty       | Custom  | Wayland    |        |
| foot            | Custom  | Wayland    |        |
| GNOME Terminal  | GTK     | Wayland    |        |
| LibreOffice     | Custom  | X11/XWayland|       |

---

## References

- [Fcitx5 Simple Input Method Tutorial](https://www.fcitx-im.org/wiki/Develop_an_simple_input_method)
- [Fcitx5 Input Method Developer Handbook](https://fcitx.github.io/developer-handbook/article-inputmethod.html)
- [Fcitx5 Architecture (DeepWiki)](https://deepwiki.com/fcitx/fcitx5)
- [Fcitx5 GitHub Repository](https://github.com/fcitx/fcitx5)
- [Fcitx5-Bamboo Architecture (DeepWiki)](https://deepwiki.com/fcitx/fcitx5-bamboo/5.1-input-methods)
- [Moroccan Arabizi-to-Arabic Rule-Based Transliteration (ScienceDirect)](https://www.sciencedirect.com/science/article/pii/S2468227624000176)
- [Arabish Python Transliteration](https://github.com/amasad/arabish)
- [Arabic Chat Alphabet (Wikipedia)](https://en.wikipedia.org/wiki/Arabic_chat_alphabet)
- [Arabizi Character Mappings (Kalimah Center)](https://kalimah-center.com/arabic-letters-in-numbers/)
- [wordfreq: Open-Source Word Frequency Data](https://github.com/rspeer/wordfreq)
- [Yamli Transliteration Engine](https://www.yamli.com/about/)
- [Microsoft Maren (Microsoft Learn Archive)](https://learn.microsoft.com/en-us/archive/blogs/robmar/microsoft-maren-type-arabic-in-roman-characters-on-the-fly)
- [Fcitx5 Wayland Support](https://fcitx-im.org/wiki/Using_Fcitx_5_on_Wayland)
