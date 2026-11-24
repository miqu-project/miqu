# miqunotepad

**miqunotepad** is a modern, lightweight, native Wayland text editor built on **miqutoolkit** for the Miqu desktop ecosystem.

---

## Features

- **Blazing Fast Native Wayland UI**: Rendered with Cairo and Pango via `miqutoolkit`, with zero idle CPU utilization.
- **Two-Tier Configuration Management**: Seamless baseline (`/usr/share/miqunotepad/miqunotepad.conf`) and user customization (`~/.config/miqunotepad/miqunotepad.conf`) with `--init-config` safe archiving.
- **Decoupled Dynamic Theming**: Inherits system tokens, fonts, and dark/light color palettes from `miqutoolkit` and `miqusettings` with live reload via inotify.
- **Inline Action Bar**: Convenient, keyboard-driven file open and save dialogs right within the editor.
- **Full Keyboard & Mouse Ergonomics**: Familiar keybindings (`Ctrl+N`, `Ctrl+O`, `Ctrl+S`, `Ctrl+Shift+S`, `Ctrl+Q`), smooth scrolling, and live status bar telemetry.

---

## Keyboard Shortcuts

| Shortcut | Action |
| :--- | :--- |
| `Ctrl+N` | New Document |
| `Ctrl+O` | Open File Prompt |
| `Ctrl+S` | Save Document |
| `Ctrl+Shift+S` | Save As Prompt |
| `Ctrl+Q` | Quit miqunotepad |
| `Esc` | Dismiss Open/Save Prompt |
| `Ctrl+W` | Delete Previous Word |
| `Ctrl+U` | Clear Line/Text |

---

## Command Line Usage

```bash
# Launch with an empty document
miqunotepad

# Open a specific file
miqunotepad ~/notes.txt

# Safely initialize user config in ~/.config/miqunotepad/miqunotepad.conf
miqunotepad --init-config

# Show help options
miqunotepad --help
```
