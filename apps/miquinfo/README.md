# miquinfo

> **System Telemetry, Hardware Monitor & Resource Dashboard**

`miquinfo` is a lightweight Wayland system monitor built with `miqutoolkit`, displaying real-time hardware diagnostics, compositor telemetry, and resource utilization.

Website & Documentation: [**miqu-project.github.io**](https://miqu-project.github.io)

---

## 🔍 What is What

- **OS & Kernel Diagnostics**: Real-time identification of kernel version, distribution details, uptime, and shell environment.
- **Wayland Compositor Info**: Active compositor protocols, display scaling, and output configuration.
- **Hardware Architecture**: Detailed CPU model and core counts, GPU acceleration status, and PCI devices.
- **Resource Indicators**: Live visual gauges for RAM usage, swap status, and filesystem allocations.
- **Miqu Theming**: Adheres strictly to Miqu design tokens and typography.

---

## ⚙️ Configuration

Generate default configuration:
```bash
miquinfo --init-config
```
Config location: `~/.config/miquinfo/miquinfo.conf`.

---

## 🔨 Build & Run

```bash
# In monorepo root:
cmake --build build --target miquinfo

# Standalone:
./make.sh
```
