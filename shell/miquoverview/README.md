# miquoverview

> **GPU-Accelerated Workspace Exposé & Window Manager Plugin for Miquland**

`miquoverview` provides an interactive, full-screen workspace expose and multitasking overview for `miquland`. It scales windows live with zero frame-dropping using hardware-accelerated scene nodes.

Website & Documentation: [**miqu-project.github.io**](https://miqu-project.github.io)

---

## 🔍 What is What

- **Live Scene Node Scaling**: Windows inside the active workspace are scaled smoothly in real time using wlroots scene nodes without CPU buffer copy overhead.
- **Interactive Workspace Pager**: Top navigation pill bar displays all active workspaces, open window counts, and current selection.
- **Direct Window & Workspace Dispatch**: Click any window to switch to that workspace and focus it immediately; click empty space to jump to that workspace; press `Esc` to dismiss.
- **Full Keyboard Navigation**: Arrow keys or `h`/`l` to switch workspaces, `j`/`k` or `Tab` to cycle windows, `1`-`9` for instant workspace jumps.
- **Dynamic Theming**: Automatically synchronizes colors and typography with `miqutheme`.

---

## ⚙️ Configuration & Keybindings

Add to `~/.config/miquland/miquland.conf`:
```ini
plugins {
    load = /usr/lib/miquland/plugins/libmiquoverview.so
}

bind = SUPER, Tab, overview_toggle
```
Config location: `~/.config/miquoverview/miquoverview.conf`.

---

## 🔨 Build & Run

```bash
# In monorepo root:
cmake --build build --target miquoverview

# Standalone:
./make.sh
```
