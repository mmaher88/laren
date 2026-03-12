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

void scheduleEvent(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
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

        // 8. Test Escape resets state
        sendString(testfrontend, uuid, "test");
        FCITX_ASSERT(ic->inputPanel().candidateList());
        testfrontend->call<ITestFrontend::sendKeyEvent>(
            uuid, Key("Escape"), false);
        // After escape, candidate list should be cleared
        auto afterEsc = ic->inputPanel().candidateList();
        FCITX_ASSERT(!afterEsc || afterEsc->size() == 0)
            << "Candidate list not cleared after Escape";

        // 9. Clean up
        testfrontend->call<ITestFrontend::destroyInputContext>(uuid);

        instance->deactivate();
    });

    instance->eventDispatcher().schedule([instance]() { instance->exit(); });
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

    char arg0[] = "testlaren";
    char arg1[] = "--disable=all";
    char arg2[] = "--enable=testim,testfrontend,laren";
    char *argv[] = {arg0, arg1, arg2};
    Log::setLogRule("default=5,laren=5");
    Instance instance(FCITX_ARRAY_SIZE(argv), argv);
    instance.addonManager().registerDefaultLoader(nullptr);
    scheduleEvent(&instance);
    instance.exec();
    return 0;
}
