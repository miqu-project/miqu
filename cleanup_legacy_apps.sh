#!/usr/bin/env bash
# ==============================================================================
# cleanup_legacy_apps.sh
# Safely purge all remnant files, binaries, configs, and assets from:
#   - miqutest
#   - miqusettings
#   - miqusecure
# ==============================================================================
set -euo pipefail

# Determine actual invoking user even if running under sudo
TARGET_USER="${SUDO_USER:-$USER}"
TARGET_HOME="$(getent passwd "$TARGET_USER" | cut -d: -f6)"

echo "================================================================"
echo " Miqu Ecosystem — Legacy Application Cleanup"
echo " Target User: $TARGET_USER ($TARGET_HOME)"
echo " Purging: miqutest, miqusettings, miqusecure"
echo "================================================================"

# Check for root privilege
if [ "${EUID}" -ne 0 ]; then
    echo "==> Note: System directories (/usr/bin, /usr/share) require root privileges."
    echo "==> Re-executing with sudo..."
    exec sudo bash "$0" "$@"
fi

remove_path() {
    local p="$1"
    if [ -e "$p" ] || [ -L "$p" ]; then
        echo "  [x] Removing: $p"
        rm -rf "$p"
    fi
}

echo ""
echo "----------------------------------------------------------------"
echo "==> [1/6] Purging system binaries (/usr/bin)..."
echo "----------------------------------------------------------------"
remove_path "/usr/bin/miqutest"
remove_path "/usr/bin/miqusettings"
remove_path "/usr/bin/miqusecure"

echo ""
echo "----------------------------------------------------------------"
echo "==> [2/6] Purging system desktop entries & config baselines..."
echo "----------------------------------------------------------------"
remove_path "/usr/share/applications/miqutest.desktop"
remove_path "/usr/share/applications/miqusettings.desktop"
remove_path "/usr/share/applications/miqusecure.desktop"
remove_path "/usr/share/miqutest"
remove_path "/usr/share/miqusettings"
remove_path "/usr/share/miqusecure"

echo ""
echo "----------------------------------------------------------------"
echo "==> [3/6] Purging system icon assets..."
echo "----------------------------------------------------------------"
for size in 16x16 22x22 24x24 32x32 48x48 64x64 128x128 256x256 512x512; do
    remove_path "/usr/share/icons/hicolor/${size}/apps/miqusecure.png"
    remove_path "/usr/share/icons/hicolor/${size}/apps/miqusettings.png"
    remove_path "/usr/share/icons/hicolor/${size}/apps/miqutest.png"
done

echo ""
echo "----------------------------------------------------------------"
echo "==> [4/6] Purging user-level configs, desktop entries & local icons..."
echo "----------------------------------------------------------------"
remove_path "${TARGET_HOME}/.config/miqutest"
remove_path "${TARGET_HOME}/.config/miqusettings"
remove_path "${TARGET_HOME}/.config/miqusecure"
remove_path "${TARGET_HOME}/.local/share/applications/miqusecure.desktop"
remove_path "${TARGET_HOME}/.local/share/applications/miqusettings.desktop"
remove_path "${TARGET_HOME}/.local/share/applications/miqutest.desktop"

for size in 16x16 22x22 24x24 32x32 48x48 64x64 128x128 256x256 512x512; do
    remove_path "${TARGET_HOME}/.local/share/icons/hicolor/${size}/apps/miqusecure.png"
    remove_path "${TARGET_HOME}/.local/share/icons/hicolor/${size}/apps/miqusettings.png"
    remove_path "${TARGET_HOME}/.local/share/icons/hicolor/${size}/apps/miqutest.png"
done

# Clean stale editor swap files
rm -f "${TARGET_HOME}/.local/state/nvim/swap/"*miqutest* 2>/dev/null || true
rm -f "${TARGET_HOME}/.local/state/nvim/swap/"*miqusettings* 2>/dev/null || true
rm -f "${TARGET_HOME}/.local/state/nvim/swap/"*miqusecure* 2>/dev/null || true

# Clean user stale builds directory
remove_path "${TARGET_HOME}/.stale_builds/miqusecure"
remove_path "${TARGET_HOME}/.stale_builds/miqusettings"
rmdir "${TARGET_HOME}/.stale_builds" 2>/dev/null || true

echo ""
echo "----------------------------------------------------------------"
echo "==> [5/6] Purging stale repository & temp build artifacts..."
echo "----------------------------------------------------------------"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
remove_path "${SCRIPT_DIR}/.old_miqu-linux-apps"
remove_path "${SCRIPT_DIR}/.old_miqutoolkit"

# Purge any stale root temp directories
for tmpdir in /tmp/miqusecure_stale_* /tmp/miqusettings_stale_* /tmp/*miqutest* /tmp/*miqusettings* /tmp/*miqusecure*; do
    if [ -e "$tmpdir" ]; then
        remove_path "$tmpdir"
    fi
done

echo ""
echo "----------------------------------------------------------------"
echo "==> [6/6] Refreshing system caches..."
echo "----------------------------------------------------------------"
update-desktop-database /usr/share/applications 2>/dev/null || true
if [ -d "${TARGET_HOME}/.local/share/applications" ]; then
    update-desktop-database "${TARGET_HOME}/.local/share/applications" 2>/dev/null || true
fi
gtk-update-icon-cache -f -t /usr/share/icons/hicolor 2>/dev/null || true

echo ""
echo "================================================================"
echo " Cleanup complete! All remnants of miqutest, miqusettings,"
echo " and miqusecure have been completely eliminated."
echo "================================================================"
