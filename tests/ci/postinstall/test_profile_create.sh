#!/bin/bash
# Test: No existing Fcitx5 profile -> creates one with keyboard-us + laren
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib.sh"
source "$SCRIPT_DIR/postinstall_functions.sh"

setup_test_env

begin_test "profile_create: creates new profile with keyboard-us and laren"

# No profile directory exists yet
add_to_fcitx_profile "$FAKE_HOME" "$FAKE_USER"

_profile="$FAKE_HOME/.config/fcitx5/profile"

assert_file_exists "$_profile" "profile file is created"
assert_file_contains "$_profile" 'Name=keyboard-us' "profile contains keyboard-us"
assert_file_contains "$_profile" 'Name=laren' "profile contains laren"
assert_file_contains "$_profile" 'DefaultIM=laren' "profile sets DefaultIM=laren"
assert_file_contains "$_profile" '\[Groups/0/Items/0\]' "profile has Items/0 section"
assert_file_contains "$_profile" '\[Groups/0/Items/1\]' "profile has Items/1 section"
assert_file_contains "$_profile" '\[GroupOrder\]' "profile has GroupOrder section"

teardown_test_env
