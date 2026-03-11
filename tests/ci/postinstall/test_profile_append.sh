#!/bin/bash
# Test: Existing profile with keyboard-us + mozc -> appends laren, preserves existing
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib.sh"
source "$SCRIPT_DIR/postinstall_functions.sh"

setup_test_env

begin_test "profile_append: appends laren to existing profile"

# Set up existing profile
mkdir -p "$FAKE_HOME/.config/fcitx5"
cp "$SCRIPT_DIR/fixtures/existing_profile.ini" "$FAKE_HOME/.config/fcitx5/profile"

add_to_fcitx_profile "$FAKE_HOME" "$FAKE_USER"

_profile="$FAKE_HOME/.config/fcitx5/profile"

assert_file_contains "$_profile" 'Name=keyboard-us' "preserves keyboard-us"
assert_file_contains "$_profile" 'Name=mozc' "preserves mozc"
assert_file_contains "$_profile" 'Name=laren' "appends laren"
assert_file_contains "$_profile" '\[Groups/0/Items/2\]' "laren is Items/2 (third entry)"

teardown_test_env
