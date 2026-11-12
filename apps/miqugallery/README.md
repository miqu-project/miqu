# miqugallery

> **High-Performance Image Viewer & Media Gallery for Wayland**

`miqugallery` is a fast media viewer built with `miqutoolkit`, optimized for smooth panning, fluid zoom, and intuitive album browsing across high-resolution image formats.

Website & Documentation: [**miqu-project.github.io**](https://miqu-project.github.io)

---

## 🔍 What is What

- **Format Support**: Full viewing support for PNG, JPEG, WebP, AVIF, HEIC, TIFF, BMP, and SVG.
- **Pan & Zoom Engine**: Smooth trackpad/mouse panning, gesture zooming, fit-to-window, and 1:1 pixel inspection.
- **Filmstrip Navigation**: Dynamic thumbnail preview bar and quick folder navigation.
- **EXIF Inspector**: Metadata panel displaying resolution, camera exposure values, and timestamps.
- **Lightweight Architecture**: Cairo-accelerated rendering with low memory overhead.

---

## ⚙️ Configuration

Generate default configuration:
```bash
miqugallery --init-config
```
Config location: `~/.config/miqugallery/miqugallery.conf`.

---

## 🔨 Build & Run

```bash
# In monorepo root:
cmake --build build --target miqugallery

# Standalone:
./make.sh
```
