#!/usr/bin/env bash
# DE profile: Hyprland (Wayland tiling compositor)
# Sources into run_e2e.sh to set DE_PACKAGES and DE_POST_INSTALL per distro.

case "$DISTRO" in
    arch)
        DE_PACKAGES="hyprland kitty xorg-xwayland polkit-kde-agent"
        DE_POST_INSTALL="$(cat <<'CMD'
mkdir -p /etc/skel/.config/hypr
cp /usr/share/hyprland/hyprland.conf /etc/skel/.config/hypr/hyprland.conf 2>/dev/null || true
CMD
)"
        ;;
    fedora)
        DE_PACKAGES="hyprland kitty xorg-x11-server-Xwayland polkit-kde"
        DE_POST_INSTALL="$(cat <<'CMD'
mkdir -p /etc/skel/.config/hypr
cp /usr/share/hyprland/hyprland.conf /etc/skel/.config/hypr/hyprland.conf 2>/dev/null || true
CMD
)"
        ;;
    ubuntu)
        # Hyprland is not in Ubuntu's official repos; the user would need a PPA
        # or build from source. We list what we can and note the limitation.
        DE_PACKAGES="kitty xwayland"
        DE_POST_INSTALL="$(cat <<'CMD'
echo "NOTE: Hyprland is not in Ubuntu official repos. Install from source or PPA."
mkdir -p /etc/skel/.config/hypr
CMD
)"
        ;;
    opensuse)
        DE_PACKAGES="hyprland kitty xwayland polkit-kde-agent"
        DE_POST_INSTALL="$(cat <<'CMD'
mkdir -p /etc/skel/.config/hypr
cp /usr/share/hyprland/hyprland.conf /etc/skel/.config/hypr/hyprland.conf 2>/dev/null || true
CMD
)"
        ;;
esac
