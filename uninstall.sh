#!/usr/bin/env bash
# ==============================================================================
# Miqu Desktop Ecosystem — Master Uninstallation & Purge Script
# Thoroughly removes all binaries, libraries, headers, desktop entries, icons,
# services, PAM configurations, and system data across all Miqu components.
# ==============================================================================
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TARGET_USER="${SUDO_USER:-$USER}"
TARGET_HOME="$(getent passwd "$TARGET_USER" 2>/dev/null | cut -d: -f6 || echo "$HOME")"

PURGE_USER_DATA=false
for arg in "$@"; do
    case "$arg" in
        --purge-user-data|--all|-a)
            PURGE_USER_DATA=true
            ;;
        --help|-h)
            echo "Usage: sudo $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --purge-user-data, --all, -a   Also purge user config (~/.config/miqu*) and caches"
            echo "  --help, -h                     Show this help message"
            exit 0
            ;;
    esac
done

echo "================================================================"
echo " Miqu Desktop Ecosystem — System Uninstaller"
echo " Target User: ${TARGET_USER} (${TARGET_HOME})"
echo "================================================================"

# Require root privilege
if [ "${EUID}" -ne 0 ]; then
    echo "==> Error: Uninstalling system files requires root privileges."
    echo "==> Re-executing with sudo..."
    exec sudo bash "$0" "$@"
fi

remove_path() {
    local p="$1"
    if [ -e "$p" ] || [ -L "$p" ]; then
        echo "  [-] Removing: $p"
        rm -rf "$p"
    fi
}

# ------------------------------------------------------------------------------
# 1. CMake Install Manifest (if present in build directories)
# ------------------------------------------------------------------------------
echo ""
echo "----------------------------------------------------------------"
echo "==> [1/8] Removing files recorded in CMake install manifests..."
echo "----------------------------------------------------------------"
for manifest in "${ROOT_DIR}/build/install_manifest.txt" \
                "${ROOT_DIR}"/*/build/install_manifest.txt \
                "${ROOT_DIR}"/*/*/build/install_manifest.txt \
                "${ROOT_DIR}"/*/*/build.root/install_manifest.txt; do
    if [ -f "$manifest" ]; then
        echo "==> Processing manifest: $manifest"
        while IFS= read -r file || [ -n "$file" ]; do
            if [ -n "$file" ]; then
                remove_path "$file"
            fi
        done < "$manifest"
    fi
done

# ------------------------------------------------------------------------------
# 2. System Binaries (/usr/bin)
# ------------------------------------------------------------------------------
echo ""
echo "----------------------------------------------------------------"
echo "==> [2/8] Purging system binaries (/usr/bin)..."
echo "----------------------------------------------------------------"
BINARIES=(
    # Core & Compositor
    "miquland"
    "miquctl"
    "miqutoolkit-cli"
    # Shell & Daemons
    "miqubar"
    "miqudesk"
    "miqulauncher"
    "miquoverview"
    "miqunotify"
    "miqupower"
    "miqudm"
    "miqudm-greeter"
    "miqubg"
    "miqulock"
    "miquidle"
    "miqupolkit"
    # Applications
    "miqucontrolpanel"
    "miqufiles"
    "miqumusic"
    "miqugallery"
    "miqutheme"
    "miquinfo"
    # Legacy
    "miqutest"
    "miqusettings"
    "miqusecure"
)

for bin in "${BINARIES[@]}"; do
    remove_path "/usr/bin/${bin}"
done

# ------------------------------------------------------------------------------
# 3. Libraries & pkg-config (/usr/lib)
# ------------------------------------------------------------------------------
echo ""
echo "----------------------------------------------------------------"
echo "==> [3/8] Purging shared libraries & pkg-config (/usr/lib)..."
echo "----------------------------------------------------------------"
for lib in /usr/lib/libmiqu* /usr/lib64/libmiqu*; do
    remove_path "$lib"
done
remove_path "/usr/lib/pkgconfig/miqutoolkit.pc"
remove_path "/usr/lib/pkgconfig/miquland.pc"
remove_path "/usr/lib/miquland"
remove_path "/usr/lib/miqudm"

# ------------------------------------------------------------------------------
# 4. Development Headers (/usr/include)
# ------------------------------------------------------------------------------
echo ""
echo "----------------------------------------------------------------"
echo "==> [4/8] Purging development headers (/usr/include)..."
echo "----------------------------------------------------------------"
remove_path "/usr/include/miqutoolkit"
remove_path "/usr/include/miquland"

# ------------------------------------------------------------------------------
# 5. System Configurations & Data (/usr/share, /etc)
# ------------------------------------------------------------------------------
echo ""
echo "----------------------------------------------------------------"
echo "==> [5/8] Purging system data & configs (/usr/share, /etc)..."
echo "----------------------------------------------------------------"
for datadir in /usr/share/miqu* /usr/share/wayland-sessions/miquland.desktop /usr/share/xdg-desktop-portal/miquland*; do
    remove_path "$datadir"
done

# Desktop application entries
for desktop in /usr/share/applications/miqu*.desktop; do
    remove_path "$desktop"
done

# Systemd units & PAM configs
remove_path "/usr/lib/systemd/system/miqudm.service"
remove_path "/usr/lib/systemd/user/miqupolkit.service"
remove_path "/etc/pam.d/miqudm"
remove_path "/etc/pam.d/miqudm-greeter"
remove_path "/etc/miqudm"

# ------------------------------------------------------------------------------
# 6. Icons (/usr/share/icons/hicolor)
# ------------------------------------------------------------------------------
echo ""
echo "----------------------------------------------------------------"
echo "==> [6/8] Purging icons (/usr/share/icons/hicolor)..."
echo "----------------------------------------------------------------"
for size in 16x16 22x22 24x24 32x32 48x48 64x64 128x128 256x256 512x512 scalable; do
    for icon in /usr/share/icons/hicolor/${size}/apps/miqu*.*; do
        remove_path "$icon"
    done
done

# ------------------------------------------------------------------------------
# 7. User Configuration & Caches (Optional or with --purge-user-data)
# ------------------------------------------------------------------------------
echo ""
echo "----------------------------------------------------------------"
echo "==> [7/8] User configurations & caches..."
echo "----------------------------------------------------------------"
if [ "$PURGE_USER_DATA" = true ]; then
    echo "==> Purging user data for ${TARGET_USER}..."
    for user_conf in "${TARGET_HOME}/.config"/miqu*; do
        remove_path "$user_conf"
    done
    for user_cache in "${TARGET_HOME}/.cache"/miqu*; do
        remove_path "$user_cache"
    done
    for user_share in "${TARGET_HOME}/.local/share"/miqu* "${TARGET_HOME}/.local/share/applications"/miqu*.desktop; do
        remove_path "$user_share"
    done
else
    echo "  [i] Skipping user configs (~/.config/miqu*). Run with --purge-user-data to remove."
fi

# Clean leftover build directories in workspace
remove_path "${ROOT_DIR}/.old_miqu-linux-apps"
remove_path "${ROOT_DIR}/.old_miqutoolkit"

# ------------------------------------------------------------------------------
# 8. Refresh System Caches
# ------------------------------------------------------------------------------
echo ""
echo "----------------------------------------------------------------"
echo "==> [8/8] Refreshing system caches..."
echo "----------------------------------------------------------------"
ldconfig 2>/dev/null || true
update-desktop-database /usr/share/applications 2>/dev/null || true
if [ -d "${TARGET_HOME}/.local/share/applications" ]; then
    update-desktop-database "${TARGET_HOME}/.local/share/applications" 2>/dev/null || true
fi
gtk-update-icon-cache -f -t /usr/share/icons/hicolor 2>/dev/null || true

echo ""
echo "================================================================"
echo " Uninstallation complete! All Miqu ecosystem components"
echo " have been completely purged from your system."
echo "================================================================"
