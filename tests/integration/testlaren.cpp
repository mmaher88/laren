/*
 * Fcitx5 integration test for Laren.
 *
 * Uses TestFrontend to simulate keystrokes and verify that Laren
 * loads, activates, and produces correct Arabic candidates.
 */

#include "testdir.h"
#include "testfrontend_public.h"
#include <filesystem>
#include <fcitx-utils/eventdispatcher.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/testing.h>
#include <fcitx/addonmanager.h>
#include <fcitx/candidatelist.h>
#include <fcitx/inputmethodgroup.h>
#include <fcitx/inputmethodmanager.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>

using namespace fcitx;

void sendString(AddonInstance *testfrontend, ICUUID uuid,
                const std::string &str) {
    for (char c : str) {
        testfrontend->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(static_cast<KeySym>(c)), false);
    }
}

void scheduleEvent(EventDispatcher &dispatcher, Instance *instance) {
    dispatcher.schedule([&dispatcher, instance]() {
        // 1. Verify addon loaded
        auto *laren = instance->addonManager().addon("laren", true);
        FCITX_ASSERT(laren) << "Laren addon failed to load";

        // 2. Set up input method group with laren
        auto defaultGroup = instance->inputMethodManager().currentGroup();
        defaultGroup.inputMethodList().clear();
        defaultGroup.inputMethodList().push_back(
            InputMethodGroupItem("keyboard-us"));
        defaultGroup.inputMethodList().push_back(
            InputMethodGroupItem("laren"));
        defaultGroup.setDefaultInputMethod("");
        instance->inputMethodManager().setGroup(defaultGroup);

        // 3. Create test input context
        auto *testfrontend = instance->addonManager().addon("testfrontend");
        FCITX_ASSERT(testfrontend) << "TestFrontend not available";
        auto uuid = testfrontend->call<ITestFrontend::createInputContext>(
            "testlaren");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        FCITX_ASSERT(ic) << "Could not find InputContext";

        // 4. Activate laren (Ctrl+Space)
        testfrontend->call<ITestFrontend::sendKeyEvent>(
            uuid, Key("Control+space"), false);
        FCITX_ASSERT(instance->inputMethod(ic) == "laren")
            << "Expected laren, got: " << instance->inputMethod(ic);

        // 5. Type "salam" and check candidates appear
        sendString(testfrontend, uuid, "salam");

        auto &panel = ic->inputPanel();
        auto candList = panel.candidateList();
        FCITX_ASSERT(candList) << "No candidate list after typing 'salam'";
        FCITX_ASSERT(candList->size() > 0)
            << "Empty candidate list for 'salam'";

        // Check that سلام is among the candidates
        bool found_salam = false;
        for (int i = 0; i < candList->size(); i++) {
            auto &cand = candList->candidate(i);
            if (cand.text().toString().find("\xd8\xb3\xd9\x84\xd8\xa7\xd9\x85")
                != std::string::npos) { // سلام in UTF-8
                found_salam = true;
                break;
            }
        }
        FCITX_ASSERT(found_salam) << "سلام not found in candidates for 'salam'";

        // 6. Commit with space — Laren appends a trailing space to committed text
        auto topCandidate = candList->candidate(0).text().toString();
        testfrontend->call<ITestFrontend::pushCommitExpectation>(
            topCandidate + " ");
        testfrontend->call<ITestFrontend::sendKeyEvent>(
            uuid, Key("space"), false);

        // 7. Type "7abibi" and verify حبيبي appears
        sendString(testfrontend, uuid, "7abibi");

        auto candList2 = ic->inputPanel().candidateList();
        FCITX_ASSERT(candList2) << "No candidate list for '7abibi'";
        FCITX_ASSERT(candList2->size() > 0)
            << "Empty candidate list for '7abibi'";

        bool found_habibi = false;
        for (int i = 0; i < candList2->size(); i++) {
            auto &cand = candList2->candidate(i);
            // حبيبي in UTF-8
            if (cand.text().toString().find(
                    "\xd8\xad\xd8\xa8\xd9\x8a\xd8\xa8\xd9\x8a") !=
                std::string::npos) {
                found_habibi = true;
                break;
            }
        }
        FCITX_ASSERT(found_habibi) << "حبيبي not found in candidates for '7abibi'";

        // Commit and clean up
        auto topCandidate2 = candList2->candidate(0).text().toString();
        testfrontend->call<ITestFrontend::pushCommitExpectation>(
            topCandidate2 + " ");
        testfrontend->call<ITestFrontend::sendKeyEvent>(
            uuid, Key("space"), false);

        // 8. Multi-word candidate quality: after two commits, verify
        //    the third word still gets real dictionary candidates (not just
        //    single-character expansions — this was the KDE/Wayland popup bug).
        sendString(testfrontend, uuid, "ktb");

        auto candList3 = ic->inputPanel().candidateList();
        FCITX_ASSERT(candList3) << "No candidate list for 'ktb' after two commits";
        FCITX_ASSERT(candList3->size() > 1)
            << "Only " << candList3->size()
            << " candidate(s) for 'ktb' — expected real dictionary matches";

        // Verify candidates contain multi-character Arabic words (not single-char)
        bool found_multichar = false;
        bool found_ktb_word = false;
        for (int i = 0; i < candList3->size(); i++) {
            auto text = candList3->candidate(i).text().toString();
            // Count UTF-8 code points (Arabic chars are multi-byte)
            size_t codepoints = 0;
            for (size_t j = 0; j < text.size(); ) {
                auto byte = static_cast<unsigned char>(text[j]);
                if (byte < 0x80) j += 1;
                else if (byte < 0xE0) j += 2;
                else if (byte < 0xF0) j += 3;
                else j += 4;
                codepoints++;
            }
            if (codepoints > 1) {
                found_multichar = true;
            }
            // Check for كتاب (ktab) or كتب (ktb)
            if (text.find("\xd9\x83\xd8\xaa\xd8\xa8") != std::string::npos || // كتب
                text.find("\xd9\x83\xd8\xaa\xd8\xa7\xd8\xa8") != std::string::npos) { // كتاب
                found_ktb_word = true;
            }
        }
        FCITX_ASSERT(found_multichar)
            << "No multi-character candidates for 'ktb' — possible popup state bug";
        FCITX_ASSERT(found_ktb_word)
            << "Neither كتاب nor كتب found in candidates for 'ktb'";

        // Commit third word
        auto topCandidate3 = candList3->candidate(0).text().toString();
        testfrontend->call<ITestFrontend::pushCommitExpectation>(
            topCandidate3 + " ");
        testfrontend->call<ITestFrontend::sendKeyEvent>(
            uuid, Key("space"), false);

        // ── Regression: popup must dismiss after commit (v0.3.3) ──
        // After committing a candidate, the panel should have no candidate
        // list. If commitText() doesn't reset the panel, the popup stays.
        {
            // Test 1: commit with Space (top candidate)
            sendString(testfrontend, uuid, "salam");
            auto cl = ic->inputPanel().candidateList();
            FCITX_ASSERT(cl && cl->size() > 0)
                << "No candidates for 'salam' in popup-dismiss test";

            auto top = cl->candidate(0).text().toString();
            testfrontend->call<ITestFrontend::pushCommitExpectation>(
                top + " ");
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("space"), false);

            auto afterCommit = ic->inputPanel().candidateList();
            FCITX_ASSERT(!afterCommit || afterCommit->size() == 0)
                << "Candidate list still visible after Space commit — "
                   "popup not dismissed (v0.3.3 regression)";

            // Test 2: arrow-select then commit with Enter
            sendString(testfrontend, uuid, "ktb");
            auto cl2 = ic->inputPanel().candidateList();
            FCITX_ASSERT(cl2 && cl2->size() > 1)
                << "Need >1 candidates for arrow-select test";

            // Press Down twice to select third candidate
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("Down"), false);
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("Down"), false);

            // The selected candidate is at index 2
            auto cl2b = ic->inputPanel().candidateList();
            auto selected = cl2b->candidate(2).text().toString();
            testfrontend->call<ITestFrontend::pushCommitExpectation>(
                selected + " ");
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("Return"), false);

            auto afterEnter = ic->inputPanel().candidateList();
            FCITX_ASSERT(!afterEnter || afterEnter->size() == 0)
                << "Candidate list still visible after arrow-select + Enter — "
                   "popup not dismissed (v0.3.3 regression)";
        }

        // ── Regression: preedit must not be doubled (v0.3.2) ──
        // If both setPreedit and setAuxUp contain the buffer, the popup
        // shows "salamsalam". Verify auxUp is empty or preedit == buffer.
        {
            sendString(testfrontend, uuid, "msr");

            auto &p = ic->inputPanel();
            auto preeditStr = p.preedit().toString();
            auto auxUpStr = p.auxUp().toString();

            // auxUp should be empty (we removed setAuxUp from updateUI)
            FCITX_ASSERT(auxUpStr.empty())
                << "auxUp is not empty ('" << auxUpStr
                << "') — will concatenate with preedit and double the text "
                   "(v0.3.2 regression)";

            // Preedit should be exactly the input, not doubled
            FCITX_ASSERT(preeditStr == "msr")
                << "Preedit is '" << preeditStr
                << "', expected 'msr' — text may be doubled "
                   "(v0.3.2 regression)";

            // Clean up: escape
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("Escape"), false);
        }

        // ── Regression: initial vowel never skipped (v0.3.1) ──
        // Words starting with a vowel (a, i, u, e, o) must always have an
        // Arabic letter at position 0 — the SKIP option must be suppressed.
        {
            sendString(testfrontend, uuid, "ahlan");

            auto cl = ic->inputPanel().candidateList();
            FCITX_ASSERT(cl && cl->size() > 0)
                << "No candidates for 'ahlan' in initial-vowel test";

            // Arabic 'a' maps to: ا أ ى ه ة — these are the valid first chars.
            // If SKIP were used at pos 0, we'd see candidates starting with
            // ه (from 'h') or ل (from 'l') with no alif prefix.
            std::string valid_starts[] = {
                "\xd8\xa7",     // ا alef
                "\xd8\xa3",     // أ alef hamza above
                "\xd8\xa5",     // إ alef hamza below
                "\xd9\x89",     // ى alef maqsura
                "\xd9\x87",     // ه ha
                "\xd8\xa9",     // ة ta marbuta
            };

            for (int i = 0; i < cl->size(); i++) {
                auto text = cl->candidate(i).text().toString();
                // Skip the raw arabizi fallback (last candidate)
                if (text == "ahlan") continue;

                bool starts_valid = false;
                for (const auto &prefix : valid_starts) {
                    if (text.substr(0, prefix.size()) == prefix) {
                        starts_valid = true;
                        break;
                    }
                }
                FCITX_ASSERT(starts_valid)
                    << "Candidate '" << text
                    << "' for 'ahlan' does not start with an Arabic vowel "
                       "letter — initial vowel was skipped (v0.3.1 regression)";
            }

            // Clean up: escape
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("Escape"), false);
        }

        // ── Backspace removes last char, candidates update ──
        {
            sendString(testfrontend, uuid, "sala");
            auto cl = ic->inputPanel().candidateList();
            FCITX_ASSERT(cl && cl->size() > 0)
                << "No candidates for 'sala'";
            auto preedit1 = ic->inputPanel().preedit().toString();
            FCITX_ASSERT(preedit1 == "sala")
                << "Preedit should be 'sala', got '" << preedit1 << "'";

            // Backspace: "sala" -> "sal"
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("BackSpace"), false);
            auto preedit2 = ic->inputPanel().preedit().toString();
            FCITX_ASSERT(preedit2 == "sal")
                << "After backspace, preedit should be 'sal', got '"
                << preedit2 << "'";
            auto cl2 = ic->inputPanel().candidateList();
            FCITX_ASSERT(cl2 && cl2->size() > 0)
                << "No candidates for 'sal' after backspace";

            // Backspace three more times: "sal" -> "sa" -> "s" -> ""
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("BackSpace"), false);
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("BackSpace"), false);
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("BackSpace"), false);

            // Buffer is now empty — should reset (no candidates)
            auto afterEmpty = ic->inputPanel().candidateList();
            FCITX_ASSERT(!afterEmpty || afterEmpty->size() == 0)
                << "Candidate list should be empty after backspacing to empty buffer";
            auto preedit3 = ic->inputPanel().preedit().toString();
            FCITX_ASSERT(preedit3.empty())
                << "Preedit should be empty after backspacing all chars, got '"
                << preedit3 << "'";
        }

        // ── Number key selects candidate by display index ──
        // Note: keys 2-9 are Arabizi input chars, so only "1" works as
        // a candidate selector (selects first candidate via commitByDisplayIndex)
        {
            sendString(testfrontend, uuid, "salam");
            auto cl = ic->inputPanel().candidateList();
            FCITX_ASSERT(cl && cl->size() >= 1)
                << "No candidates for number-key test";

            // Press "1" to select first candidate (index 0)
            auto first = cl->candidate(0).text().toString();
            testfrontend->call<ITestFrontend::pushCommitExpectation>(
                first + " ");
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key(static_cast<KeySym>(FcitxKey_1)), false);

            auto afterNum = ic->inputPanel().candidateList();
            FCITX_ASSERT(!afterNum || afterNum->size() == 0)
                << "Candidate list still visible after number-key commit — "
                   "popup not dismissed";
        }

        // ── Arabic question mark ──
        // "?" while buffer has content: commits current candidate then inserts ؟
        {
            sendString(testfrontend, uuid, "hal");
            auto cl = ic->inputPanel().candidateList();
            FCITX_ASSERT(cl && cl->size() > 0)
                << "No candidates for 'hal' in question-mark test";

            auto top = cl->candidate(0).text().toString();
            // Expect two commits: the candidate + space, then ؟
            testfrontend->call<ITestFrontend::pushCommitExpectation>(
                top + " ");
            testfrontend->call<ITestFrontend::pushCommitExpectation>(
                "\xd8\x9f"); // ؟
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("question"), false);

            auto afterQ = ic->inputPanel().candidateList();
            FCITX_ASSERT(!afterQ || afterQ->size() == 0)
                << "Candidate list should be cleared after question mark";
        }

        // ── Apostrophe input for 3-char tokens (3', 7') ──
        {
            // 3' = غ (ghain)
            sendString(testfrontend, uuid, "3");
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("apostrophe"), false);
            sendString(testfrontend, uuid, "rb");
            // Should now have "3'rb" in buffer

            auto cl = ic->inputPanel().candidateList();
            FCITX_ASSERT(cl && cl->size() > 0)
                << "No candidates for \"3'rb\"";

            auto preedit = ic->inputPanel().preedit().toString();
            FCITX_ASSERT(preedit == "3'rb")
                << "Preedit should be \"3'rb\", got '" << preedit << "'";

            // غرب should be among candidates
            bool found_gharb = false;
            for (int i = 0; i < cl->size(); i++) {
                if (cl->candidate(i).text().toString().find(
                        "\xd8\xba\xd8\xb1\xd8\xa8") != std::string::npos) {
                    found_gharb = true;
                    break;
                }
            }
            FCITX_ASSERT(found_gharb)
                << "غرب not found in candidates for \"3'rb\"";

            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("Escape"), false);
        }

        // ── Emoji mode: enter, search, commit, dismiss ──
        {
            // Enter emoji mode with ":"
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("colon"), false);

            // Preedit should show ":"
            auto preedit1 = ic->inputPanel().preedit().toString();
            // clientPreedit may have it depending on capability flags;
            // check panel preedit or auxUp for ":"
            // (emoji mode uses setAuxUp for ":shortcode")

            // Type "smile"
            sendString(testfrontend, uuid, "smile");

            // Should have emoji candidates
            auto cl = ic->inputPanel().candidateList();
            FCITX_ASSERT(cl && cl->size() > 0)
                << "No emoji candidates for ':smile'";

            // First candidate should contain 😀
            auto firstEmoji = cl->candidate(0).text().toString();
            FCITX_ASSERT(firstEmoji.find("\xf0\x9f\x98\x80") != std::string::npos
                       || firstEmoji.find("smile") != std::string::npos)
                << "First emoji candidate for ':smile' unexpected: '"
                << firstEmoji << "'";

            // Commit with Enter
            testfrontend->call<ITestFrontend::pushCommitExpectation>(
                "\xf0\x9f\x98\x80 "); // 😀 + space
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("Return"), false);

            // Popup should be dismissed
            auto afterEmoji = ic->inputPanel().candidateList();
            FCITX_ASSERT(!afterEmoji || afterEmoji->size() == 0)
                << "Candidate list still visible after emoji commit";
        }

        // ── Emoji mode: backspace and escape ──
        {
            // Enter emoji mode
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("colon"), false);
            sendString(testfrontend, uuid, "smi");

            auto cl = ic->inputPanel().candidateList();
            FCITX_ASSERT(cl && cl->size() > 0)
                << "No emoji candidates for ':smi'";

            // Backspace: ":smi" -> ":sm"
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("BackSpace"), false);

            // Still in emoji mode with candidates
            auto cl2 = ic->inputPanel().candidateList();
            // ":sm" may or may not have candidates, but shouldn't crash

            // Escape: exit emoji mode
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("Escape"), false);

            auto afterEsc = ic->inputPanel().candidateList();
            FCITX_ASSERT(!afterEsc || afterEsc->size() == 0)
                << "Candidate list should be cleared after Escape in emoji mode";
        }

        // ── Emoji mode: double colon commits first match ──
        {
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("colon"), false);
            sendString(testfrontend, uuid, "smile");

            auto cl = ic->inputPanel().candidateList();
            FCITX_ASSERT(cl && cl->size() > 0)
                << "No emoji candidates for double-colon test";

            // Second colon commits first match
            testfrontend->call<ITestFrontend::pushCommitExpectation>(
                "\xf0\x9f\x98\x80 "); // 😀 + space
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("colon"), false);

            auto afterDC = ic->inputPanel().candidateList();
            FCITX_ASSERT(!afterDC || afterDC->size() == 0)
                << "Candidate list still visible after double-colon emoji commit";
        }

        // ── Classic emoji: :) commits 😊 ──
        {
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("colon"), false);
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("parenright"), false);

            auto cl = ic->inputPanel().candidateList();
            FCITX_ASSERT(cl && cl->size() > 0)
                << "No candidates for ':)'";
            auto first = cl->candidate(0).text().toString();
            FCITX_ASSERT(first.find("\xf0\x9f\x98\x8a") != std::string::npos
                       || first.find(")") != std::string::npos)
                << "First candidate for ':)' unexpected: '" << first << "'";

            // Commit with Enter
            testfrontend->call<ITestFrontend::pushCommitExpectation>(
                "\xf0\x9f\x98\x8a "); // 😊 + space
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("Return"), false);

            auto after = ic->inputPanel().candidateList();
            FCITX_ASSERT(!after || after->size() == 0)
                << "Candidate list still visible after :) commit";
        }

        // ── Classic emoji: :( commits 😞 ──
        {
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("colon"), false);
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("parenleft"), false);

            auto cl = ic->inputPanel().candidateList();
            FCITX_ASSERT(cl && cl->size() > 0)
                << "No candidates for ':('";

            testfrontend->call<ITestFrontend::pushCommitExpectation>(
                "\xf0\x9f\x98\x9e "); // 😞 + space
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("Return"), false);
        }

        // ── Classic emoji: :D commits 😃 ──
        {
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("colon"), false);
            // :D — uppercase D gets lowercased to 'd' in emoji search
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("D"), false);

            auto cl = ic->inputPanel().candidateList();
            FCITX_ASSERT(cl && cl->size() > 0)
                << "No candidates for ':D'";
            // 'd' shortcode should be first (shortest exact match)
            testfrontend->call<ITestFrontend::pushCommitExpectation>(
                "\xf0\x9f\x98\x83 "); // 😃 + space
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("Return"), false);
        }

        // ── Classic emoji with nose: :-) commits 😊 ──
        {
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("colon"), false);
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("minus"), false);
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("parenright"), false);

            auto cl = ic->inputPanel().candidateList();
            FCITX_ASSERT(cl && cl->size() > 0)
                << "No candidates for ':-)'";

            testfrontend->call<ITestFrontend::pushCommitExpectation>(
                "\xf0\x9f\x98\x8a "); // 😊 + space
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("Return"), false);
        }

        // ── Classic emoji: :| commits 😐 ──
        {
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("colon"), false);
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("bar"), false);

            auto cl = ic->inputPanel().candidateList();
            FCITX_ASSERT(cl && cl->size() > 0)
                << "No candidates for ':|'";

            testfrontend->call<ITestFrontend::pushCommitExpectation>(
                "\xf0\x9f\x98\x90 "); // 😐 + space
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("Return"), false);
        }

        // ── Classic emoji: :'( commits 😢 ──
        {
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("colon"), false);
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("apostrophe"), false);
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("parenleft"), false);

            auto cl = ic->inputPanel().candidateList();
            FCITX_ASSERT(cl && cl->size() > 0)
                << "No candidates for ':'('";

            testfrontend->call<ITestFrontend::pushCommitExpectation>(
                "\xf0\x9f\x98\xa2 "); // 😢 + space
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("Return"), false);
        }

        // ── Classic emoji: :# commits 🤐 ──
        {
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("colon"), false);
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("numbersign"), false);

            auto cl = ic->inputPanel().candidateList();
            FCITX_ASSERT(cl && cl->size() > 0)
                << "No candidates for ':#'";

            testfrontend->call<ITestFrontend::pushCommitExpectation>(
                "\xf0\x9f\xa4\x90 "); // 🤐 + space
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("Return"), false);
        }

        // ── Key passthrough when buffer is empty ──
        {
            // With empty buffer, regular keys should NOT be consumed
            // (sendKeyEvent returns false = not filtered)
            bool consumed = testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("a"), false);
            // 'a' is a valid Arabizi char, so Laren DOES consume it
            FCITX_ASSERT(consumed)
                << "Laren should consume 'a' even with empty buffer";

            // Escape to clear
            testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("Escape"), false);

            // But non-Arabizi keys like F1 should pass through
            bool f1consumed = testfrontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key("F1"), false);
            FCITX_ASSERT(!f1consumed)
                << "F1 should not be consumed when buffer is empty";
        }

        // 9. Test Escape resets state
        sendString(testfrontend, uuid, "test");
        FCITX_ASSERT(ic->inputPanel().candidateList());
        testfrontend->call<ITestFrontend::sendKeyEvent>(
            uuid, Key("Escape"), false);
        // After escape, candidate list should be cleared
        auto afterEsc = ic->inputPanel().candidateList();
        FCITX_ASSERT(!afterEsc || afterEsc->size() == 0)
            << "Candidate list not cleared after Escape";

        // 10. Clean up
        testfrontend->call<ITestFrontend::destroyInputContext>(uuid);

        dispatcher.schedule([instance]() { instance->exit(); });
    });
}

int main() {
    // addonDirs: where laren.so lives (relative or absolute)
    // dataDirs: where addon/*.conf and inputmethod/*.conf live
    //           AND where laren/ data files (dictionary.tsv etc.) live
    std::string srcDir = std::string(TESTING_SOURCE_DIR) + "/../..";
    setupTestingEnvironmentPath(
        TESTING_BINARY_DIR, {"bin"},
        {TESTING_BINARY_DIR, srcDir});

    // Create bin/ symlink to the laren.so
    std::filesystem::path binDir =
        std::filesystem::path(TESTING_BINARY_DIR) / "bin";
    std::filesystem::create_directories(binDir);
    auto soLink = binDir / "laren.so";
    std::filesystem::remove(soLink);
    std::filesystem::create_symlink(
        std::filesystem::path(LAREN_BINARY_DIR) / "src" / "laren.so", soLink);

    // Create laren/ symlink so fcitx5 can find data files (dictionary.tsv etc.)
    auto dataLink =
        std::filesystem::path(TESTING_BINARY_DIR) / "laren";
    std::filesystem::remove(dataLink);
    std::filesystem::create_symlink(
        std::filesystem::path(srcDir) / "data", dataLink);

    char arg0[] = "testlaren";
    char arg1[] = "--disable=all";
    char arg2[] = "--enable=testim,testfrontend,laren";
    char *argv[] = {arg0, arg1, arg2};
    Log::setLogRule("default=5,laren=5");
    Instance instance(FCITX_ARRAY_SIZE(argv), argv);
    instance.addonManager().registerDefaultLoader(nullptr);
    EventDispatcher dispatcher;
    dispatcher.attach(&instance.eventLoop());
    scheduleEvent(dispatcher, &instance);
    instance.exec();
    return 0;
}
