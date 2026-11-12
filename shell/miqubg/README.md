# miqubg

> **Independent Wayland Wallpaper Daemon & Background Renderer**

`miqubg` is a lightweight, dedicated wallpaper daemon for Wayland compositors, rendering and scaling background images across all connected displays via `wlr-layer-shell`.

Website & Documentation: [**miqu-project.github.io**](https://miqu-project.github.io)

---

## 🔍 What is What

- **Layer Shell Background**: Maps directly to the Wayland background layer (`wlr-layer-shell-unstable-v1`) with zero exclusive zone reservation.
- **Multi-Monitor Display Scaling**: Supports `fill`, `fit`, `stretch`, `center`, and `tile` scaling modes across individual outputs.
- **Dynamic Hotplugging**: Automatically detects newly attached monitors and immediately applies the active wallpaper configuration.
- **Zero Idle CPU**: Renders once to the compositor surface and sleeps; consumes 0.0% CPU when static.

---

## ⚙️ Usage & Configuration

```bash
# Set wallpaper on all monitors:
miqubg -i /path/to/wallpaper.png

# Set with scaling mode:
miqubg -i /path/to/wallpaper.png -m fill

# Generate user config:
miqubg --init-config
```
Config location: `~/.config/miqubg/miqubg.conf`.

---

## 🔨 Build & Run

```bash
# In monorepo root:
cmake --build build --target miqubg

# Standalone:
./make.sh
```
