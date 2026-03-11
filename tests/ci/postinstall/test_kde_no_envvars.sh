#!/bin/bash
# Test: KDE path should NOT create environment.d/fcitx5.conf
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib.sh"
source "$SCRIPT_DIR/postinstall_functions.sh"

setup_test_env

begin_test "kde_no_envvars: KDE setup does not create environment.d"

# Run only the KDE setup (as the install script would for KDE)
setup_kde "$FAKE_HOME" "$FAKE_USER"

_envfile="$FAKE_HOME/.config/environment.d/fcitx5.conf"

assert_file_not_exists "$_envfile" \
    "environment.d/fcitx5.conf should not be created by KDE path"

# Confirm kwinrc IS created (the KDE path does its own thing)
assert_file_exists "$FAKE_HOME/.config/kwinrc" \
    "kwinrc should be created by KDE setup"

teardown_test_env
