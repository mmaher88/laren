#!/usr/bin/env bash
# DE profile: Sway (Wayland tiling compositor)
# Sources into run_e2e.sh to set DE_PACKAGES and DE_POST_INSTALL per distro.

case "$DISTRO" in
    arch)
        DE_PACKAGES="sway swaybg swayidle swaylock foot wmenu xorg-xwayland"
        DE_POST_INSTALL="$(cat <<'CMD'
mkdir -p /etc/skel/.config/sway
cp /etc/sway/config /etc/skel/.config/sway/config 2>/dev/null || true
CMD
)"
        ;;
    fedora)
        DE_PACKAGES="sway foot wmenu xorg-x11-server-Xwayland"
        DE_POST_INSTALL="$(cat <<'CMD'
mkdir -p /etc/skel/.config/sway
cp /etc/sway/config /etc/skel/.config/sway/config 2>/dev/null || true
CMD
)"
        ;;
    ubuntu)
        DE_PACKAGES="sway foot wmenu xwayland"
        DE_POST_INSTALL="$(cat <<'CMD'
mkdir -p /etc/skel/.config/sway
cp /etc/sway/config /etc/skel/.config/sway/config 2>/dev/null || true
CMD
)"
        ;;
esac
