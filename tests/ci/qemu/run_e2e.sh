#!/usr/bin/env bash
# End-to-end test orchestrator: boots a throwaway QEMU VM with a real DE,
# installs the Laren package, and runs verification.
#
# Usage: ./run_e2e.sh [--gui] <distro> <de> [package-path]
#   --gui:   Open a QEMU window, boot into the DE, and keep VM alive for
#            manual inspection. Default is headless (automated test only).
#   distro:  arch | fedora | ubuntu | opensuse
#   de:      kde-plasma | gnome | sway | hyprland
#   package: path to .pkg.tar.zst / .rpm / .deb  (optional; built from source if omitted)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CACHE_DIR="${HOME}/.cache/laren-e2e"
SSH_PORT=2222
VNC_DISPLAY=1
VM_RAM=4096
VM_CPUS=2
SSH_USER="laren"
SSH_PASS="laren"
SSH_OPTS="-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o ConnectTimeout=5"
BOOT_TIMEOUT=600  # 10 minutes for cloud-init
INTERACTIVE=false

info()  { echo -e "\033[1;34m==>\033[0m \033[1m$*\033[0m"; }
warn()  { echo -e "\033[1;33m==> WARNING:\033[0m $*"; }
error() { echo -e "\033[1;31m==> ERROR:\033[0m $*" >&2; exit 1; }

usage() {
    echo "Usage: $0 [--gui] <distro> <de> [package-path]"
    echo ""
    echo "  --gui:   Show QEMU window, boot into the DE after install,"
    echo "           and keep VM running for manual inspection."
    echo "  distro:  arch | fedora | ubuntu | opensuse"
    echo "  de:      kde-plasma | gnome | sway | hyprland"
    echo "  package: path to .pkg.tar.zst / .rpm / .deb"
    exit 1
}

# --- Argument parsing -------------------------------------------------------

# Extract --gui flag, collect remaining positional args
POSITIONAL=()
for arg in "$@"; do
    case "$arg" in
        --gui) INTERACTIVE=true ;;
        *)     POSITIONAL+=("$arg") ;;
    esac
done
set -- "${POSITIONAL[@]+"${POSITIONAL[@]}"}"

[[ $# -lt 2 ]] && usage

DISTRO="$1"
DE="$2"
PACKAGE="${3:-}"

case "$DISTRO" in
    arch|fedora|ubuntu|opensuse) ;;
    *) error "Unknown distro: $DISTRO (supported: arch, fedora, ubuntu, opensuse)" ;;
esac

case "$DE" in
    kde-plasma|gnome|sway|hyprland) ;;
    *) error "Unknown DE: $DE (supported: kde-plasma, gnome, sway, hyprland)" ;;
esac

PROFILE_SCRIPT="${SCRIPT_DIR}/profiles/${DE}.sh"
[[ -f "$PROFILE_SCRIPT" ]] || error "Profile not found: $PROFILE_SCRIPT"

if [[ -n "$PACKAGE" ]]; then
    [[ -f "$PACKAGE" ]] || error "Package file not found: $PACKAGE"
    PACKAGE="$(realpath "$PACKAGE")"
fi

# --- Prerequisites check ----------------------------------------------------

for cmd in qemu-system-x86_64 cloud-localds sshpass; do
    if ! command -v "$cmd" &>/dev/null; then
        # cloud-localds might not exist; we fall back to genisoimage
        if [[ "$cmd" == "cloud-localds" ]]; then
            if ! command -v genisoimage &>/dev/null && ! command -v mkisofs &>/dev/null; then
                error "Need cloud-localds, genisoimage, or mkisofs to create cloud-init ISO"
            fi
        else
            error "Required command not found: $cmd"
        fi
    fi
done

# --- Cleanup on exit --------------------------------------------------------

WORK_DIR=""
QEMU_PID=""

cleanup() {
    info "Cleaning up"
    if [[ -n "$QEMU_PID" ]] && kill -0 "$QEMU_PID" 2>/dev/null; then
        kill "$QEMU_PID" 2>/dev/null || true
        wait "$QEMU_PID" 2>/dev/null || true
    fi
    if [[ -n "$WORK_DIR" && -d "$WORK_DIR" ]]; then
        rm -rf "$WORK_DIR"
    fi
}
trap cleanup EXIT INT TERM

# --- Download cloud image ----------------------------------------------------

download_image() {
    mkdir -p "$CACHE_DIR"

    case "$DISTRO" in
        arch)
            IMAGE_URL="https://geo.mirror.pkgbuild.com/images/latest/Arch-Linux-x86_64-cloudimg.qcow2"
            IMAGE_FILE="$CACHE_DIR/arch-cloudimg.qcow2"
            ;;
        fedora)
            IMAGE_URL="https://download.fedoraproject.org/pub/fedora/linux/releases/41/Cloud/x86_64/images/Fedora-Cloud-Base-Generic-41-1.4.x86_64.qcow2"
            IMAGE_FILE="$CACHE_DIR/fedora-41-cloudimg.qcow2"
            ;;
        ubuntu)
            IMAGE_URL="https://cloud-images.ubuntu.com/noble/current/noble-server-cloudimg-amd64.img"
            IMAGE_FILE="$CACHE_DIR/ubuntu-noble-cloudimg.qcow2"
            ;;
        opensuse)
            IMAGE_URL="https://download.opensuse.org/tumbleweed/appliances/openSUSE-Tumbleweed-Minimal-VM.x86_64-Cloud.qcow2"
            IMAGE_FILE="$CACHE_DIR/opensuse-tw-cloudimg.qcow2"
            ;;
    esac

    if [[ -f "$IMAGE_FILE" ]]; then
        info "Using cached image: $IMAGE_FILE"
    else
        info "Downloading cloud image: $IMAGE_URL"
        curl -L -# -o "${IMAGE_FILE}.tmp" "$IMAGE_URL"
        mv "${IMAGE_FILE}.tmp" "$IMAGE_FILE"
    fi
}

# --- Create work directory and overlay ---------------------------------------

create_overlay() {
    WORK_DIR="$(mktemp -d /tmp/laren-e2e-XXXXXX)"
    info "Work directory: $WORK_DIR"

    OVERLAY="$WORK_DIR/disk.qcow2"
    qemu-img create -f qcow2 -b "$IMAGE_FILE" -F qcow2 "$OVERLAY" 20G
    info "Created overlay: $OVERLAY"
}

# --- Generate cloud-init config ----------------------------------------------

generate_cloud_init() {
    # Source the DE profile to get DE_PACKAGES and DE_POST_INSTALL
    DE_PACKAGES=""
    DE_POST_INSTALL=""
    # shellcheck source=/dev/null
    source "$PROFILE_SCRIPT"

    local userdata="$WORK_DIR/user-data"
    local metadata="$WORK_DIR/meta-data"

    cat > "$metadata" <<EOF
instance-id: laren-e2e-${DISTRO}-${DE}
local-hostname: laren-e2e
EOF

    cat > "$userdata" <<USERDATA
#cloud-config
hostname: laren-e2e
users:
  - name: ${SSH_USER}
    plain_text_passwd: "${SSH_PASS}"
    sudo: ALL=(ALL) NOPASSWD:ALL
    lock_passwd: false
    shell: /bin/bash
    ssh_authorized_keys: []

ssh_pwauth: true

package_update: true
USERDATA

    # In GUI mode: write autologin config files via cloud-init write_files
    # (much more reliable than generating them via runcmd shell commands)
    if $INTERACTIVE; then
        case "$DE" in
            kde-plasma)
                cat >> "$userdata" <<'WRITEFILES'

write_files:
  - path: /etc/sddm.conf
    content: |
      [Autologin]
      User=laren
      Session=plasma
    owner: root:root
    permissions: '0644'
WRITEFILES
                ;;
            gnome)
                cat >> "$userdata" <<'WRITEFILES'

write_files:
  - path: /etc/gdm/custom.conf
    content: |
      [daemon]
      AutomaticLoginEnable=true
      AutomaticLogin=laren
    owner: root:root
    permissions: '0644'
WRITEFILES
                ;;
            sway)
                cat >> "$userdata" <<WRITEFILES

write_files:
  - path: /etc/systemd/system/getty@tty1.service.d/autologin.conf
    content: |
      [Service]
      ExecStart=
      ExecStart=-/sbin/agetty --autologin ${SSH_USER} --noclear %I \$TERM
    owner: root:root
    permissions: '0644'
  - path: /home/${SSH_USER}/.bash_profile
    content: |
      [ -z "\$DISPLAY" ] && [ "\$(tty)" = "/dev/tty1" ] && exec sway
    owner: ${SSH_USER}:${SSH_USER}
    permissions: '0644'
WRITEFILES
                ;;
            hyprland)
                cat >> "$userdata" <<WRITEFILES

write_files:
  - path: /etc/systemd/system/getty@tty1.service.d/autologin.conf
    content: |
      [Service]
      ExecStart=
      ExecStart=-/sbin/agetty --autologin ${SSH_USER} --noclear %I \$TERM
    owner: root:root
    permissions: '0644'
  - path: /home/${SSH_USER}/.bash_profile
    content: |
      [ -z "\$DISPLAY" ] && [ "\$(tty)" = "/dev/tty1" ] && exec Hyprland
    owner: ${SSH_USER}:${SSH_USER}
    permissions: '0644'
WRITEFILES
                ;;
        esac
    fi

    # Add packages if the profile defined them
    if [[ -n "$DE_PACKAGES" ]]; then
        echo "packages:" >> "$userdata"
        # Split DE_PACKAGES by space and write each as a YAML list item
        for pkg in $DE_PACKAGES; do
            echo "  - $pkg" >> "$userdata"
        done
    fi

    # Build runcmd list
    local has_runcmd=false

    if [[ -n "$DE_POST_INSTALL" ]]; then
        echo "runcmd:" >> "$userdata"
        has_runcmd=true
        while IFS= read -r cmd; do
            [[ -z "$cmd" ]] && continue
            echo "  - $cmd" >> "$userdata"
        done <<< "$DE_POST_INSTALL"
    fi

    # In GUI mode: ensure graphical target is set via runcmd
    if $INTERACTIVE; then
        if ! $has_runcmd; then
            echo "runcmd:" >> "$userdata"
        fi

        echo "  - systemctl set-default graphical.target" >> "$userdata"
    fi

    # Power state: don't reboot, we'll drive it over SSH
    cat >> "$userdata" <<'USERDATA'

power_state:
  mode: poweroff
  condition: false

final_message: "cloud-init complete after $UPTIME seconds"
USERDATA

    info "Generated cloud-init config"

    # Create cloud-init ISO
    local ciiso="$WORK_DIR/cloud-init.iso"
    if command -v cloud-localds &>/dev/null; then
        cloud-localds "$ciiso" "$userdata" "$metadata"
    elif command -v genisoimage &>/dev/null; then
        genisoimage -output "$ciiso" -volid cidata -joliet -rock \
            "$userdata" "$metadata" 2>/dev/null
    elif command -v mkisofs &>/dev/null; then
        mkisofs -output "$ciiso" -volid cidata -joliet -rock \
            "$userdata" "$metadata" 2>/dev/null
    else
        error "No tool available to create cloud-init ISO"
    fi

    info "Created cloud-init ISO: $ciiso"
}

# --- Boot QEMU ---------------------------------------------------------------

boot_vm() {
    info "Booting QEMU VM (${DISTRO}/${DE})"
    info "  RAM: ${VM_RAM}MB, CPUs: ${VM_CPUS}"
    info "  SSH: localhost:${SSH_PORT}"
    $INTERACTIVE && info "  Mode: GUI (window)" || info "  VNC: :${VNC_DISPLAY}"

    # Check if SSH port is already in use
    if ss -tlnp 2>/dev/null | grep -q ":${SSH_PORT} " || \
       netstat -tlnp 2>/dev/null | grep -q ":${SSH_PORT} "; then
        error "Port ${SSH_PORT} already in use. Is another VM running?"
    fi

    local kvm_flag=""
    if [[ -r /dev/kvm ]]; then
        kvm_flag="-enable-kvm -cpu host"
    else
        warn "KVM not available, VM will be slow"
        kvm_flag="-cpu qemu64"
    fi

    local display_opts
    if $INTERACTIVE; then
        display_opts="-device virtio-vga -display gtk"
    else
        display_opts="-vnc :${VNC_DISPLAY} -display none"
    fi

    if $INTERACTIVE; then
        # Can't use -daemonize with -display gtk; background with &
        # shellcheck disable=SC2086
        qemu-system-x86_64 \
            $kvm_flag \
            -m "$VM_RAM" \
            -smp "$VM_CPUS" \
            -drive file="$WORK_DIR/disk.qcow2",format=qcow2,if=virtio \
            -drive file="$WORK_DIR/cloud-init.iso",format=raw,if=virtio \
            -netdev user,id=net0,hostfwd=tcp::${SSH_PORT}-:22 \
            -device virtio-net-pci,netdev=net0 \
            $display_opts \
            -pidfile "$WORK_DIR/qemu.pid" \
            &
        QEMU_PID=$!
    else
        # shellcheck disable=SC2086
        qemu-system-x86_64 \
            $kvm_flag \
            -m "$VM_RAM" \
            -smp "$VM_CPUS" \
            -drive file="$WORK_DIR/disk.qcow2",format=qcow2,if=virtio \
            -drive file="$WORK_DIR/cloud-init.iso",format=raw,if=virtio \
            -netdev user,id=net0,hostfwd=tcp::${SSH_PORT}-:22 \
            -device virtio-net-pci,netdev=net0 \
            $display_opts \
            -daemonize \
            -pidfile "$WORK_DIR/qemu.pid" \
            || error "Failed to start QEMU"

        QEMU_PID="$(cat "$WORK_DIR/qemu.pid")"
    fi

    info "QEMU started (PID: $QEMU_PID)"
}

# --- Wait for SSH ------------------------------------------------------------

wait_for_ssh() {
    info "Waiting for SSH (up to ${BOOT_TIMEOUT}s for cloud-init)..."
    local start_time elapsed
    start_time=$(date +%s)

    while true; do
        elapsed=$(( $(date +%s) - start_time ))
        if [[ $elapsed -ge $BOOT_TIMEOUT ]]; then
            error "Timed out waiting for SSH after ${BOOT_TIMEOUT}s"
        fi

        # Check QEMU is still alive
        if ! kill -0 "$QEMU_PID" 2>/dev/null; then
            error "QEMU process died unexpectedly"
        fi

        # Try SSH
        if sshpass -p "$SSH_PASS" ssh $SSH_OPTS -p "$SSH_PORT" "${SSH_USER}@localhost" \
            "echo ok" &>/dev/null; then
            info "SSH ready (after ${elapsed}s)"
            return
        fi

        sleep 5
    done
}

# --- Run command in VM -------------------------------------------------------

vm_exec() {
    sshpass -p "$SSH_PASS" ssh $SSH_OPTS -p "$SSH_PORT" "${SSH_USER}@localhost" "$@"
}

vm_copy() {
    sshpass -p "$SSH_PASS" scp $SSH_OPTS -P "$SSH_PORT" "$1" "${SSH_USER}@localhost:$2"
}

# --- Wait for cloud-init to finish ------------------------------------------

wait_for_cloud_init() {
    info "Waiting for cloud-init to finish..."
    local start_time elapsed
    start_time=$(date +%s)

    while true; do
        elapsed=$(( $(date +%s) - start_time ))
        if [[ $elapsed -ge $BOOT_TIMEOUT ]]; then
            warn "cloud-init still running after ${BOOT_TIMEOUT}s, proceeding anyway"
            return
        fi

        if vm_exec "test -f /var/lib/cloud/instance/boot-finished" 2>/dev/null; then
            info "cloud-init finished (after ${elapsed}s)"
            return
        fi

        sleep 10
    done
}

# --- Install package in VM ---------------------------------------------------

install_package() {
    if [[ -n "$PACKAGE" ]]; then
        local pkg_name
        pkg_name="$(basename "$PACKAGE")"
        info "Copying package to VM: $pkg_name"
        vm_copy "$PACKAGE" "/tmp/$pkg_name"
        info "Copying install helper to VM"
        vm_copy "$SCRIPT_DIR/install_package.sh" "/tmp/install_package.sh"
        vm_exec "chmod +x /tmp/install_package.sh && sudo /tmp/install_package.sh /tmp/$pkg_name"
    else
        info "No package specified; building from source in VM"
        vm_copy "$SCRIPT_DIR/install_from_source.sh" "/tmp/install_from_source.sh"
        vm_exec "chmod +x /tmp/install_from_source.sh && sudo /tmp/install_from_source.sh"
    fi
}

# --- Run verification -------------------------------------------------------

run_verification() {
    info "Copying verification script to VM"
    vm_copy "$SCRIPT_DIR/verify_ime.sh" "/tmp/verify_ime.sh"
    vm_exec "chmod +x /tmp/verify_ime.sh"

    info "Running verification (DISTRO=$DISTRO, DE=$DE)"
    # Pass DISTRO and DE as env vars so verify_ime.sh can do DE-specific checks
    vm_exec "DISTRO=$DISTRO DE=$DE GUI_MODE=false /tmp/verify_ime.sh" || true
}

# --- Capture screenshot ------------------------------------------------------

capture_screenshot() {
    local screenshot_dir="${SCRIPT_DIR}/../../../build/e2e-screenshots"
    mkdir -p "$screenshot_dir"
    local screenshot_file="${screenshot_dir}/${DISTRO}-${DE}-$(date +%Y%m%d-%H%M%S).ppm"

    info "Capturing VNC screenshot"
    # Use QEMU monitor to screendump (connect via QMP isn't needed for basic screendump)
    # The simplest way is to use gvnccapture or vncsnapshot if available
    if command -v gvnccapture &>/dev/null; then
        gvnccapture "localhost:${VNC_DISPLAY}" "$screenshot_file" 2>/dev/null && \
            info "Screenshot saved: $screenshot_file" || \
            warn "Failed to capture screenshot (VM may not have display output yet)"
    elif command -v vncsnapshot &>/dev/null; then
        vncsnapshot "localhost:${VNC_DISPLAY}" "$screenshot_file" 2>/dev/null && \
            info "Screenshot saved: $screenshot_file" || \
            warn "Failed to capture screenshot"
    else
        warn "No VNC capture tool (gvnccapture or vncsnapshot) found; skipping screenshot"
    fi
}

# --- GUI mode: reboot into graphical session ---------------------------------

interactive_session() {
    info "Rebooting VM into graphical desktop..."
    vm_exec "sudo reboot" 2>/dev/null || true

    echo ""
    info "The VM is rebooting into ${DE}."
    info "It will auto-login as user '${SSH_USER}' (password: ${SSH_PASS})."
    info "You can SSH in:  sshpass -p ${SSH_PASS} ssh -p ${SSH_PORT} ${SSH_USER}@localhost"
    info "Close the QEMU window when you're done."
    echo ""

    # Disable cleanup trap — we don't want to kill the VM while the user is using it
    trap - EXIT INT TERM

    # Wait for user to close the QEMU window
    wait "$QEMU_PID" 2>/dev/null || true

    # Clean up work directory after VM exits
    if [[ -n "$WORK_DIR" && -d "$WORK_DIR" ]]; then
        rm -rf "$WORK_DIR"
    fi
}

# --- Main --------------------------------------------------------------------

main() {
    info "Laren E2E test: ${DISTRO} + ${DE}"
    [[ -n "$PACKAGE" ]] && info "Package: $PACKAGE"
    $INTERACTIVE && info "Mode: GUI (interactive)"
    echo ""

    download_image
    create_overlay
    generate_cloud_init
    boot_vm
    wait_for_ssh
    wait_for_cloud_init
    install_package
    run_verification

    if $INTERACTIVE; then
        interactive_session
    else
        capture_screenshot
    fi

    info "E2E test complete for ${DISTRO}/${DE}"
}

main
