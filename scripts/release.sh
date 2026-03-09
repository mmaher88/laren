#!/usr/bin/env bash
# Release script for Laren.
# Bumps version everywhere, commits, tags, pushes, and updates OBS.
#
# Usage: ./scripts/release.sh <NEW_VERSION>
# Example: ./scripts/release.sh 0.2.0

set -euo pipefail

VERSION="${1:-}"
if [ -z "$VERSION" ]; then
    echo "Usage: $0 <VERSION>"
    echo "  Example: $0 0.2.0"
    exit 1
fi

# Validate version format
if ! [[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "Error: Version must be in X.Y.Z format (got: $VERSION)"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$REPO_ROOT"

# Check clean working tree
if [ -n "$(git status --porcelain)" ]; then
    echo "Error: Working tree is not clean. Commit or stash changes first."
    exit 1
fi

DATE_RPM=$(date "+%a %b %d %Y")
DATE_DEB=$(date -R)
YEAR=$(date "+%Y")

echo "==> Bumping version to $VERSION"

# 1. CMakeLists.txt
sed -i "s/project(laren VERSION [0-9.]*/project(laren VERSION $VERSION/" CMakeLists.txt
echo "    CMakeLists.txt"

# 2. PKGBUILD (Arch)
sed -i "s/^pkgver=.*/pkgver=$VERSION/" pkg/arch/PKGBUILD
sed -i "s/^pkgrel=.*/pkgrel=1/" pkg/arch/PKGBUILD
# sha256sum will need updating after the tag is pushed — mark as SKIP for now
sed -i "s/^sha256sums=.*/sha256sums=('SKIP')/" pkg/arch/PKGBUILD
echo "    pkg/arch/PKGBUILD (sha256sum=SKIP, update after release)"

# 3. Fedora spec
sed -i "s/^Version:.*/Version:        $VERSION/" pkg/fedora/fcitx5-laren.spec
sed -i "s/^Release:.*/Release:        1%{?dist}/" pkg/fedora/fcitx5-laren.spec
echo "    pkg/fedora/fcitx5-laren.spec"

# 4. OBS spec
sed -i "s/^Version:.*/Version:        $VERSION/" pkg/obs/fcitx5-laren.spec
sed -i "s/^Release:.*/Release:        1%{?dist}/" pkg/obs/fcitx5-laren.spec
echo "    pkg/obs/fcitx5-laren.spec"

# 5. OBS _service (update git tag)
sed -i "s|<param name=\"revision\">.*</param>|<param name=\"revision\">v$VERSION</param>|" pkg/obs/_service
echo "    pkg/obs/_service"

# 6. OBS .dsc
sed -i "s/^Version:.*/Version: $VERSION-1/" pkg/obs/fcitx5-laren.dsc
echo "    pkg/obs/fcitx5-laren.dsc"

# 7. OBS debian.changelog (prepend new entry)
EXISTING_CHANGELOG=$(cat pkg/obs/debian.changelog)
cat > pkg/obs/debian.changelog <<EOF
fcitx5-laren ($VERSION-1) unstable; urgency=medium

  * Release $VERSION.

 -- mmaher88 <mina.maher88@hotmail.com>  $DATE_DEB

$EXISTING_CHANGELOG
EOF
echo "    pkg/obs/debian.changelog"

echo ""
echo "==> Committing version bump"
git add -A
git commit -m "Release v$VERSION"

echo "==> Creating tag v$VERSION"
git tag -a "v$VERSION" -m "Release v$VERSION"

echo "==> Pushing to GitHub"
git push
git push --tags

echo ""
echo "==> Updating AUR PKGBUILD sha256sum"
# Download the release tarball and compute hash
TARBALL_URL="https://github.com/mmaher88/laren/archive/v$VERSION/laren-$VERSION.tar.gz"
echo "    Waiting for GitHub to generate tarball..."
sleep 5
TMPFILE=$(mktemp)
if curl -sL -o "$TMPFILE" "$TARBALL_URL" && [ -s "$TMPFILE" ]; then
    SHA256=$(sha256sum "$TMPFILE" | cut -d' ' -f1)
    rm -f "$TMPFILE"
    sed -i "s/^sha256sums=.*/sha256sums=('$SHA256')/" pkg/arch/PKGBUILD
    git add pkg/arch/PKGBUILD
    git commit -m "Update PKGBUILD sha256sum for v$VERSION"
    git push
    echo "    PKGBUILD updated with sha256: $SHA256"
else
    rm -f "$TMPFILE"
    echo "    Warning: Could not download tarball. Update sha256sum manually."
fi

echo ""
echo "==> Updating OBS"
OBS_PROJECT=$(grep -o 'project: .*' .obs/workflows.yml | head -1 | awk '{print $2}' 2>/dev/null || true)
if [ -n "$OBS_PROJECT" ] && command -v osc >/dev/null 2>&1; then
    WORK_DIR=$(mktemp -d)
    trap "rm -rf $WORK_DIR" EXIT
    cd "$WORK_DIR"
    if osc checkout "$OBS_PROJECT/fcitx5-laren" 2>/dev/null; then
        cd "$OBS_PROJECT/fcitx5-laren"
        cp "$REPO_ROOT/pkg/obs/"* .
        osc add * 2>/dev/null || true
        osc commit -m "Update to v$VERSION"
        echo "    OBS updated and build triggered"
    else
        echo "    Warning: Could not checkout OBS package. Update manually."
    fi
else
    echo "    Skipped (osc not installed or OBS project not configured)"
    echo "    Upload files from pkg/obs/ to OBS manually"
fi

echo ""
echo "=== Release v$VERSION complete ==="
echo ""
echo "Checklist:"
echo "  [x] Version bumped in all packaging files"
echo "  [x] Git commit + tag v$VERSION"
echo "  [x] Pushed to GitHub"
echo "  [ ] Verify OBS builds: https://build.opensuse.org"
echo "  [ ] Update AUR: cd aur/fcitx5-laren && makepkg --printsrcinfo > .SRCINFO && git push"
echo "  [ ] Create GitHub release: gh release create v$VERSION --generate-notes"
