#!/bin/bash
# Regression test: updateUI() must not set both setPreedit and setAuxUp
# with the same buffer text, as Fcitx5 concatenates them in the popup,
# causing the input to appear doubled (e.g. "gdagda" when typing "gda").

source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

begin_test "no double preedit in updateUI"

SRC_FILE="src/engine/laren_state.cpp"

if [ ! -f "$SRC_FILE" ]; then
    fail "source file not found: $SRC_FILE"
else
    # Extract the updateUI function body and check for setAuxUp
    if sed -n '/^void LarenState::updateUI/,/^void LarenState::/p' "$SRC_FILE" \
       | grep -q 'setAuxUp'; then
        fail "updateUI() calls setAuxUp — this causes doubled preedit text in the popup"
    else
        pass "updateUI() does not call setAuxUp (no double preedit)"
    fi
fi
