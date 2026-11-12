# miqulauncher

> **Fast Fuzzy Application Launcher, Window Switcher & dmenu Alternative**

`miqulauncher` is a lightweight, keyboard-driven launcher and menu utility for Wayland, built with `miqutoolkit`. It offers Rofi/dmenu compatibility with modern styling and instant theme updates.

Website & Documentation: [**miqu-project.github.io**](https://miqu-project.github.io)

---

## 🔍 What is What

- **Application Launcher (`drun`)**: Fuzzy-searches installed desktop applications from standard system and user `.desktop` paths.
- **Active Window Switcher (`window`)**: Lists all running windows across all workspaces with live focus switching.
- **dmenu Mode (`dmenu`)**: Reads newline-separated items from standard input and prints the selected line to stdout (drop-in Rofi/dmenu replacement).
- **Fuzzy Matching**: High-speed substring and fuzzy scoring algorithm for instant search feedback.
- **Keyboard Ergonomics**: Navigate with Arrow keys or Vim keys (`j`/`k`, `Ctrl+N`/`Ctrl+P`), confirm with `Enter`, cancel with `Esc`.

---

## ⚙️ Usage & Configuration

```bash
# Launch app runner:
miqulauncher

# Switch windows:
miqulauncher --mode window

# Pipe into dmenu:
printf "Option A\nOption B\nOption C" | miqulauncher --dmenu

# Initialize config:
miqulauncher --init-config
```
Config location: `~/.config/miqulauncher/miqulauncher.conf`.

---

## 🔨 Build & Run

```bash
# In monorepo root:
cmake --build build --target miqulauncher

# Standalone:
./make.sh
```
