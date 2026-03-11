#!/usr/bin/env bash
# Post-install script tests for Laren.
# Validates the shell logic used in RPM %post, Arch .install, and install.sh
# without actually installing anything system-wide.
#
# Runs in CI on ubuntu:24.04 (pure POSIX shell constructs).
set -euo pipefail

PASS=0
FAIL=0
TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

ok()   { PASS=$((PASS + 1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL + 1)); echo "  FAIL: $1"; }

echo "=== Post-install logic tests ==="

# ---------------------------------------------------------------------------
# Test 1: Profile creation from scratch
# ---------------------------------------------------------------------------
echo ""
echo "-- Test 1: Create Fcitx5 profile from scratch --"
PROFILE="$TMPDIR/test1/fcitx5/profile"
mkdir -p "$(dirname "$PROFILE")"
# Simulate the profile-creation logic from install.sh / RPM %post
if [ ! -f "$PROFILE" ]; then
    cat > "$PROFILE" << 'EOF'
[Groups/0]
Name=Default
Default Layout=us
DefaultIM=laren

[Groups/0/Items/0]
Name=keyboard-us
Layout=

[Groups/0/Items/1]
Name=laren
Layout=

[GroupOrder]
0=Default
EOF
fi

if grep -q 'Name=laren' "$PROFILE"; then
    ok "Profile created with laren entry"
else
    fail "Profile missing laren entry"
fi

if grep -q 'Name=keyboard-us' "$PROFILE"; then
    ok "Profile has keyboard-us fallback"
else
    fail "Profile missing keyboard-us"
fi

if grep -q 'DefaultIM=laren' "$PROFILE"; then
    ok "DefaultIM is laren"
else
    fail "DefaultIM not set to laren"
fi

# ---------------------------------------------------------------------------
# Test 2: Append laren to existing profile
# ---------------------------------------------------------------------------
echo ""
echo "-- Test 2: Append laren to existing profile --"
PROFILE2="$TMPDIR/test2/fcitx5/profile"
mkdir -p "$(dirname "$PROFILE2")"
cat > "$PROFILE2" << 'EOF'
[Groups/0]
Name=Default
Default Layout=us
DefaultIM=keyboard-us

[Groups/0/Items/0]
Name=keyboard-us
Layout=

[GroupOrder]
0=Default
EOF

# Simulate the append logic (used in install.sh and RPM %post)
if ! grep -q 'Name=laren' "$PROFILE2"; then
    LAST_IDX=$(grep -oP 'Groups/0/Items/\K\d+' "$PROFILE2" | sort -n | tail -1)
    NEXT_IDX=$(( LAST_IDX + 1 ))
    printf '\n[Groups/0/Items/%s]\nName=laren\nLayout=\n' "$NEXT_IDX" >> "$PROFILE2"
fi

if grep -q 'Name=laren' "$PROFILE2"; then
    ok "Laren appended to existing profile"
else
    fail "Laren not appended"
fi

if grep -q 'Groups/0/Items/1' "$PROFILE2"; then
    ok "Appended at correct index (1)"
else
    fail "Wrong index for appended entry"
fi

# ---------------------------------------------------------------------------
# Test 3: Don't double-add laren
# ---------------------------------------------------------------------------
echo ""
echo "-- Test 3: Idempotent -- don't double-add --"
BEFORE=$(grep -c 'Name=laren' "$PROFILE2")
# Run the same logic again
if ! grep -q 'Name=laren' "$PROFILE2"; then
    LAST_IDX=$(grep -oP 'Groups/0/Items/\K\d+' "$PROFILE2" | sort -n | tail -1)
    NEXT_IDX=$(( LAST_IDX + 1 ))
    printf '\n[Groups/0/Items/%s]\nName=laren\nLayout=\n' "$NEXT_IDX" >> "$PROFILE2"
fi
AFTER=$(grep -c 'Name=laren' "$PROFILE2")

if [ "$BEFORE" = "$AFTER" ]; then
    ok "No duplicate laren entry added"
else
    fail "Duplicate laren entry added ($BEFORE -> $AFTER)"
fi

# ---------------------------------------------------------------------------
# Test 4: Environment file creation (non-KDE path)
# ---------------------------------------------------------------------------
echo ""
echo "-- Test 4: Environment variable file creation --"
ENVFILE="$TMPDIR/test4/environment.d/fcitx5.conf"
mkdir -p "$(dirname "$ENVFILE")"

if [ ! -f "$ENVFILE" ]; then
    cat > "$ENVFILE" << 'ENVEOF'
GTK_IM_MODULE=fcitx
QT_IM_MODULE=fcitx
XMODIFIERS=@im=fcitx
SDL_IM_MODULE=fcitx
ENVEOF
fi

for VAR in GTK_IM_MODULE QT_IM_MODULE XMODIFIERS SDL_IM_MODULE; do
    if grep -q "$VAR=fcitx" "$ENVFILE" 2>/dev/null || grep -q "$VAR=@im=fcitx" "$ENVFILE" 2>/dev/null; then
        ok "$VAR present in env file"
    else
        fail "$VAR missing from env file"
    fi
done

# ---------------------------------------------------------------------------
# Test 5: Autostart desktop entry creation
# ---------------------------------------------------------------------------
echo ""
echo "-- Test 5: Autostart desktop entry --"
AUTOSTART="$TMPDIR/test5/autostart/org.fcitx.Fcitx5.desktop"
mkdir -p "$(dirname "$AUTOSTART")"

if [ ! -f "$AUTOSTART" ]; then
    cat > "$AUTOSTART" << 'AUTOEOF'
[Desktop Entry]
Name=Fcitx 5
Comment=Start Input Method
Exec=fcitx5 -d
Type=Application
Categories=System;Utility;
X-GNOME-Autostart-enabled=true
AUTOEOF
fi

if grep -q 'Exec=fcitx5' "$AUTOSTART"; then
    ok "Autostart entry has fcitx5 exec"
else
    fail "Autostart entry missing fcitx5 exec"
fi

if grep -q 'Type=Application' "$AUTOSTART"; then
    ok "Autostart entry is valid desktop file"
else
    fail "Autostart entry not a valid desktop file"
fi

# ---------------------------------------------------------------------------
# Test 6: KDE kwinrc generation
# ---------------------------------------------------------------------------
echo ""
echo "-- Test 6: KDE kwinrc InputMethod configuration --"
KWINRC="$TMPDIR/test6/kwinrc"
mkdir -p "$(dirname "$KWINRC")"
_desktop="/usr/share/applications/org.fcitx.Fcitx5.desktop"

# Simulate fresh kwinrc creation (no existing file)
if [ ! -f "$KWINRC" ]; then
    printf '[Wayland]\nInputMethod[$e]=%s\nVirtualKeyboardEnabled=true\n' "$_desktop" > "$KWINRC"
fi

if grep -q 'InputMethod' "$KWINRC"; then
    ok "kwinrc has InputMethod entry"
else
    fail "kwinrc missing InputMethod"
fi

if grep -q 'VirtualKeyboardEnabled=true' "$KWINRC"; then
    ok "kwinrc has VirtualKeyboardEnabled"
else
    fail "kwinrc missing VirtualKeyboardEnabled"
fi

if grep -q 'org.fcitx.Fcitx5.desktop' "$KWINRC"; then
    ok "kwinrc points to correct desktop file"
else
    fail "kwinrc has wrong desktop file path"
fi

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
exit 0
