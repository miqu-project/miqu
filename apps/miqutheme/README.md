# miqutheme

> **Visual Appearance, Palette Extraction & Wallpaper Theme Manager**

`miqutheme` is the appearance manager for the Miqu desktop ecosystem, generating dynamic Material You color schemes from wallpapers and propagating design tokens to running applications in real time.

Website & Documentation: [**miqu-project.github.io**](https://miqu-project.github.io)

---

## 🔍 What is What

- **Material You Palette Generation**: Uses `matugen` to extract harmonious color schemes directly from wallpapers.
- **Wallpaper Gallery**: Visual browser to preview, select, and apply desktop wallpapers via `miqubg`.
- **Live Token Propagation**: Exports active design tokens to `~/.config/miqutheme/current/`, triggering instant inotify reloads across `miqubar`, `miqulauncher`, `miqudesk`, and all other client apps.
- **Light & Dark Modes**: Instant switching between light and dark Material Design 3 palettes.

---

## ⚙️ Configuration & Paths

- System assets & palettes: `/usr/share/miqutheme/`
- User wallpapers & themes: `~/.config/miqutheme/`
- Active exported tokens: `~/.config/miqutheme/current/miqutoolkit.conf`

---

## 🔨 Build & Run

```bash
# In monorepo root:
cmake --build build --target miqutheme

# Standalone:
./make.sh
```
