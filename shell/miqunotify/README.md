# miqunotify

> **Native Wayland Desktop Notification Daemon & Slide-Out History Center**

`miqunotify` is a modern desktop notification service implementing the FreeDesktop notifications specification (v1.2) with floating toast banners and a slide-out notification drawer, built with `miqutoolkit` and `sd-bus`.

Website & Documentation: [**miqu-project.github.io**](https://miqu-project.github.io)

---

## 🔍 What is What

- **Corner Toast Banners**: Floating banners with application icons, timestamps, action buttons, progress bars, and hover-to-pause.
- **Slide-Out Notification Drawer**: Persistent side center (`miqunotify --toggle`) tracking notification history, DND toggle, and clear-all actions.
- **Do Not Disturb (DND)**: Suppresses popup banners while recording all notifications in history.
- **Miqubar Integration**: Automatically updates `/run/user/<uid>/miqunotify_state` so `miqubar` displays live unread notification counters.
- **FreeDesktop Specification**: Fully compatible with `notify-send` and desktop application notifications.

---

## ⚙️ Usage & Configuration

```bash
# Start daemon:
miqunotify

# Toggle notification center drawer:
miqunotify --toggle

# Re-show recent notification:
miqunotify --pop

# Generate user config:
miqunotify --init-config
```
Config location: `~/.config/miqunotify/miqunotify.conf`.

---

## 🔨 Build & Run

```bash
# In monorepo root:
cmake --build build --target miqunotify

# Standalone:
./make.sh
```
