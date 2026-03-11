#!/bin/bash
# Test: kwinrc already has [Wayland] with different InputMethod -> replaces with Fcitx5
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib.sh"
source "$SCRIPT_DIR/postinstall_functions.sh"

setup_test_env

begin_test "kde_kwinrc_existing: replaces existing InputMethod with Fcitx5"

# Set up kwinrc with existing [Wayland] section pointing to ibus
mkdir -p "$FAKE_HOME/.config"
cp "$SCRIPT_DIR/fixtures/kwinrc_with_wayland.ini" "$FAKE_HOME/.config/kwinrc"

setup_kde "$FAKE_HOME" "$FAKE_USER"

_kwinrc="$FAKE_HOME/.config/kwinrc"

assert_file_contains "$_kwinrc" '\[Wayland\]' "kwinrc still has [Wayland] section"
assert_file_contains "$_kwinrc" 'org.fcitx.Fcitx5.desktop' "InputMethod now points to Fcitx5"
assert_file_not_contains "$_kwinrc" 'IBus' "ibus reference is removed"
assert_file_contains "$_kwinrc" 'VirtualKeyboardEnabled=true' "VirtualKeyboardEnabled is true"
assert_file_contains "$_kwinrc" '\[Compositing\]' "other sections are preserved"
assert_file_contains "$_kwinrc" '\[Windows\]' "Windows section is preserved"

# Also test: kwinrc exists but has no [Wayland] section
teardown_test_env
setup_test_env

mkdir -p "$FAKE_HOME/.config"
cp "$SCRIPT_DIR/fixtures/kwinrc_no_wayland.ini" "$FAKE_HOME/.config/kwinrc"

setup_kde "$FAKE_HOME" "$FAKE_USER"

_kwinrc="$FAKE_HOME/.config/kwinrc"

assert_file_contains "$_kwinrc" '\[Wayland\]' "kwinrc gets [Wayland] section appended"
assert_file_contains "$_kwinrc" 'org.fcitx.Fcitx5.desktop' "InputMethod points to Fcitx5"
assert_file_contains "$_kwinrc" 'VirtualKeyboardEnabled=true' "VirtualKeyboardEnabled is true"
assert_file_contains "$_kwinrc" '\[Compositing\]' "existing sections preserved"

teardown_test_env
