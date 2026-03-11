Name:           fcitx5-laren
Version:        0.2.8
Release:        1%{?dist}
Summary:        Arabizi to Arabic transliteration engine for Fcitx5
License:        GPL-3.0-or-later
URL:            https://github.com/mmaher88/laren
Source0:        laren-%{version}.tar.xz

BuildRequires:  cmake >= 3.21
BuildRequires:  gcc-c++
BuildRequires:  fcitx5-devel

Requires:       fcitx5
Requires:       fcitx5-configtool
Requires:       hicolor-icon-theme

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
# Auto-add Laren to each user's Fcitx5 profile if not already present
for _home in /home/*; do
    _profile="$_home/.config/fcitx5/profile"
    [ -f "$_profile" ] || continue
    grep -q 'Name=laren' "$_profile" && continue
    _user=$(basename "$_home")
    # Find the last Items index and add laren after it
    _last=$(grep -c '^\[Groups/0/Items/' "$_profile" 2>/dev/null || echo 0)
    cat >> "$_profile" << EOF

[Groups/0/Items/$_last]
Name=laren
Layout=
EOF
    chown "$_user":"$_user" "$_profile" 2>/dev/null || true
done
echo ""
echo "  Laren installed successfully!"
echo ""
echo "  To activate:"
echo "    1. Set Fcitx5 as your input method (if not already):"
echo "       - Fedora/openSUSE: usually set by default"
echo "       - Ubuntu/Debian:   im-config -n fcitx5"
echo "    2. Log out and back in (or restart Fcitx5)"
echo "    3. Press Ctrl+Space to switch to Laren"
echo ""
echo "  To configure: fcitx5-configtool"
echo ""

%files
%{_libdir}/fcitx5/laren.so
%{_datadir}/fcitx5/addon/laren.conf
%{_datadir}/fcitx5/inputmethod/laren.conf
%dir %{_datadir}/laren
%{_datadir}/laren/dictionary.tsv
%{_datadir}/laren/emoji.tsv
%{_datadir}/fcitx5/default/profile
%dir %{_datadir}/icons/hicolor
%dir %{_datadir}/icons/hicolor/scalable
%dir %{_datadir}/icons/hicolor/scalable/apps
%dir %{_datadir}/icons/hicolor/16x16
%dir %{_datadir}/icons/hicolor/16x16/apps
%dir %{_datadir}/icons/hicolor/22x22
%dir %{_datadir}/icons/hicolor/22x22/apps
%dir %{_datadir}/icons/hicolor/24x24
%dir %{_datadir}/icons/hicolor/24x24/apps
%dir %{_datadir}/icons/hicolor/32x32
%dir %{_datadir}/icons/hicolor/32x32/apps
%dir %{_datadir}/icons/hicolor/48x48
%dir %{_datadir}/icons/hicolor/48x48/apps
%dir %{_datadir}/icons/hicolor/128x128
%dir %{_datadir}/icons/hicolor/128x128/apps
%{_datadir}/icons/hicolor/scalable/apps/fcitx-laren.svg
%{_datadir}/icons/hicolor/16x16/apps/fcitx-laren.png
%{_datadir}/icons/hicolor/22x22/apps/fcitx-laren.png
%{_datadir}/icons/hicolor/24x24/apps/fcitx-laren.png
%{_datadir}/icons/hicolor/32x32/apps/fcitx-laren.png
%{_datadir}/icons/hicolor/48x48/apps/fcitx-laren.png
%{_datadir}/icons/hicolor/128x128/apps/fcitx-laren.png
%dir %{_datadir}/icons/breeze
%dir %{_datadir}/icons/breeze/status
%dir %{_datadir}/icons/breeze/status/22
%{_datadir}/icons/breeze/status/22/fcitx-laren.svg
%dir %{_datadir}/icons/breeze-dark
%dir %{_datadir}/icons/breeze-dark/status
%dir %{_datadir}/icons/breeze-dark/status/22
%{_datadir}/icons/breeze-dark/status/22/fcitx-laren.svg
