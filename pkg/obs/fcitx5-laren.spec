Name:           fcitx5-laren
Version:        0.2.2
Release:        1%{?dist}
Summary:        Arabizi to Arabic transliteration engine for Fcitx5
License:        GPL-3.0-or-later
URL:            https://github.com/mmaher88/laren
Source0:        laren-%{version}.tar.xz

BuildRequires:  cmake >= 3.21
BuildRequires:  gcc-c++
BuildRequires:  fcitx5-devel

Requires:       fcitx5
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
# Copy tray icon into Breeze theme (KDE tray doesn't reliably fall back to hicolor)
_src=%{_datadir}/icons/hicolor/scalable/apps/fcitx-laren.svg
if [ -f "$_src" ]; then
    for _theme in %{_datadir}/icons/breeze %{_datadir}/icons/breeze-dark; do
        if [ -d "$_theme/status/22" ]; then
            cp "$_src" "$_theme/status/22/fcitx-laren.svg" 2>/dev/null
            cp "$_src" "$_theme/status/24/fcitx-laren.svg" 2>/dev/null
            gtk-update-icon-cache -f -q "$_theme" 2>/dev/null || true
        fi
    done
fi
echo ""
echo "  Laren installed successfully!"
echo ""
echo "  To activate:"
echo "    1. Set Fcitx5 as your input method (if not already):"
echo "       - Fedora/openSUSE: usually set by default"
echo "       - Ubuntu/Debian:   im-config -n fcitx5"
echo "    2. Log out and back in"
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
