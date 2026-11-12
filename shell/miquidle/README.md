# miquidle

> **Wayland Idle Management Daemon & Power State Coordinator**

`miquidle` monitors user activity and coordinates display power saving and screen locking via the Wayland `ext-idle-notify-v1` protocol and `systemd-logind`.

Website & Documentation: [**miqu-project.github.io**](https://miqu-project.github.io)

---

## 🔍 What is What

- **Ext Idle Notify Protocol**: Listens for user inactivity timeouts natively from the Wayland compositor with full protocol inhibitor support.
- **Logind & Sleep Integration**: Integrates with `systemd-logind` to acquire delay inhibitors and lock the session before system suspend.
- **Configurable Listeners**: Modular timeout triggers (`timeout`, `on-timeout`, `on-resume`) and command hooks (`lock_cmd`, `before_sleep_cmd`, `after_sleep_cmd`).
- **Signal Handling**: Responds to `SIGUSR1` for instant lock testing, and `SIGTERM`/`SIGINT` for graceful termination.

---

## ⚙️ Usage & Configuration

```bash
# Start daemon with default config:
miquidle

# Specify inline commands (swayidle-compatible):
miquidle timeout 300 'miqulock' before-sleep 'miqulock'
```
Config location: `~/.config/miquidle/miquidle.conf`.

---

## 🔨 Build & Run

```bash
# In monorepo root:
cmake --build build --target miquidle

# Standalone:
./make.sh
```
