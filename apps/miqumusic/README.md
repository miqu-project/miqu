# miqumusic

> **Fast, Lightweight Native MPD Audio Player & Queue Manager**

`miqumusic` is a native Wayland client for the Music Player Daemon (MPD), built with `miqutoolkit` and `libmpdclient`. It provides smooth local audio playback, queue management, and album art display with zero web/Electron bloat.

Website & Documentation: [**miqu-project.github.io**](https://miqu-project.github.io)

---

## 🔍 What is What

- **MPD Socket Integration**: Direct communication with local or networked Music Player Daemon instances.
- **Playback & Seek Engine**: Responsive seek bar, volume slider, track skip, shuffle, and repeat modes.
- **Queue & Playlist Controls**: Real-time playlist viewer, dynamic queue reordering, and search filtering.
- **Artwork & Metadata**: Embedded album artwork viewer and clean track typography.
- **Dynamic Theming**: Instant color palette updates via `miqutheme`.

---

## ⚙️ Configuration

Generate default configuration:
```bash
miqumusic --init-config
```
Config location: `~/.config/miqumusic/miqumusic.conf`.

---

## 🔨 Build & Run

```bash
# In monorepo root:
cmake --build build --target miqumusic

# Standalone:
./make.sh
```
