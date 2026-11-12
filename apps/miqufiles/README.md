# miqufiles

> **Yazi-Style Miller Columns File Manager for Wayland**

`miqufiles` is a fast, keyboard-first native Wayland file manager featuring a 3-column Miller Columns browsing interface and instant contextual previews, built with `miqutoolkit`.

Website & Documentation: [**miqu-project.github.io**](https://miqu-project.github.io)

---

## 🔍 What is What

- **Miller Columns Layout**:
  - **Left**: Parent directory navigation and folder breadcrumb context.
  - **Center**: Active folder contents with instant keyboard cursor traversal.
  - **Right**: Contextual live preview (child directory preview, high-resolution image rendering, monospace code/text syntax view, or binary file metadata).
- **Keyboard Ergonomics**: Navigate with `h`/`j`/`k`/`l` or Arrow keys; toggle hidden files with `.`; search with `/`; refresh with `F5`; jump to home with `~`.
- **Lightweight C++20 Core**: Instant startup, minimal RAM footprint, and 0% idle CPU usage.

---

## ⚙️ Configuration

Generate default configuration:
```bash
miqufiles --init-config
```
Config location: `~/.config/miqufiles/miqufiles.conf`.

---

## 🔨 Build & Run

```bash
# In monorepo root:
cmake --build build --target miqufiles

# Standalone:
./make.sh
```
