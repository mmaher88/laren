#!/usr/bin/env bash
# DE profile: GNOME
# Sources into run_e2e.sh to set DE_PACKAGES and DE_POST_INSTALL per distro.

case "$DISTRO" in
    arch)
        DE_PACKAGES="gnome gnome-extra gdm xorg-server"
        DE_POST_INSTALL="$(cat <<'CMD'
systemctl enable gdm
CMD
)"
        ;;
    fedora)
        DE_PACKAGES="@gnome-desktop gdm gnome-session-xsession xorg-x11-server-Xorg"
        DE_POST_INSTALL="$(cat <<'CMD'
systemctl enable gdm
systemctl set-default graphical.target
CMD
)"
        ;;
    ubuntu)
        DE_PACKAGES="ubuntu-desktop gdm3"
        DE_POST_INSTALL="$(cat <<'CMD'
systemctl enable gdm3
systemctl set-default graphical.target
CMD
)"
        ;;
    opensuse)
        DE_PACKAGES="patterns-gnome-gnome gdm xorg-x11-server"
        DE_POST_INSTALL="$(cat <<'CMD'
systemctl enable gdm
systemctl set-default graphical.target
CMD
)"
        ;;
esac
