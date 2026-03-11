#!/bin/bash
# Test: Non-KDE desktop creates environment.d/fcitx5.conf + autostart entry
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib.sh"
source "$SCRIPT_DIR/postinstall_functions.sh"

setup_test_env

begin_test "gnome_env: non-KDE creates env vars file and autostart entry"

# Run the generic (non-KDE) setup functions
setup_env_vars "$FAKE_HOME" "$FAKE_USER"
setup_autostart "$FAKE_HOME" "$FAKE_USER"

_envfile="$FAKE_HOME/.config/environment.d/fcitx5.conf"
_autostart="$FAKE_HOME/.config/autostart/org.fcitx.Fcitx5.desktop"

# Environment file
assert_file_exists "$_envfile" "environment.d/fcitx5.conf is created"
assert_file_contains "$_envfile" 'GTK_IM_MODULE=fcitx' "GTK_IM_MODULE is set"
assert_file_contains "$_envfile" 'QT_IM_MODULE=fcitx' "QT_IM_MODULE is set"
assert_file_contains "$_envfile" 'XMODIFIERS=@im=fcitx' "XMODIFIERS is set"
assert_file_contains "$_envfile" 'SDL_IM_MODULE=fcitx' "SDL_IM_MODULE is set"

# Autostart desktop entry
assert_file_exists "$_autostart" "autostart desktop entry is created"
assert_file_contains "$_autostart" 'Exec=fcitx5' "autostart exec is fcitx5"
assert_file_contains "$_autostart" 'X-GNOME-Autostart-enabled=true' "GNOME autostart is enabled"
assert_file_contains "$_autostart" '\[Desktop Entry\]' "has Desktop Entry header"

# pam_environment
_pamenv="$FAKE_HOME/.pam_environment"
assert_file_exists "$_pamenv" "pam_environment is created"
assert_file_contains "$_pamenv" 'GTK_IM_MODULE DEFAULT=fcitx' "pam GTK_IM_MODULE is set"

teardown_test_env
