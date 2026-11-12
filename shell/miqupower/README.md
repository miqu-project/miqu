# miqupower

> **Desktop Power & Session Management Dialog with Running Application Guards**

`miqupower` is a native Wayland session dialog built with `miqutoolkit`. It provides accessible power controls, running window warnings, and graceful shutdown orchestration for `miquland`.

Website & Documentation: [**miqu-project.github.io**](https://miqu-project.github.io)

---

## 🔍 What is What

- **5 Core Power Actions**: Shut Down (`poweroff`), Restart (`reboot`), Sleep (`suspend` with auto-lock), Lock (`miqulock`), and Log Out (graceful compositor exit).
- **Active Application Guard**: Queries running windows from `miquland` via IPC (`clients`); warns the user before shutdown or reboot to prevent data loss.
- **Graceful Teardown Feedback**: Renders an overlay progress view while windows close cleanly before power termination.
- **Keyboard Ergonomics**: Hotkeys `S` (Shut Down), `R` (Restart), `U` (Sleep), `L` (Lock), `E` (Log Out); `Esc` to cancel.
- **Direct CLI Execution**: Supports instant action execution via `miqupower --shutdown` or `miqupower --reboot`.

---

## ⚙️ Configuration

Generate default configuration:
```bash
miqupower --init-config
```
Config location: `~/.config/miqupower/miqupower.conf`.

---

## 🔨 Build & Run

```bash
# In monorepo root:
cmake --build build --target miqupower

# Standalone:
./make.sh
```
