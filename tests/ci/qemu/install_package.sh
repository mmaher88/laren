#!/usr/bin/env bash
# Helper script to install a Laren package inside the VM.
# Detects package type by extension and uses the appropriate package manager.
#
# Usage: sudo ./install_package.sh <package-file>

set -euo pipefail

info()  { echo -e "\033[1;34m==>\033[0m \033[1m$*\033[0m"; }
error() { echo -e "\033[1;31m==> ERROR:\033[0m $*" >&2; exit 1; }

[[ $# -lt 1 ]] && error "Usage: $0 <package-file>"

PKG="$1"
[[ -f "$PKG" ]] || error "Package file not found: $PKG"

case "$PKG" in
    *.pkg.tar.zst|*.pkg.tar.xz)
        info "Installing Arch package: $(basename "$PKG")"
        pacman -U --noconfirm "$PKG"
        ;;
    *.rpm)
        info "Installing RPM package: $(basename "$PKG")"
        if command -v dnf &>/dev/null; then
            dnf install -y "$PKG"
        elif command -v zypper &>/dev/null; then
            zypper install -y --allow-unsigned-rpm "$PKG"
        else
            rpm -i "$PKG"
        fi
        ;;
    *.deb)
        info "Installing DEB package: $(basename "$PKG")"
        dpkg -i "$PKG" || true
        apt-get -f install -y
        ;;
    *)
        error "Unknown package type: $PKG (expected .pkg.tar.zst, .rpm, or .deb)"
        ;;
esac

info "Package installed successfully"
