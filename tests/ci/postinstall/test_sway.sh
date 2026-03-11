#!/bin/bash
# Test: Sway config exists -> appends exec_always fcitx5, idempotent on re-run
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib.sh"
source "$SCRIPT_DIR/postinstall_functions.sh"

setup_test_env

begin_test "sway: appends exec_always line to sway config"

# Set up sway config
mkdir -p "$FAKE_HOME/.config/sway"
cp "$SCRIPT_DIR/fixtures/sway_config" "$FAKE_HOME/.config/sway/config"

setup_sway "$FAKE_HOME" "$FAKE_USER"

_swayconf="$FAKE_HOME/.config/sway/config"

assert_file_contains "$_swayconf" 'exec_always fcitx5 -d --replace' \
    "sway config has exec_always fcitx5 line"
assert_file_contains "$_swayconf" 'set \$mod Mod4' \
    "original sway config is preserved"

# Idempotency: run again
setup_sway "$FAKE_HOME" "$FAKE_USER"

assert_file_count "$_swayconf" 'exec.*fcitx5' 1 \
    "fcitx5 line appears exactly once after second run"

teardown_test_env
