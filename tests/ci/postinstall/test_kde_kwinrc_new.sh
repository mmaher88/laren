#!/bin/bash
# Test: No existing kwinrc -> creates one with [Wayland] section
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib.sh"
source "$SCRIPT_DIR/postinstall_functions.sh"

setup_test_env

begin_test "kde_kwinrc_new: creates kwinrc with Wayland section"

# No kwinrc exists yet
setup_kde "$FAKE_HOME" "$FAKE_USER"

_kwinrc="$FAKE_HOME/.config/kwinrc"

assert_file_exists "$_kwinrc" "kwinrc is created"
assert_file_contains "$_kwinrc" '\[Wayland\]' "kwinrc has [Wayland] section"
assert_file_contains "$_kwinrc" 'InputMethod' "kwinrc has InputMethod key"
assert_file_contains "$_kwinrc" 'org.fcitx.Fcitx5.desktop' "InputMethod points to Fcitx5"
assert_file_contains "$_kwinrc" 'VirtualKeyboardEnabled=true' "VirtualKeyboardEnabled is true"

teardown_test_env
