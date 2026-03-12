#!/usr/bin/env python3
"""
Build Laren's Arabic dictionary from open-source frequency lists.

Sources:
  1. CAMeL Arabic Frequency Lists (MIX) — CC BY-SA 4.0
     https://github.com/CAMeL-Lab/Camel_Arabic_Frequency_Lists
  2. FrequencyWords (OpenSubtitles 2018) — MIT
     https://github.com/hermitdave/FrequencyWords
  3. Wikipedia Word Frequency — MIT
     https://github.com/IlyaSemenov/wikipedia-word-frequency

Strategy:
  - Include any word appearing in 2+ independent sources (cross-validated)
  - Include single-source words with freq >= 1000 (high-confidence)
  - Auto-generate taa marbuta variants (ة ↔ ه) for broader IME coverage
  - Frequency = sum across all sources (normalized to 1-10000)
  - Filter out single characters, numerals, symbols, non-Arabic script
"""

import sys
import unicodedata
from pathlib import Path


def is_arabic_word(word: str) -> bool:
    """Check if word is 2+ Arabic script characters."""
    if len(word) < 2:
        return False
    for ch in word:
        cat = unicodedata.category(ch)
        if cat.startswith('L'):
            if not ('\u0600' <= ch <= '\u06FF' or '\u0750' <= ch <= '\u077F' or
                    '\u08A0' <= ch <= '\u08FF' or '\uFB50' <= ch <= '\uFDFF' or
                    '\uFE70' <= ch <= '\uFEFF'):
                return False
        elif cat.startswith('M'):
            pass  # combining marks (diacritics) OK
        else:
            return False
    return True


def load_tsv(path: Path) -> dict[str, int]:
    d = {}
    with open(path, 'r', encoding='utf-8') as f:
        for line in f:
            parts = line.strip().split('\t')
            if len(parts) == 2:
                try:
                    d[parts[0]] = int(parts[1])
                except ValueError:
                    pass
    return d


def load_space_sep(path: Path) -> dict[str, int]:
    d = {}
    with open(path, 'r', encoding='utf-8') as f:
        for line in f:
            parts = line.strip().rsplit(' ', 1)
            if len(parts) == 2:
                try:
                    d[parts[0]] = int(parts[1])
                except ValueError:
                    pass
    return d


def normalize_freq(freqs: dict[str, int]) -> dict[str, float]:
    if not freqs:
        return {}
    max_f = max(freqs.values())
    if max_f == 0:
        return {w: 0.0 for w in freqs}
    return {w: (f / max_f) * 10000 for w, f in freqs.items()}


def main():
    base = Path(__file__).parent

    print("Loading CAMeL MIX...", file=sys.stderr)
    camel = load_tsv(base / 'camel_mix' / 'MIX_freq_lists.tsv')
    print(f"  {len(camel):,} words", file=sys.stderr)

    print("Loading FrequencyWords (OpenSubtitles)...", file=sys.stderr)
    hermit = load_space_sep(base / 'FrequencyWords' / 'content' / '2018' / 'ar' / 'ar_full.txt')
    print(f"  {len(hermit):,} words", file=sys.stderr)

    print("Loading Wikipedia frequency...", file=sys.stderr)
    wiki = load_space_sep(base / 'wikipedia-word-frequency' / 'results' / 'arwiki-2022-08-29.txt')
    print(f"  {len(wiki):,} words", file=sys.stderr)

    # Normalize
    print("Normalizing frequencies...", file=sys.stderr)
    camel_n = normalize_freq(camel)
    hermit_n = normalize_freq(hermit)
    wiki_n = normalize_freq(wiki)

    # Select words: 2+ sources OR high-freq single source
    all_words = set(camel) | set(hermit) | set(wiki)
    selected = {}
    for w in all_words:
        if not is_arabic_word(w):
            continue
        sources = (w in camel) + (w in hermit) + (w in wiki)
        score = camel_n.get(w, 0) + hermit_n.get(w, 0) + wiki_n.get(w, 0)
        if sources >= 2:
            selected[w] = score
        elif sources == 1 and max(camel.get(w, 0), hermit.get(w, 0), wiki.get(w, 0)) >= 1000:
            selected[w] = score

    print(f"Selected (2+ sources or freq>=1000): {len(selected):,}", file=sys.stderr)

    # Auto-generate taa marbuta variants
    variants_added = 0
    extra = {}
    for w, score in list(selected.items()):
        if w.endswith('\u0629'):  # ة
            alt = w[:-1] + '\u0647'  # ه
            if alt not in selected:
                extra[alt] = score * 0.8
                variants_added += 1
        elif w.endswith('\u0647'):  # ه
            alt = w[:-1] + '\u0629'  # ة
            if alt not in selected:
                extra[alt] = score * 0.8
                variants_added += 1
    selected.update(extra)
    print(f"Taa marbuta variants added: {variants_added:,}", file=sys.stderr)
    print(f"Total words: {len(selected):,}", file=sys.stderr)

    # Scale to 1-10000
    max_score = max(selected.values()) if selected else 1
    sorted_words = sorted(
        ((w, max(1, round((s / max_score) * 10000))) for w, s in selected.items()),
        key=lambda x: (-x[1], x[0])
    )

    for w, f in sorted_words:
        print(f"{w}\t{f}")

    print(f"\nDone: {len(sorted_words):,} words written", file=sys.stderr)


if __name__ == '__main__':
    main()
