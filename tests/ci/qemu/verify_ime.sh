#!/usr/bin/env bash
# Verification script that runs inside the VM after package installation.
# Checks that Laren is correctly installed and configured for the given DE.
#
# Environment variables:
#   DISTRO - arch | fedora | ubuntu
#   DE     - kde-plasma | gnome | sway | hyprland

set -uo pipefail

PASS=0
FAIL=0

pass() { echo -e "  \033[1;32mPASS\033[0m $*"; (( PASS++ )); }
fail() { echo -e "  \033[1;31mFAIL\033[0m $*"; (( FAIL++ )); }
info() { echo -e "\033[1;34m==>\033[0m \033[1m$*\033[0m"; }
skip() { echo -e "  \033[1;33mSKIP\033[0m $*"; }

DISTRO="${DISTRO:-unknown}"
DE="${DE:-unknown}"

echo ""
info "Laren IME Verification  (distro=$DISTRO  de=$DE)"
echo ""

# ---------------------------------------------------------------------------
# 1. Installed files
# ---------------------------------------------------------------------------

info "Checking installed files"

# Find laren.so in known locations
LAREN_SO=""
for path in \
    /usr/lib/fcitx5/laren.so \
    /usr/lib64/fcitx5/laren.so \
    "/usr/lib/$(uname -m)-linux-gnu/fcitx5/laren.so"; do
    if [[ -f "$path" ]]; then
        LAREN_SO="$path"
        break
    fi
done

if [[ -n "$LAREN_SO" ]]; then
    pass "laren.so found: $LAREN_SO"
else
    fail "laren.so not found in any expected path"
fi

# Addon config
ADDON_CONF=""
for path in \
    /usr/share/fcitx5/addon/laren.conf \
    /usr/share/fcitx5/addon/laren-addon.conf; do
    if [[ -f "$path" ]]; then
        ADDON_CONF="$path"
        break
    fi
done

if [[ -n "$ADDON_CONF" ]]; then
    pass "Addon config found: $ADDON_CONF"
else
    fail "Addon config not found"
fi

# Input method config
IM_CONF=""
for path in \
    /usr/share/fcitx5/inputmethod/laren.conf \
    /usr/share/fcitx5/inputmethod/laren-im.conf; do
    if [[ -f "$path" ]]; then
        IM_CONF="$path"
        break
    fi
done

if [[ -n "$IM_CONF" ]]; then
    pass "Input method config found: $IM_CONF"
else
    fail "Input method config not found"
fi

# Dictionary data
if [[ -f /usr/share/laren/dictionary.tsv ]]; then
    pass "Dictionary data found"
else
    fail "Dictionary data not found at /usr/share/laren/dictionary.tsv"
fi

# Icon
ICON_FOUND=false
for path in \
    /usr/share/icons/hicolor/scalable/apps/fcitx-laren.svg \
    /usr/share/icons/hicolor/48x48/apps/fcitx-laren.png; do
    if [[ -f "$path" ]]; then
        ICON_FOUND=true
        break
    fi
done

if $ICON_FOUND; then
    pass "Icon file found"
else
    fail "No icon file found (checked hicolor scalable + 48x48)"
fi

# ---------------------------------------------------------------------------
# 2. Shared library validation
# ---------------------------------------------------------------------------

info "Validating shared library"

if [[ -n "$LAREN_SO" ]]; then
    if file "$LAREN_SO" | grep -q "shared object"; then
        pass "laren.so is a valid shared object"
    else
        fail "laren.so is not a shared object: $(file "$LAREN_SO")"
    fi

    if command -v ldd &>/dev/null; then
        if ldd "$LAREN_SO" 2>/dev/null | grep -qi fcitx; then
            pass "laren.so links against Fcitx5 libraries"
        else
            fail "laren.so does not appear to link against Fcitx5"
        fi
    else
        skip "ldd not available, cannot check library linkage"
    fi
else
    skip "laren.so not found, skipping library validation"
fi

# ---------------------------------------------------------------------------
# 3. Fcitx5 profile
# ---------------------------------------------------------------------------

info "Checking Fcitx5 profile"

# Check system-level default profile
DEFAULT_PROFILE="/usr/share/fcitx5/default/profile"
if [[ -f "$DEFAULT_PROFILE" ]]; then
    if grep -q 'Name=laren' "$DEFAULT_PROFILE"; then
        pass "Laren present in system default profile"
    else
        fail "Laren NOT in system default profile ($DEFAULT_PROFILE)"
    fi
else
    skip "No system default profile at $DEFAULT_PROFILE"
fi

# ---------------------------------------------------------------------------
# 4. DE-specific checks
# ---------------------------------------------------------------------------

info "DE-specific checks ($DE)"

case "$DE" in
    kde-plasma)
        # Check that SDDM is the display manager
        if systemctl is-enabled sddm &>/dev/null; then
            pass "SDDM is enabled"
        else
            fail "SDDM is not enabled (expected for KDE Plasma)"
        fi

        # Check for KDE virtual keyboard config (kwinrc)
        KWINRC="/etc/xdg/kwinrc"
        if [[ -f "$KWINRC" ]] && grep -qi 'fcitx' "$KWINRC" 2>/dev/null; then
            pass "KDE kwinrc references Fcitx5"
        else
            skip "No system-level kwinrc Fcitx5 config (may be user-level)"
        fi

        # Breeze icon theme
        if [[ -f /usr/share/icons/breeze/status/22/fcitx-laren.svg ]]; then
            pass "Breeze theme icon installed"
        else
            fail "Breeze theme icon not found"
        fi
        ;;

    gnome)
        # Check for GDM or relevant display manager
        if systemctl is-enabled gdm &>/dev/null; then
            pass "GDM is enabled"
        else
            skip "GDM not enabled (may use another DM)"
        fi

        # Check for gsettings/im-config
        if command -v im-config &>/dev/null; then
            pass "im-config available for input method setup"
        else
            skip "im-config not available"
        fi
        ;;

    sway)
        # Check sway is installed
        if command -v sway &>/dev/null; then
            pass "Sway binary found"
        else
            fail "Sway binary not found"
        fi

        # Check for typical env var config locations
        if [[ -f /etc/environment ]] && grep -q 'fcitx' /etc/environment 2>/dev/null; then
            pass "Fcitx5 environment variables in /etc/environment"
        else
            skip "No system-level Fcitx5 env vars in /etc/environment (may be user-level)"
        fi
        ;;

    hyprland)
        # Check hyprland is installed
        if command -v Hyprland &>/dev/null || command -v hyprland &>/dev/null; then
            pass "Hyprland binary found"
        else
            fail "Hyprland binary not found"
        fi
        ;;

    *)
        skip "No DE-specific checks for: $DE"
        ;;
esac

# ---------------------------------------------------------------------------
# 5. Fcitx5 runtime test (GUI mode only — needs a display session)
# ---------------------------------------------------------------------------

GUI_MODE="${GUI_MODE:-false}"

if [[ "$GUI_MODE" == "true" ]]; then
    info "Fcitx5 runtime addon loading test"

    if command -v fcitx5 &>/dev/null; then
        # Start Fcitx5 in the background with a timeout
        export DISPLAY=:99
        export WAYLAND_DISPLAY=""

        # Try to start Xvfb if available (for X11 fallback)
        if command -v Xvfb &>/dev/null; then
            Xvfb :99 -screen 0 1024x768x24 &>/dev/null &
            XVFB_PID=$!
            sleep 1
        else
            XVFB_PID=""
        fi

        # Start fcitx5 and wait briefly for it to initialize
        fcitx5 -d &>/dev/null &
        FCITX_PID=$!
        sleep 3

        if kill -0 "$FCITX_PID" 2>/dev/null; then
            pass "Fcitx5 started successfully"

            # Check if laren addon was loaded via log or /tmp debug log
            if [[ -f /tmp/laren-debug.log ]]; then
                pass "Laren addon loaded (debug log exists)"
            else
                # Try checking fcitx5 log output
                if journalctl --user -u fcitx5 --no-pager -n 20 2>/dev/null | grep -qi laren; then
                    pass "Laren addon referenced in fcitx5 journal"
                else
                    skip "Cannot confirm laren addon loaded (no debug log or journal)"
                fi
            fi

            # D-Bus query for available input methods
            if command -v qdbus6 &>/dev/null; then
                info "Querying D-Bus for input methods"
                IM_LIST=$(qdbus6 org.fcitx.Fcitx5 /controller \
                    org.fcitx.Fcitx.Controller1.AvailableInputMethods 2>/dev/null || true)
                if echo "$IM_LIST" | grep -qi laren; then
                    pass "Laren listed in D-Bus AvailableInputMethods"
                else
                    fail "Laren NOT found in D-Bus AvailableInputMethods"
                fi
            elif command -v dbus-send &>/dev/null; then
                info "Querying D-Bus (dbus-send) for input methods"
                IM_LIST=$(dbus-send --session --print-reply --dest=org.fcitx.Fcitx5 \
                    /controller org.fcitx.Fcitx.Controller1.AvailableInputMethods 2>/dev/null || true)
                if echo "$IM_LIST" | grep -qi laren; then
                    pass "Laren listed in D-Bus AvailableInputMethods"
                else
                    fail "Laren NOT found in D-Bus AvailableInputMethods"
                fi
            else
                skip "No D-Bus query tool (qdbus6 or dbus-send) available"
            fi

            # Clean up
            kill "$FCITX_PID" 2>/dev/null || true
        else
            fail "Fcitx5 failed to start"
        fi

        # Clean up Xvfb
        if [[ -n "${XVFB_PID:-}" ]]; then
            kill "$XVFB_PID" 2>/dev/null || true
        fi
    else
        fail "fcitx5 binary not found"
    fi
else
    skip "Fcitx5 runtime test (skipped in headless mode, use --gui)"
fi

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------

echo ""
echo "========================================="
echo "  Results:  ${PASS} passed,  ${FAIL} failed"
echo "========================================="
echo ""

if [[ $FAIL -gt 0 ]]; then
    exit 1
else
    exit 0
fi
