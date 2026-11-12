# miquland

> **Dynamic Tiling Wayland Compositor Built with wlroots & SceneFX**

`miquland` is the core Wayland compositor and window manager for the Miqu Desktop Ecosystem. Built with modern C++20, `wlroots`, and `SceneFX`, it delivers high-framerate dynamic tiling, GPU-accelerated window effects (rounded corners, dual-pass blurs, soft drop shadows), protocol-compliant session management, and a native IPC plugin architecture.

Website & Documentation: [**miqu-project.github.io/#docs**](https://miqu-project.github.io/#docs)

---

## 🔍 What is What

- **Dynamic Tiling Layouts**: Supports `spiral` (Fibonacci binary space partitioning) and `stack` (master-and-stack column layout) with dynamic splitting.
- **GPU Visual Pipeline**: Real-time corner rounding, dual-Kawase window blur, drop shadows, and border gradients via `SceneFX`.
- **Protocol Suite**: Implements `wlr-layer-shell-v1`, `ext-session-lock-v1`, `ext-idle-notify-v1`, `wlr-output-power-management-v1`, and XWayland.
- **IPC Daemon (`miquctl`)**: Unix domain socket controller for querying state, dispatching layouts, live event subscription, and graceful shutdown orchestration.
- **Two-Tier Configuration**: System baseline at `/usr/share/miquland/miquland.conf`, user config at `~/.config/miquland/miquland.conf` (created via `miquland --init-config` with safe timestamped archiving).

---

## ⌨️ Common Keybindings

| Key Combo | Action |
| :--- | :--- |
| `Super + Return` | Swap active window with master |
| `Super + T` | Open default terminal (`kitty`) |
| `Super + Space` | Open application launcher (`miqulauncher`) |
| `Super + Tab` | Toggle workspace overview (`miquoverview`) |
| `Super + Q` | Close focused window |
| `Super + V` | Toggle floating state |
| `Super + M` | Toggle fullscreen |
| `Super + L` | Toggle layout engine (`spiral` ↔ `stack`) |
| `Super + J` / `K` | Focus next / previous window |
| `Super + 1` .. `9` | Switch to workspace 1–9 |
| `Super + Shift + 1` .. `9` | Move window to workspace 1–9 |
| `Super + Alt + L` | Lock session (`miqulock`) |
| `Super + Shift + Q` | Graceful shutdown prompt (`miqupower`) |

---

## 🔨 Build & Run

```bash
# In monorepo root:
cmake --build build --target miquland

# Standalone build & install:
./make.sh
sudo ./make.sh
```
