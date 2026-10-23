#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Auto-initialize submodules if any required build target is missing
if [ ! -f "$ROOT_DIR/miquland/CMakeLists.txt" ] || \
   [ ! -f "$ROOT_DIR/miqutoolkit/CMakeLists.txt" ] || \
   [ ! -f "$ROOT_DIR/miqu-linux-apps/CMakeLists.txt" ]; then
    echo "==> Required submodules not detected or uninitialized."
    echo "==> Automatically initializing Git submodules..."
    git -C "$ROOT_DIR" submodule update --init --recursive
fi

# Ensure AGENTS.md symlink exists
if [ ! -e "$ROOT_DIR/AGENTS.md" ] && [ -f "$ROOT_DIR/.github/AGENTS.md" ]; then
    ln -sf .github/AGENTS.md "$ROOT_DIR/AGENTS.md"
fi

if [ "${EUID}" -ne 0 ]; then
    echo "==> Warning: Running without root/sudo privileges."
    echo "    To install system-wide to /usr, run: sudo $0"
    echo ""
fi

echo "================================================================"
echo " Starting complete build & install for Miqu Ecosystem"
echo " Target directory: $ROOT_DIR"
echo "================================================================"

# 1. miquland (Compositor)
echo ""
echo "----------------------------------------------------------------"
echo "==> [1/3] Building & installing miquland..."
echo "----------------------------------------------------------------"
(
    cd "$ROOT_DIR/miquland"
    chmod +x make.sh
    ./make.sh "$@"
)
echo "==> miquland completed successfully."

# 2. miqutoolkit (Core library)
echo ""
echo "----------------------------------------------------------------"
echo "==> [2/3] Building & installing miqutoolkit..."
echo "----------------------------------------------------------------"
(
    cd "$ROOT_DIR/miqutoolkit"
    chmod +x make.sh
    ./make.sh "$@"
)
echo "==> miqutoolkit completed successfully."

# 3. miqu-linux-apps (First-party application suite)
echo ""
echo "----------------------------------------------------------------"
echo "==> [3/3] Building & installing miqu-linux-apps..."
echo "----------------------------------------------------------------"
(
    cd "$ROOT_DIR/miqu-linux-apps"
    chmod +x install_all.sh
    ./install_all.sh "$@"
)
echo "==> miqu-linux-apps completed successfully."

echo ""
echo "================================================================"
echo " All Miqu components built & installed successfully!"
echo "================================================================"
