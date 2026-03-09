#!/usr/bin/env bash
# One-time OBS project setup for Laren.
# Run after creating an account at https://build.opensuse.org
#
# Usage: ./scripts/obs-setup.sh [OBS_USERNAME]
# Example: ./scripts/obs-setup.sh mmaher88

set -euo pipefail

OBS_USER="${1:-}"
if [ -z "$OBS_USER" ]; then
    echo "Usage: $0 <OBS_USERNAME>"
    echo "  Get your username from https://build.opensuse.org after signing up"
    exit 1
fi

PROJECT="home:${OBS_USER}:laren"
PACKAGE="fcitx5-laren"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"
OBS_DIR="$REPO_ROOT/pkg/obs"

command -v osc >/dev/null 2>&1 || {
    echo "Error: 'osc' not found. Install it first:"
    echo "  Arch:   sudo pacman -S osc"
    echo "  Fedora: sudo dnf install osc"
    echo "  Ubuntu: sudo apt install osc"
    exit 1
}

echo "==> Creating OBS project: $PROJECT"
osc meta prj "$PROJECT" -F - <<EOF
<project name="$PROJECT">
  <title>Laren - Arabizi to Arabic transliteration for Fcitx5</title>
  <description>Fcitx5 input method that transliterates Arabizi (Latin) to Arabic script.</description>
  <person userid="$OBS_USER" role="maintainer"/>
  <repository name="Fedora_41">
    <path project="Fedora:41" repository="standard"/>
    <arch>x86_64</arch>
  </repository>
  <repository name="Fedora_42">
    <path project="Fedora:42" repository="standard"/>
    <arch>x86_64</arch>
  </repository>
  <repository name="xUbuntu_24.04">
    <path project="Ubuntu:24.04" repository="universe"/>
    <arch>x86_64</arch>
  </repository>
  <repository name="xUbuntu_25.10">
    <path project="Ubuntu:25.10" repository="universe"/>
    <arch>x86_64</arch>
  </repository>
  <repository name="Debian_12">
    <path project="Debian:12" repository="standard"/>
    <arch>x86_64</arch>
  </repository>
  <repository name="openSUSE_Tumbleweed">
    <path project="openSUSE:Factory" repository="snapshot"/>
    <arch>x86_64</arch>
  </repository>
</project>
EOF
echo "    Project created with 6 build targets"

echo "==> Creating package: $PACKAGE"
osc meta pkg "$PROJECT" "$PACKAGE" -F - <<EOF
<package name="$PACKAGE" project="$PROJECT">
  <title>Arabizi to Arabic transliteration engine for Fcitx5</title>
  <description>Laren is an Arabizi-to-Arabic transliteration input method for Fcitx5.
Type in Latin characters (Arabizi) and get Arabic script suggestions,
similar to Microsoft Maren.</description>
  <scmsync>https://github.com/mmaher88/laren</scmsync>
</package>
EOF
echo "    Package created"

echo "==> Checking out package"
WORK_DIR=$(mktemp -d)
trap "rm -rf $WORK_DIR" EXIT
cd "$WORK_DIR"
osc checkout "$PROJECT/$PACKAGE"
cd "$PROJECT/$PACKAGE"

echo "==> Uploading packaging files"
for f in _service fcitx5-laren.spec fcitx5-laren.dsc debian.control debian.rules debian.changelog debian.postinst; do
    cp "$OBS_DIR/$f" .
    osc add "$f" 2>/dev/null || true
done

echo "==> Committing to OBS"
osc commit -m "Initial packaging for fcitx5-laren"

echo "==> Triggering source service"
osc service remoterun "$PROJECT" "$PACKAGE" || echo "    (service trigger may require manual run on first setup)"

echo ""
echo "Done! Monitor builds at:"
echo "  https://build.opensuse.org/project/show/$PROJECT"
echo ""
echo "Once builds succeed, users install with:"
echo "  Fedora:  sudo dnf config-manager --add-repo https://download.opensuse.org/repositories/$PROJECT/Fedora_41/$PROJECT.repo"
echo "  Ubuntu:  See https://build.opensuse.org/project/show/$PROJECT for apt instructions"
echo "  openSUSE: sudo zypper ar https://download.opensuse.org/repositories/$PROJECT/openSUSE_Tumbleweed/$PROJECT.repo"
