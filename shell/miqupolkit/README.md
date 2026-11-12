# miqupolkit

> **Native Graphical Polkit Authentication Agent for Wayland**

`miqupolkit` is a lightweight Wayland authentication agent built with `miqutoolkit` and `polkit-gobject-1`, handling privilege escalation prompts with layer-shell modal dialogs.

Website & Documentation: [**miqu-project.github.io**](https://miqu-project.github.io)

---

## 🔍 What is What

- **PolicyKit Integration**: Implements the `org.freedesktop.PolicyKit1.AuthenticationAgent` D-Bus interface.
- **Layer Shell Modal Dialog**: Centered modal dialog with keyboard focus grab using `wlr-layer-shell` on the active compositor.
- **Unified Design Tokens**: Uses `miqutoolkit` styling tokens, typography, and corner radiuses.
- **Lightweight C++20 Core**: Zero GTK or Qt desktop dependencies.

---

## ⚙️ Usage & Configuration

```bash
# Start agent (usually launched automatically in desktop autostart):
miqupolkit

# Generate user config:
miqupolkit --init-config
```
Config location: `~/.config/miqupolkit/miqupolkit.conf`.

---

## 🔨 Build & Run

```bash
# In monorepo root:
cmake --build build --target miqupolkit

# Standalone:
./make.sh
```
