#!/usr/bin/env bash
# Install Laren from source.
# Detects distro, installs dependencies, builds, installs, and configures Fcitx5.
#
# Usage: ./scripts/install.sh
#   or:  curl -fsSL https://raw.githubusercontent.com/mmaher88/laren/main/scripts/install.sh | bash

set -euo pipefail

info()  { echo -e "\033[1;34m==>\033[0m \033[1m$*\033[0m"; }
warn()  { echo -e "\033[1;33m==> WARNING:\033[0m $*"; }
error() { echo -e "\033[1;31m==> ERROR:\033[0m $*"; exit 1; }

# Detect distro
if [ -f /etc/os-release ]; then
    . /etc/os-release
    DISTRO="${ID}"
else
    error "Cannot detect distribution"
fi

info "Detected: ${PRETTY_NAME:-$DISTRO}"

# Install dependencies
info "Installing build dependencies"
case "$DISTRO" in
    ubuntu|debian|pop|linuxmint)
        sudo apt-get update -qq
        sudo apt-get install -y cmake g++ git fcitx5 fcitx5-modules-dev libfcitx5core-dev fcitx5-configtool im-config
        ;;
    fedora)
        sudo dnf install -y cmake gcc-c++ git fcitx5 fcitx5-devel fcitx5-configtool
        ;;
    opensuse*|suse*)
        sudo zypper install -y cmake gcc-c++ git fcitx5 fcitx5-devel fcitx5-configtool
        ;;
    arch|endeavouros|manjaro)
        sudo pacman -S --needed --noconfirm cmake git fcitx5 fcitx5-configtool
        ;;
    *)
        warn "Unknown distro '$DISTRO'. Install these manually: cmake, g++, git, fcitx5, fcitx5-devel"
        ;;
esac

# Clone or update repo
REPO_URL="https://github.com/mmaher88/laren.git"
CLONE_DIR="${LAREN_SRC:-$HOME/laren}"

if [ -d "$CLONE_DIR/.git" ]; then
    info "Updating existing source in $CLONE_DIR"
    git -C "$CLONE_DIR" pull --ff-only
else
    info "Cloning to $CLONE_DIR"
    git clone "$REPO_URL" "$CLONE_DIR"
fi

cd "$CLONE_DIR"

# Build
info "Building"
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TESTS=OFF
cmake --build build -j"$(nproc)"

# Install
info "Installing (requires sudo)"
sudo cmake --install build

# Verify files
for f in \
    "$(pkg-config --variable=prefix fcitx5 2>/dev/null || echo /usr)/lib/fcitx5/laren.so" \
    "/usr/lib/$(uname -m)-linux-gnu/fcitx5/laren.so" \
    "/usr/lib/fcitx5/laren.so" \
    "/usr/lib64/fcitx5/laren.so"; do
    if [ -f "$f" ]; then
        FOUND_SO="$f"
        break
    fi
done

if [ -z "${FOUND_SO:-}" ]; then
    warn "laren.so not found in expected paths. Check cmake --install output above."
else
    info "Installed: $FOUND_SO"
fi

# Set Fcitx5 as input method (DEB-based distros)
if command -v im-config >/dev/null 2>&1; then
    info "Setting Fcitx5 as input method framework"
    im-config -n fcitx5
fi

# Enable Laren in Fcitx5 profile if not already present
PROFILE="$HOME/.config/fcitx5/profile"
if [ -f "$PROFILE" ]; then
    if ! grep -q 'Name=laren' "$PROFILE" 2>/dev/null; then
        info "Adding Laren to existing Fcitx5 profile"
        # Find the highest Items index in the default group and append
        LAST_IDX=$(grep -oP 'Groups/0/Items/\K\d+' "$PROFILE" | sort -n | tail -1)
        NEXT_IDX=$(( LAST_IDX + 1 ))
        echo -e "\n[Groups/0/Items/${NEXT_IDX}]\nName=laren\nLayout=" >> "$PROFILE"
    else
        info "Laren already in Fcitx5 profile"
    fi
else
    info "Creating Fcitx5 profile with Laren enabled"
    mkdir -p "$(dirname "$PROFILE")"
    cat > "$PROFILE" <<'EOF'
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
fi

# Restart Fcitx5 if running
if pgrep -x fcitx5 >/dev/null 2>&1; then
    info "Restarting Fcitx5"
    fcitx5 -r -d 2>/dev/null || true
else
    info "Starting Fcitx5"
    fcitx5 -d 2>/dev/null || true
fi

echo ""
info "Laren installed successfully!"
echo "  Switch input method: Ctrl+Space"
echo "  Configure:           fcitx5-configtool"
echo ""
echo "  If Fcitx5 doesn't start on login, log out and back in."
