# miquphotos

> **Fast Miller Columns Photo Manager & Gallery for Wayland**

`miquphotos` is a lightweight, responsive photo manager built with `miqutoolkit`. It organizes your photo collections across folders into a flat album browser using the unified Miller columns layout.

---

## 🔍 Features

- **Flat Album Discovery**: Automatically finds all directories containing photos in your home folder.
- **Miller Columns Flow**:
  - **Left Pillar**: Photos hero brand card (or album list when drilled).
  - **Middle Pillar**: All folders containing images with photo counts.
  - **Right Pillar**: Instant thumbnail preview of the highlighted album's photos.
- **Photo Inspection**: View image previews with file size, modified date, dimensions, and quick viewing actions.
- **Keyboard Navigation**:
  - `↑` / `↓` (`k` / `j`): Navigate active list.
  - `Enter` / `→` (`l`): Open album or preview photo.
  - `←` / `Backspace` (`h`): Navigate back to album list.
  - `Ctrl+F`: Instant photo/album filter.
  - `F11`: Full-width photo preview.
  - `Esc`: Clear search / back / exit.

---

## ⚙️ Configuration

Generate default user configuration:
```bash
miquphotos --init-config
```
Configuration path: `~/.config/miquphotos/miquphotos.conf`

---

## 🔨 Build & Run

```bash
# In app folder:
./make.sh

# System-wide install:
sudo ./make.sh
```
