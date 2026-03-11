#!/bin/bash
# Post-install test assertion library.
# Sourced by every test_*.sh file.

set -euo pipefail

# ---------------------------------------------------------------------------
# Counters (guarded so re-sourcing does not reset them)
# ---------------------------------------------------------------------------
: "${_PASS_COUNT:=0}"
: "${_FAIL_COUNT:=0}"
_TEST_NAME=""

# ---------------------------------------------------------------------------
# Environment setup / teardown
# ---------------------------------------------------------------------------

setup_test_env() {
    TEST_TMPDIR=$(mktemp -d "/tmp/laren-postinstall-test.XXXXXX")
    FAKE_HOME="$TEST_TMPDIR/fakehome"
    FAKE_USER="testuser"
    mkdir -p "$FAKE_HOME"
    export _PI_NO_CHOWN=1   # skip chown calls in postinstall_functions.sh
}

teardown_test_env() {
    if [ -n "${TEST_TMPDIR:-}" ] && [ -d "$TEST_TMPDIR" ]; then
        rm -rf "$TEST_TMPDIR"
    fi
}

# ---------------------------------------------------------------------------
# Test naming
# ---------------------------------------------------------------------------

begin_test() {
    _TEST_NAME="$1"
}

# ---------------------------------------------------------------------------
# Assertions
# ---------------------------------------------------------------------------

pass() {
    local msg="${1:-$_TEST_NAME}"
    _PASS_COUNT=$((_PASS_COUNT + 1))
    echo "  PASS: $msg"
}

fail() {
    local msg="${1:-$_TEST_NAME}"
    _FAIL_COUNT=$((_FAIL_COUNT + 1))
    echo "  FAIL: $msg" >&2
}

assert_file_exists() {
    local path="$1"
    local msg="${2:-file exists: $path}"
    if [ -f "$path" ]; then
        pass "$msg"
    else
        fail "$msg (file not found: $path)"
    fi
}

assert_file_not_exists() {
    local path="$1"
    local msg="${2:-file does not exist: $path}"
    if [ ! -f "$path" ]; then
        pass "$msg"
    else
        fail "$msg (file unexpectedly exists: $path)"
    fi
}

assert_file_contains() {
    local path="$1"
    local pattern="$2"
    local msg="${3:-file contains '$pattern'}"
    if [ ! -f "$path" ]; then
        fail "$msg (file not found: $path)"
        return
    fi
    if grep -q -- "$pattern" "$path"; then
        pass "$msg"
    else
        fail "$msg (pattern '$pattern' not found in $path)"
    fi
}

assert_file_not_contains() {
    local path="$1"
    local pattern="$2"
    local msg="${3:-file does not contain '$pattern'}"
    if [ ! -f "$path" ]; then
        pass "$msg"
        return
    fi
    if grep -q -- "$pattern" "$path"; then
        fail "$msg (pattern '$pattern' unexpectedly found in $path)"
    else
        pass "$msg"
    fi
}

assert_file_count() {
    local path="$1"
    local pattern="$2"
    local expected="$3"
    local msg="${4:-count of '$pattern' is $expected}"
    if [ ! -f "$path" ]; then
        fail "$msg (file not found: $path)"
        return
    fi
    local actual
    actual=$(grep -c -- "$pattern" "$path" 2>/dev/null || echo 0)
    if [ "$actual" -eq "$expected" ]; then
        pass "$msg"
    else
        fail "$msg (expected $expected, got $actual in $path)"
    fi
}

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------

print_summary() {
    echo ""
    echo "========================================"
    echo "  Results: $_PASS_COUNT passed, $_FAIL_COUNT failed"
    echo "========================================"
    if [ "$_FAIL_COUNT" -gt 0 ]; then
        return 1
    fi
    return 0
}
