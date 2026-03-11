Name:           fcitx5-laren
Version:        0.2.8
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
