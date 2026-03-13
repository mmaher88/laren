#!/bin/bash
# Source-level regression tests.
# These grep the source code for patterns known to cause bugs.
# They serve as guardrails when the Fcitx5 UI can't be tested in CI.

source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

SRC="src/engine/laren_state.cpp"

# ── Regression: doubled preedit (v0.3.2) ──
# updateUI() must not call setAuxUp — Fcitx5 concatenates auxUp + preedit
# in the popup, causing input to appear doubled.
begin_test "no setAuxUp in updateUI (doubled preedit regression)"
if [ ! -f "$SRC" ]; then
    fail "source file not found: $SRC"
else
    UPDATE_UI_BODY=$(sed -n '/^void LarenState::updateUI/,/^void LarenState::/p' "$SRC")
    if echo "$UPDATE_UI_BODY" | grep -q 'setAuxUp'; then
        fail "updateUI() calls setAuxUp — causes doubled preedit text"
    else
        pass "updateUI() does not call setAuxUp"
    fi
fi

# ── Regression: popup not dismissing (v0.3.3) ──
# commitText() must reset the panel and update the UI, otherwise the
# candidate popup stays visible after committing a candidate.
begin_test "commitText resets panel (popup dismiss regression)"
if [ ! -f "$SRC" ]; then
    fail "source file not found: $SRC"
else
    COMMIT_BODY=$(sed -n '/^void LarenState::commitText/,/^void LarenState::/p' "$SRC")
    if echo "$COMMIT_BODY" | grep -q 'panel.reset()'; then
        pass "commitText() calls panel.reset()"
    else
        fail "commitText() does not reset panel — popup will stay visible after commit"
    fi
    if echo "$COMMIT_BODY" | grep -q 'updateUserInterface'; then
        pass "commitText() calls updateUserInterface()"
    else
        fail "commitText() does not call updateUserInterface — UI won't refresh after commit"
    fi
fi

# ── Regression: initial vowel skip (v0.3.1) ──
# expand_dfs must skip the SKIP option at position 0.
begin_test "initial vowel skip guard in rule_engine (vowel skip regression)"
RULE_SRC="src/core/rule_engine.cpp"
if [ ! -f "$RULE_SRC" ]; then
    fail "source file not found: $RULE_SRC"
else
    if grep -q 'pos == 0.*continue\|if (pos == 0) continue' "$RULE_SRC"; then
        pass "rule_engine.cpp has pos==0 guard for SKIP"
    else
        fail "rule_engine.cpp missing pos==0 guard — initial vowels will be skipped"
    fi
fi
