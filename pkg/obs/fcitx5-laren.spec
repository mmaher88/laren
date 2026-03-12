Name:           fcitx5-laren
Version:        0.3.0
Release:        1%{?dist}
Summary:        Arabizi to Arabic transliteration engine for Fcitx5
License:        GPL-3.0-or-later
URL:            https://github.com/mmaher88/laren
Source0:        laren-%{version}.tar.xz

BuildRequires:  cmake >= 3.21
BuildRequires:  gcc-c++
BuildRequires:  gettext-tools
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
. %{_datadir}/laren/postinstall-common.sh
configure_all_users
echo ""
echo "  Laren installed successfully!"
echo "  Log out and back in, then press Ctrl+Space to switch to Laren."
echo "  To configure: fcitx5-configtool"
echo ""

%files
%{_libdir}/fcitx5/laren.so
%{_datadir}/fcitx5/addon/laren.conf
%{_datadir}/fcitx5/inputmethod/laren.conf
%dir %{_datadir}/laren
%{_datadir}/laren/dictionary.tsv
%{_datadir}/laren/emoji.tsv
%{_datadir}/laren/postinstall-common.sh
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
%{_datadir}/locale/ar/LC_MESSAGES/fcitx5-laren.mo
