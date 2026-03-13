# Pre-release Smoke Test Checklist

Run these checks **before** every version bump. All three v0.3.x regressions
(doubled preedit, popup not dismissing, initial vowel skip) would have been
caught by this 2-minute manual test.

## Build & automated tests
- [ ] `cmake --build build -j$(nproc)` — no errors
- [ ] `cd build && ctest --output-on-failure` — all tests pass

## Install & restart
- [ ] `sudo cmake --install build`
- [ ] `sudo cp build/src/laren.so /usr/lib/fcitx5/laren.so` (if AUR version installed)
- [ ] `qdbus6 org.fcitx.Fcitx5 /controller org.fcitx.Fcitx.Controller1.Restart`
- [ ] Verify: `qdbus6 org.fcitx.Fcitx5 /controller org.fcitx.Fcitx.Controller1.CurrentInputMethod` returns `laren`

## Manual typing test (open any text editor)
1. **Basic word** — type `salam`, verify:
   - [ ] Preedit shows `salam` (not doubled like `salamsalam`)
   - [ ] Candidates include سلام
   - [ ] Press Enter → سلام is committed, popup **disappears**
2. **Second word** — type `ktab`, verify:
   - [ ] Popup reappears with new candidates (كتاب etc.)
   - [ ] Press Space → committed, popup disappears
3. **Initial vowel** — type `ahlan`, verify:
   - [ ] All candidates start with an Arabic letter (ا, أ, etc.), not a consonant
4. **Arrow selection** — type `salam`, press Down arrow twice, Enter:
   - [ ] Third candidate is committed, popup disappears
5. **Escape** — type `test`, press Escape:
   - [ ] Buffer clears, popup disappears, nothing is committed
6. **Emoji** — type `:smile`, verify:
   - [ ] Emoji candidates appear
   - [ ] Press Enter → emoji committed, popup disappears
