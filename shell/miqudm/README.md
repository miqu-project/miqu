# miqudm

> **Wayland-Native Display Manager & Login Greeter**

`miqudm` is a lightweight, pure Wayland display manager and session greeter designed for modern Wayland desktop sessions, integrating PAM authentication, logind seat management, and `miqutoolkit`.

Website & Documentation: [**miqu-project.github.io**](https://miqu-project.github.io)

---

## 🔍 What is What

- **`miqudm` Daemon**: System service running as root managing VTs, logind seats, user enumeration, asynchronous PAM authentication, and privilege dropping.
- **`miqudm-greeter`**: Unprivileged Wayland UI client rendered inside a minimal kiosk compositor (`cage`). Displays user avatars, password entry, session selector, and power buttons.
- **Pre-Session Authority**: Dedicated system-wide configuration at `/etc/miqudm/miqudm.conf` (avoids touching user directories before authentication).
- **Interactive Testing Mode**: Run `miqudm-greeter --test` to test the greeter UI live directly inside your running desktop session.

---

## ⚙️ Service Activation & Configuration

```bash
# Enable as system display manager:
sudo systemctl enable miqudm.service

# Test greeter UI without logging out:
miqudm-greeter --test
```
Config location: `/etc/miqudm/miqudm.conf`.

---

## 🔨 Build & Run

```bash
# In monorepo root:
cmake --build build --target miqudm

# Standalone:
./make.sh
```
