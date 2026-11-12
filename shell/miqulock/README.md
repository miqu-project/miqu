# miqulock

> **Protocol-Compliant Wayland Session Locker**

`miqulock` is a secure session lock utility implementing the Wayland `ext-session-lock-v1` protocol, featuring asynchronous PAM authentication, live theme reload, and real-time clock rendering.

Website & Documentation: [**miqu-project.github.io**](https://miqu-project.github.io)

---

## 🔍 What is What

- **Ext Session Lock Protocol**: Strictly shields all attached displays via `ext-session-lock-v1`; newly hotplugged monitors are locked immediately.
- **Secure PAM Validation**: Non-blocking authentication worker with memory zeroing (`explicit_bzero`) for sensitive password buffers.
- **Real-Time Clock & Feedback**: Second-accurate clock display, Caps Lock indicator badge, and 60fps authentication failure shake animations.
- **Dynamic Theming**: Automatically reloads colors and typography when `~/.config/miqulock/miqulock.conf` or `miqutheme` changes.

---

## ⚙️ Usage & Configuration

```bash
# Lock the screen:
miqulock

# Generate user config:
miqulock --init-config
```
Config location: `~/.config/miqulock/miqulock.conf`.

---

## 🔨 Build & Run

```bash
# In monorepo root:
cmake --build build --target miqulock

# Standalone:
./make.sh
```
