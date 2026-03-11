#!/bin/bash
# Test: Existing env file with ibus settings -> should NOT be overwritten
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib.sh"
source "$SCRIPT_DIR/postinstall_functions.sh"

setup_test_env

begin_test "no_clobber: existing env file with ibus settings is not overwritten"

# Create existing environment.d/fcitx5.conf with ibus settings
mkdir -p "$FAKE_HOME/.config/environment.d"
cat > "$FAKE_HOME/.config/environment.d/fcitx5.conf" <<'EOF'
GTK_IM_MODULE=ibus
QT_IM_MODULE=ibus
XMODIFIERS=@im=ibus
EOF

# Run the env vars setup
setup_env_vars "$FAKE_HOME" "$FAKE_USER"

_envfile="$FAKE_HOME/.config/environment.d/fcitx5.conf"

# The function should NOT overwrite existing file
assert_file_contains "$_envfile" 'GTK_IM_MODULE=ibus' \
    "existing ibus GTK_IM_MODULE is preserved"
assert_file_contains "$_envfile" 'QT_IM_MODULE=ibus' \
    "existing ibus QT_IM_MODULE is preserved"
assert_file_not_contains "$_envfile" 'GTK_IM_MODULE=fcitx' \
    "fcitx GTK_IM_MODULE was not written over ibus"

# Also test: existing autostart is not clobbered
mkdir -p "$FAKE_HOME/.config/autostart"
cat > "$FAKE_HOME/.config/autostart/org.fcitx.Fcitx5.desktop" <<'EOF'
[Desktop Entry]
Name=Custom Fcitx
Exec=fcitx5 -d --custom-flag
Type=Application
EOF

setup_autostart "$FAKE_HOME" "$FAKE_USER"

_autostart="$FAKE_HOME/.config/autostart/org.fcitx.Fcitx5.desktop"
assert_file_contains "$_autostart" '--custom-flag' \
    "existing autostart with custom flags is preserved"

teardown_test_env
