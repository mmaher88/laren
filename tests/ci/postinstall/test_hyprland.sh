#!/bin/bash
# Test: Hyprland config exists -> appends exec-once fcitx5, idempotent on re-run
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib.sh"
source "$SCRIPT_DIR/postinstall_functions.sh"

setup_test_env

begin_test "hyprland: appends exec-once line to hyprland config"

# Set up hyprland config
mkdir -p "$FAKE_HOME/.config/hypr"
cp "$SCRIPT_DIR/fixtures/hyprland.conf" "$FAKE_HOME/.config/hypr/hyprland.conf"

setup_hyprland "$FAKE_HOME" "$FAKE_USER"

_hyprconf="$FAKE_HOME/.config/hypr/hyprland.conf"

assert_file_contains "$_hyprconf" 'exec-once = fcitx5 -d' \
    "hyprland config has exec-once fcitx5 line"
assert_file_contains "$_hyprconf" 'monitor=,preferred,auto,1' \
    "original hyprland config is preserved"

# Idempotency: run again
setup_hyprland "$FAKE_HOME" "$FAKE_USER"

assert_file_count "$_hyprconf" 'exec.*fcitx5' 1 \
    "fcitx5 line appears exactly once after second run"

teardown_test_env
