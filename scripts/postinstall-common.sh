#!/bin/bash
# postinstall-common.sh — Single source of truth for Laren post-install logic.
# Installed to /usr/share/laren/postinstall-common.sh.
# Sourced by all packaging scripts (AUR, RPM, DEB, manual install).
# Tested by tests/ci/postinstall/.
#
# Every function takes explicit parameters so tests can inject fake paths.
# Set _PI_NO_CHOWN=1 to skip ownership changes (for unprivileged test runs).

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

_pi_chown() {
    if [ "${_PI_NO_CHOWN:-}" = "1" ]; then
        return 0
    fi
    chown "$@"
}

_pi_install_dir() {
    # install -d -o user -g user dir  (ownership skipped if _PI_NO_CHOWN)
    if [ "${_PI_NO_CHOWN:-}" = "1" ]; then
        mkdir -p "$3"
    else
        install -d -o "$1" -g "$2" "$3"
    fi
}

# ---------------------------------------------------------------------------
# 1. Fcitx5 profile management
# ---------------------------------------------------------------------------

add_to_fcitx_profile() {
    local home="$1"
    local user="$2"
    local profile="$home/.config/fcitx5/profile"

    if [ -f "$profile" ] && ! grep -q 'Name=laren' "$profile"; then
        local last
        last=$(grep -c '^\[Groups/0/Items/' "$profile" 2>/dev/null || echo 0)
        cat >> "$profile" << EOF

[Groups/0/Items/$last]
Name=laren
Layout=
EOF
        _pi_chown "$user":"$user" "$profile" 2>/dev/null || true
    elif [ ! -f "$profile" ]; then
        _pi_install_dir "$user" "$user" "$home/.config/fcitx5"
        cat > "$profile" << 'EOF'
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
        _pi_chown "$user":"$user" "$profile" 2>/dev/null || true
    fi
}

# ---------------------------------------------------------------------------
# 2. KDE / KWin configuration
# ---------------------------------------------------------------------------

setup_kde() {
    local home="$1"
    local user="$2"
    local desktop="${3:-/usr/share/applications/org.fcitx.Fcitx5.desktop}"
    local kwinrc="$home/.config/kwinrc"

    _pi_install_dir "$user" "$user" "$home/.config"

    if [ -f "$kwinrc" ]; then
        # Remove any existing InputMethod / VirtualKeyboardEnabled from [Wayland]
        sed -i '/^\[Wayland\]/,/^\[/{/^InputMethod/d}' "$kwinrc"
        sed -i '/^\[Wayland\]/,/^\[/{/^VirtualKeyboardEnabled/d}' "$kwinrc"

        if grep -q '^\[Wayland\]' "$kwinrc"; then
            sed -i "/^\[Wayland\]/a VirtualKeyboardEnabled=true" "$kwinrc"
            sed -i "/^\[Wayland\]/a InputMethod[\$e]=$desktop" "$kwinrc"
        else
            printf '\n[Wayland]\nInputMethod[$e]=%s\nVirtualKeyboardEnabled=true\n' "$desktop" >> "$kwinrc"
        fi
    else
        cat > "$kwinrc" <<KWINEOF
[Wayland]
InputMethod[\$e]=$desktop
VirtualKeyboardEnabled=true
KWINEOF
        _pi_chown "$user":"$user" "$kwinrc" 2>/dev/null || true
    fi
}

# ---------------------------------------------------------------------------
# 3. Environment variables (systemd environment.d + pam_environment)
# ---------------------------------------------------------------------------

setup_env_vars() {
    local home="$1"
    local user="$2"
    local envfile="$home/.config/environment.d/fcitx5.conf"

    _pi_install_dir "$user" "$user" "$home/.config/environment.d"
    if [ ! -f "$envfile" ]; then
        cat > "$envfile" <<'ENVEOF'
GTK_IM_MODULE=fcitx
QT_IM_MODULE=fcitx
XMODIFIERS=@im=fcitx
SDL_IM_MODULE=fcitx
ENVEOF
        _pi_chown "$user":"$user" "$envfile" 2>/dev/null || true
    fi

    local pamenv="$home/.pam_environment"
    if [ ! -f "$pamenv" ] || ! grep -q 'GTK_IM_MODULE.*fcitx' "$pamenv" 2>/dev/null; then
        cat >> "$pamenv" <<'PAMEOF'
GTK_IM_MODULE DEFAULT=fcitx
QT_IM_MODULE DEFAULT=fcitx
XMODIFIERS DEFAULT=@im=fcitx
PAMEOF
        _pi_chown "$user":"$user" "$pamenv" 2>/dev/null || true
    fi
}

# ---------------------------------------------------------------------------
# 4. XDG autostart desktop entry
# ---------------------------------------------------------------------------

setup_autostart() {
    local home="$1"
    local user="$2"
    local autostart="$home/.config/autostart/org.fcitx.Fcitx5.desktop"

    _pi_install_dir "$user" "$user" "$home/.config/autostart"
    if [ ! -f "$autostart" ]; then
        cat > "$autostart" <<'AUTOEOF'
[Desktop Entry]
Name=Fcitx 5
Comment=Start Input Method
Exec=fcitx5 -d
Type=Application
Categories=System;Utility;
X-GNOME-Autostart-enabled=true
AUTOEOF
        _pi_chown "$user":"$user" "$autostart" 2>/dev/null || true
    fi
}

# ---------------------------------------------------------------------------
# 5. Sway compositor
# ---------------------------------------------------------------------------

setup_sway() {
    local home="$1"
    local user="$2"
    local swayconf="$home/.config/sway/config"

    if [ -f "$swayconf" ]; then
        if ! grep -q 'exec.*fcitx5' "$swayconf" 2>/dev/null; then
            echo 'exec_always fcitx5 -d --replace' >> "$swayconf"
            _pi_chown "$user":"$user" "$swayconf" 2>/dev/null || true
        fi
    fi
}

# ---------------------------------------------------------------------------
# 6. Hyprland compositor
# ---------------------------------------------------------------------------

setup_hyprland() {
    local home="$1"
    local user="$2"
    local hyprconf="$home/.config/hypr/hyprland.conf"

    if [ -f "$hyprconf" ]; then
        if ! grep -q 'exec.*fcitx5' "$hyprconf" 2>/dev/null; then
            echo 'exec-once = fcitx5 -d' >> "$hyprconf"
            _pi_chown "$user":"$user" "$hyprconf" 2>/dev/null || true
        fi
    fi
}

# ---------------------------------------------------------------------------
# 7. Orchestrator — configure a single user (called by packaging scripts)
# ---------------------------------------------------------------------------

configure_user() {
    local home="$1"
    local user="$2"
    # DE detection: pass explicitly, or empty to auto-detect from config files
    local de="${3:-}"

    # 1. Always add to Fcitx5 profile
    add_to_fcitx_profile "$home" "$user"

    # 2. Detect DE if not provided
    if [ -z "$de" ]; then
        if [ -f "$home/.config/kwinrc" ] || command -v plasmashell >/dev/null 2>&1; then
            de="kde"
        elif [ -f "$home/.config/sway/config" ]; then
            de="sway"
        elif [ -f "$home/.config/hypr/hyprland.conf" ]; then
            de="hyprland"
        else
            de="generic"
        fi
    fi

    # 3. DE-specific configuration
    case "$de" in
        *kde*|*plasma*)
            local desktop=""
            # Prefer wayland launcher (KDE 6+), fall back to standard desktop file
            [ -f /usr/share/applications/fcitx5-wayland-launcher.desktop ] && desktop="/usr/share/applications/fcitx5-wayland-launcher.desktop"
            [ -z "$desktop" ] && [ -f /usr/share/applications/org.fcitx.Fcitx5.desktop ] && desktop="/usr/share/applications/org.fcitx.Fcitx5.desktop"
            setup_kde "$home" "$user" "${desktop:-/usr/share/applications/org.fcitx.Fcitx5.desktop}"
            ;;
        *sway*)
            setup_env_vars "$home" "$user"
            setup_autostart "$home" "$user"
            setup_sway "$home" "$user"
            ;;
        *hyprland*)
            setup_env_vars "$home" "$user"
            setup_autostart "$home" "$user"
            setup_hyprland "$home" "$user"
            ;;
        *)
            setup_env_vars "$home" "$user"
            setup_autostart "$home" "$user"
            ;;
    esac
}

# ---------------------------------------------------------------------------
# 8. Iterate all users in /home (for RPM %post / DEB postinst)
# ---------------------------------------------------------------------------

configure_all_users() {
    for _home in /home/*; do
        local _user
        _user=$(basename "$_home")
        [ -d "$_home" ] || continue
        id "$_user" >/dev/null 2>&1 || continue
        configure_user "$_home" "$_user"
    done
}
