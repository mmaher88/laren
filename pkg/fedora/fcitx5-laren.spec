Name:           fcitx5-laren
Version:        0.2.9
Release:        1%{?dist}
Summary:        Arabizi to Arabic transliteration engine for Fcitx5
License:        GPL-3.0-or-later
URL:            https://github.com/mmaher88/laren
Source0:        %{url}/archive/v%{version}/laren-%{version}.tar.gz

BuildRequires:  cmake >= 3.21
BuildRequires:  gcc-c++
BuildRequires:  fcitx5-devel

Requires:       fcitx5
Requires:       fcitx5-configtool

%description
Laren is an Arabizi-to-Arabic transliteration input method for Fcitx5.
Type in Latin characters (Arabizi) and get Arabic script suggestions,
similar to Microsoft Maren.

%prep
%autosetup -n laren-%{version}

%build
%cmake -DBUILD_TESTS=OFF
%cmake_build

%install
%cmake_install

%post
# Configure Laren for each user: Fcitx5 profile, DE-specific setup
for _home in /home/*; do
    _user=$(basename "$_home")
    [ -d "$_home" ] || continue
    id "$_user" >/dev/null 2>&1 || continue

    # 1. Add Laren to Fcitx5 profile (or create one)
    _profile="$_home/.config/fcitx5/profile"
    if [ -f "$_profile" ] && ! grep -q 'Name=laren' "$_profile"; then
        _last=$(grep -c '^\[Groups/0/Items/' "$_profile" 2>/dev/null || echo 0)
        cat >> "$_profile" << EOF

[Groups/0/Items/$_last]
Name=laren
Layout=
EOF
        chown "$_user":"$_user" "$_profile" 2>/dev/null || true
    elif [ ! -f "$_profile" ]; then
        mkdir -p "$_home/.config/fcitx5"
        cat > "$_profile" << 'PROFEOF'
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
PROFEOF
        chown -R "$_user":"$_user" "$_home/.config/fcitx5" 2>/dev/null || true
    fi

    # 2. Detect KDE by kwinrc or plasmashell
    _is_kde=false
    _kwinrc="$_home/.config/kwinrc"
    if [ -f "$_kwinrc" ] || command -v plasmashell >/dev/null 2>&1; then
        _is_kde=true
        if ! grep -q 'InputMethod' "$_kwinrc" 2>/dev/null; then
            _desktop=""
            [ -f /usr/share/applications/org.fcitx.Fcitx5.desktop ] && _desktop="/usr/share/applications/org.fcitx.Fcitx5.desktop"
            [ -z "$_desktop" ] && [ -f /usr/share/applications/fcitx5-wayland-launcher.desktop ] && _desktop="/usr/share/applications/fcitx5-wayland-launcher.desktop"
            if [ -n "$_desktop" ]; then
                mkdir -p "$(dirname "$_kwinrc")"
                printf '\n[Wayland]\nInputMethod[$e]=%s\nVirtualKeyboardEnabled=true\n' "$_desktop" >> "$_kwinrc"
                chown "$_user":"$_user" "$_kwinrc" 2>/dev/null || true
            fi
        fi
    fi

    # 3. Non-KDE: set env vars, autostart, compositor config
    if [ "$_is_kde" = false ]; then
        _envfile="$_home/.config/environment.d/fcitx5.conf"
        if [ ! -f "$_envfile" ]; then
            mkdir -p "$_home/.config/environment.d"
            cat > "$_envfile" << 'ENVEOF'
GTK_IM_MODULE=fcitx
QT_IM_MODULE=fcitx
XMODIFIERS=@im=fcitx
SDL_IM_MODULE=fcitx
ENVEOF
            chown -R "$_user":"$_user" "$_home/.config/environment.d" 2>/dev/null || true
        fi

        _autostart="$_home/.config/autostart/org.fcitx.Fcitx5.desktop"
        if [ ! -f "$_autostart" ]; then
            mkdir -p "$_home/.config/autostart"
            cat > "$_autostart" << 'AUTOEOF'
[Desktop Entry]
Name=Fcitx 5
Comment=Start Input Method
Exec=fcitx5 -d
Type=Application
Categories=System;Utility;
X-GNOME-Autostart-enabled=true
AUTOEOF
            chown -R "$_user":"$_user" "$_home/.config/autostart" 2>/dev/null || true
        fi

        _swayconf="$_home/.config/sway/config"
        if [ -f "$_swayconf" ] && ! grep -q 'exec.*fcitx5' "$_swayconf" 2>/dev/null; then
            echo 'exec_always fcitx5 -d --replace' >> "$_swayconf"
            chown "$_user":"$_user" "$_swayconf" 2>/dev/null || true
        fi

        _hyprconf="$_home/.config/hypr/hyprland.conf"
        if [ -f "$_hyprconf" ] && ! grep -q 'exec.*fcitx5' "$_hyprconf" 2>/dev/null; then
            echo 'exec-once = fcitx5 -d' >> "$_hyprconf"
            chown "$_user":"$_user" "$_hyprconf" 2>/dev/null || true
        fi
    fi
done
echo ""
echo "  Laren installed successfully!"
echo "  Log out and back in, then press Ctrl+Space to switch to Laren."
echo "  To configure: fcitx5-configtool"
echo ""

%files
%license LICENSE
%doc README.md
%{_libdir}/fcitx5/laren.so
%{_datadir}/fcitx5/addon/laren.conf
%{_datadir}/fcitx5/inputmethod/laren.conf
%{_datadir}/laren/dictionary.tsv
%{_datadir}/laren/emoji.tsv
%{_datadir}/fcitx5/default/profile
%{_datadir}/icons/hicolor/scalable/apps/fcitx-laren.svg
%{_datadir}/icons/hicolor/*/apps/fcitx-laren.png
%dir %{_datadir}/icons/breeze
%dir %{_datadir}/icons/breeze/status
%dir %{_datadir}/icons/breeze/status/22
%{_datadir}/icons/breeze/status/22/fcitx-laren.svg
%dir %{_datadir}/icons/breeze-dark
%dir %{_datadir}/icons/breeze-dark/status
%dir %{_datadir}/icons/breeze-dark/status/22
%{_datadir}/icons/breeze-dark/status/22/fcitx-laren.svg
