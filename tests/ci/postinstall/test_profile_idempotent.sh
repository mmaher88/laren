#!/bin/bash
# Test: Running add_to_fcitx_profile twice -> laren appears exactly once
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib.sh"
source "$SCRIPT_DIR/postinstall_functions.sh"

setup_test_env

begin_test "profile_idempotent: laren appears exactly once after two runs"

# First run: creates the profile
add_to_fcitx_profile "$FAKE_HOME" "$FAKE_USER"

# Second run: should be a no-op
add_to_fcitx_profile "$FAKE_HOME" "$FAKE_USER"

_profile="$FAKE_HOME/.config/fcitx5/profile"

assert_file_exists "$_profile" "profile exists"
assert_file_count "$_profile" 'Name=laren' 1 "laren appears exactly once"

# Also test idempotency on existing profile with append path
teardown_test_env
setup_test_env

mkdir -p "$FAKE_HOME/.config/fcitx5"
cp "$SCRIPT_DIR/fixtures/existing_profile.ini" "$FAKE_HOME/.config/fcitx5/profile"

add_to_fcitx_profile "$FAKE_HOME" "$FAKE_USER"
add_to_fcitx_profile "$FAKE_HOME" "$FAKE_USER"

_profile="$FAKE_HOME/.config/fcitx5/profile"
assert_file_count "$_profile" 'Name=laren' 1 "laren appears exactly once (append path)"

teardown_test_env
