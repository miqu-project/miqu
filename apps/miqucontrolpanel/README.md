# miqucontrolpanel

> **Centralized Desktop Settings, Network Security & Privacy Manager**

`miqucontrolpanel` is a modern Miller-columns settings manager for the Miqu desktop environment, built with `miqutoolkit`. It consolidates hardware telemetry, network security, and appearance customization into an intuitive, multi-column interface.

Website & Documentation: [**miqu-project.github.io**](https://miqu-project.github.io)

---

## 🔍 What is What

- **Network & Connectivity**: Manage Wi-Fi connections, Ethernet, Airplane Mode, and Bluetooth pairings.
- **WireGuard VPN**: Manage, inspect, and toggle WireGuard tunnels with live telemetry and endpoint handshake stats.
- **Private DNS (DoT)**: RFC 7858 DNS over TLS encryption with one-click presets (Cloudflare, Quad9, AdGuard, Mullvad) or custom DoT hostnames.
- **System Firewall**: Manage UFW/nftables packet filtering policies, port profiles (Home, Work, Public), and application exceptions.
- **Display & Monitors**: Multi-monitor layout, resolution, refresh rate, and scaling controls.
- **Theme & Appearance**: Real-time palette previews, wallpaper management, and design token inspection.

---

## ⚙️ Configuration

Generate user configuration:
```bash
miqucontrolpanel --init-config
```
Config location: `~/.config/miqucontrolpanel/miqucontrolpanel.conf`.

---

## 🔨 Build & Run

```bash
# In monorepo root:
cmake --build build --target miqucontrolpanel

# Standalone:
./make.sh
```
