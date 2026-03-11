#!/usr/bin/env bash
# DE profile: KDE Plasma
# Sources into run_e2e.sh to set DE_PACKAGES and DE_POST_INSTALL per distro.

case "$DISTRO" in
    arch)
        DE_PACKAGES="plasma-meta sddm konsole dolphin qt6-virtualkeyboard xorg-server mesa xf86-video-fbdev"
        DE_POST_INSTALL="$(cat <<'CMD'
systemctl enable sddm
systemctl set-default graphical.target
CMD
)"
        ;;
    fedora)
        DE_PACKAGES="@kde-desktop-environment sddm"
        DE_POST_INSTALL="$(cat <<'CMD'
systemctl enable sddm
systemctl set-default graphical.target
CMD
)"
        ;;
    ubuntu)
        DE_PACKAGES="kde-standard sddm xserver-xorg"
        DE_POST_INSTALL="$(cat <<'CMD'
systemctl enable sddm
systemctl set-default graphical.target
CMD
)"
        ;;
    opensuse)
        DE_PACKAGES="patterns-kde-kde_plasma sddm xorg-x11-server Mesa"
        DE_POST_INSTALL="$(cat <<'CMD'
systemctl enable sddm
systemctl enable display-manager.service
systemctl set-default graphical.target
CMD
)"
        ;;
esac
