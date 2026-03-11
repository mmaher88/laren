#!/usr/bin/env bash
# Build and install Laren from source inside the VM.
# Detects distro and installs build dependencies automatically.
#
# Usage: sudo ./install_from_source.sh

set -euo pipefail

info()  { echo -e "\033[1;34m==>\033[0m \033[1m$*\033[0m"; }
error() { echo -e "\033[1;31m==> ERROR:\033[0m $*" >&2; exit 1; }

# Detect distro
if [ -f /etc/os-release ]; then
    . /etc/os-release
    DISTRO="${ID}"
else
    error "Cannot detect distribution"
fi

info "Detected: ${PRETTY_NAME:-$DISTRO}"

# Install build + runtime dependencies
info "Installing dependencies"
case "$DISTRO" in
    arch|endeavouros|manjaro|cachyos)
        pacman -Syu --noconfirm cmake make gcc git fcitx5 fcitx5-configtool
        ;;
    fedora)
        dnf install -y cmake make gcc-c++ git fcitx5-devel fcitx5-configtool
        ;;
    opensuse*)
        zypper refresh
        zypper install -y cmake make gcc-c++ git fcitx5-devel fcitx5-configtool
        ;;
    ubuntu|debian|pop|linuxmint)
        apt-get update -qq
        apt-get install -y cmake make g++ git fcitx5 fcitx5-modules-dev libfcitx5core-dev fcitx5-config-qt
        ;;
    *)
        error "Unsupported distro: $DISTRO"
        ;;
esac

# Clone and build
info "Cloning Laren"
git clone https://github.com/mmaher88/laren.git /tmp/laren-src
cd /tmp/laren-src

info "Building"
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TESTS=OFF
cmake --build build -j"$(nproc)"

info "Installing"
cmake --install build

# Run post-install
if [ -f /usr/share/laren/postinstall-common.sh ]; then
    . /usr/share/laren/postinstall-common.sh
    configure_all_users
fi

info "Laren installed from source successfully"
