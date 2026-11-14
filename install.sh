#!/usr/bin/env bash
# ==============================================================================
# Miqu Desktop Ecosystem — Master Monorepo Installer
# Builds and installs miquland, miqutoolkit, shell components, and apps.
# ==============================================================================
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build}"
BUILD_TYPE="${BUILD_TYPE:-Release}"

echo "================================================================"
echo " Miqu Desktop Ecosystem — Unified Monorepo Build & Install"
echo " Target directory: $ROOT_DIR"
echo "================================================================"

CMAKE_GENERATOR="Ninja"
if ! command -v ninja >/dev/null 2>&1; then
    CMAKE_GENERATOR="Unix Makefiles"
fi

echo "==> Configuring build system (${BUILD_TYPE}) using ${CMAKE_GENERATOR}..."
cmake -B "$BUILD_DIR" -S "$ROOT_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_INSTALL_PREFIX="/usr" \
    -DCMAKE_INSTALL_SYSCONFDIR="/etc" \
    -G "$CMAKE_GENERATOR" "$@"

echo "==> Compiling all ecosystem components in parallel..."
cmake --build "$BUILD_DIR" -j"$(nproc)"

if [ "${EUID}" -eq 0 ]; then
    echo ""
    echo "==> Installing system-wide to /usr and /etc..."
    cmake --install "$BUILD_DIR"

    echo "==> Running post-installation hooks..."
    ldconfig 2>/dev/null || true
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database /usr/share/applications 2>/dev/null || true
    fi
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        gtk-update-icon-cache -f -t /usr/share/icons/hicolor 2>/dev/null || true
    fi
    if command -v systemctl >/dev/null 2>&1; then
        systemctl daemon-reload 2>/dev/null || true
    fi

    echo ""
    echo "================================================================"
    echo " All Miqu components successfully built and installed!"
    echo "================================================================"
else
    echo ""
    echo "================================================================"
    echo " Build completed successfully in: $BUILD_DIR"
    echo " To install system-wide to /usr, run: sudo $0"
    echo "================================================================"
fi
