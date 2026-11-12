# miqubar

> **Wayland Status Bar & Windows-Style Taskbar**

`miqubar` is the native Wayland desktop panel and taskbar for the Miqu ecosystem, built with `miqutoolkit` using layer-shell surface protocols.

Website & Documentation: [**miqu-project.github.io**](https://miqu-project.github.io)

---

## 🔍 What is What

- **Start Menu Button**: Quick launcher trigger (launches `miqulauncher` by default).
- **Workspace Indicator**: Shows the active workspace with a badge (`[ ⊞ 1 ]`); hover or click to reveal an interactive workspace flyout.
- **Foreign Toplevel Taskbar**: Tracks open windows via `zwlr_foreign_toplevel_manager_v1` with application icons, title text, and active accent underlines. Click to focus/minimize, middle-click to close.
- **Hardware Telemetry Modules**: Real-time CPU, RAM, and Battery percentages with warning thresholds.
- **Audio Control**: Audio volume slider and quick settings flyout.
- **Stacked Clock & Calendar**: Compact time/date display; click to open the interactive monthly calendar flyout.
- **Layer Shell Anchoring**: Anchored to screen edges with an exclusive zone so tiling windows adjust automatically.

---

## ⚙️ Configuration

Generate user configuration:
```bash
miqubar --init-config
```
Config location: `~/.config/miqubar/miqubar.conf` (automatically watched via inotify for live reload).

---

## 🔨 Build & Run

```bash
# In monorepo root:
cmake --build build --target miqubar

# Standalone:
./make.sh
```
