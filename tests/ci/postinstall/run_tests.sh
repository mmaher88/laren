#!/usr/bin/env bash
# Post-install test runner for Laren.
# Discovers and runs all test_*.sh files, accumulates pass/fail counts,
# and prints a summary at the end.
#
# Usage:  bash tests/ci/postinstall/run_tests.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source the assertion library so counters accumulate across all test files
source "$SCRIPT_DIR/lib.sh"

# Source the functions under test once (individual test files also source them,
# but re-sourcing is harmless)
source "$SCRIPT_DIR/postinstall_functions.sh"

echo "========================================"
echo "  Laren post-install tests"
echo "========================================"
echo ""

for test_file in "$SCRIPT_DIR"/test_*.sh; do
    test_name="$(basename "$test_file" .sh)"
    echo "--- $test_name ---"

    # Source the test file in the current shell so that _PASS_COUNT and
    # _FAIL_COUNT accumulate. Each test file manages its own setup/teardown.
    if ! source "$test_file"; then
        echo "  FAIL: $test_name exited with error" >&2
        _FAIL_COUNT=$((_FAIL_COUNT + 1))
    fi

    echo ""
done

if ! print_summary; then
    exit 1
fi
