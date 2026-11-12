# miqudesk

> **Interactive Desktop Canvas, Icons & Widgets Layer for Wayland**

`miqudesk` brings familiar desktop canvas ergonomics—desktop shortcuts, app launchers, and customizable widgets—to modern tiling Wayland environments, built natively with `miqutoolkit`.

Website & Documentation: [**miqu-project.github.io**](https://miqu-project.github.io)

---

## 🔍 What is What

- **Desktop Icon Grid**: Scans `~/Desktop/*.desktop` files automatically; supports drag-and-drop repositioning with persistent coordinate memory.
- **Custom Widget Engine**: Embeds lightweight system status cards and information widgets directly on the desktop canvas.
- **Layer Shell Background**: Maps directly to the Wayland background layer (`wlr-layer-shell`) with zero window manager interference.
- **Dynamic Theming**: Synchronizes typography, colors, and icon styles instantly with `miqutheme`.

---

## ⚙️ Configuration

Generate default configuration:
```bash
miqudesk --init-config
```
Config location: `~/.config/miqudesk/miqudesk.conf`.  
Add desktop shortcuts by placing `.desktop` files into `~/Desktop/`.

---

## 🔨 Build & Run

```bash
# In monorepo root:
cmake --build build --target miqudesk

# Standalone:
./make.sh
```
