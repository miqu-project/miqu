#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

NAME="miquland"
BUILD_DIR="${BUILD_DIR:-${SCRIPT_DIR}/build}"
BUILD_TYPE="${BUILD_TYPE:-Release}"

CMAKE_GENERATOR="Ninja"
if ! command -v ninja >/dev/null 2>&1; then
    CMAKE_GENERATOR="Unix Makefiles"
fi

echo "==> Configuring ${NAME} (${BUILD_TYPE}) using ${CMAKE_GENERATOR}..."
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_INSTALL_PREFIX="/usr" -G "$CMAKE_GENERATOR" "$@"

echo "==> Building ${NAME}..."
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo "==> Build finished successfully! Binary is at $BUILD_DIR/${NAME}"

# If invoked with sudo/root, automatically install to system
if [ "${EUID}" -eq 0 ]; then
    echo "==> Installing ${NAME} to system (/usr/bin, /usr/share/miquland, /usr/share/wayland-sessions)..."
    cmake --install "$BUILD_DIR"
    echo "==> ${NAME} successfully installed! You can select 'miquland' in your display manager or launch it via 'miquland'."
else
    echo "==> Built locally in $BUILD_DIR. To install system-wide, run: sudo ./make.sh"
fi
