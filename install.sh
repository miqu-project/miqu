#!/usr/bin/env bash
# ==============================================================================
# Miqu Desktop Ecosystem — Master Monorepo Installer
# Builds and installs miquland, miqutoolkit, shell components, and apps.
# ==============================================================================
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build"

echo "================================================================"
echo " Miqu Desktop Ecosystem — Unified Monorepo Build & Install"
echo " Target directory: $ROOT_DIR"
echo "================================================================"

if [ "${EUID}" -ne 0 ]; then
    echo "==> Note: System-wide installation (/usr) requires root privileges."
    echo "    Running build locally. To install, execute: sudo $0"
    echo ""
fi

# Configure with Ninja if available, otherwise default to Unix Makefiles
CMAKE_GENERATOR="Ninja"
if ! command -v ninja >/dev/null 2>&1; then
    CMAKE_GENERATOR="Unix Makefiles"
fi

echo "==> Configuring build system using ${CMAKE_GENERATOR}..."
cmake -B "$BUILD_DIR" -S "$ROOT_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -G "$CMAKE_GENERATOR" "$@"

echo "==> Compiling all ecosystem components in parallel..."
cmake --build "$BUILD_DIR" -j"$(nproc)"

if [ "${EUID}" -eq 0 ]; then
    echo ""
    echo "==> Installing system-wide to /usr..."
    cmake --install "$BUILD_DIR"
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
