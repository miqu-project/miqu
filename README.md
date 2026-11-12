<p align="center">
  <img src="https://raw.githubusercontent.com/miqu-project/.github/main/assets/logo.png" alt="Miqu Project Logo" width="220" />
</p>

<h1 align="center">Miqu Desktop Ecosystem</h1>

<p align="center">
  <b>The unified monorepo for the Miqu Wayland desktop environment, UI toolkit, shell, and native application suite.</b>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Language-C%2B%2B20-blue?style=flat-square" alt="C++20" />
  <img src="https://img.shields.io/badge/Display-Wayland%20Native-orange?style=flat-square" alt="Wayland Native" />
  <img src="https://img.shields.io/badge/Toolkit-miqutoolkit-purple?style=flat-square" alt="miqutoolkit" />
  <img src="https://img.shields.io/badge/Architecture-Monorepo-green?style=flat-square" alt="Monorepo" />
  <img src="https://img.shields.io/badge/Build-CMake%20%2F%20Ninja-red?style=flat-square" alt="Ninja" />
</p>

<p align="center">
  <img src="https://raw.githubusercontent.com/miqu-project/miqu-project.github.io/main/assets/screenshots/sc1.png" alt="Miqu Desktop" width="100%" />
</p>

---

## 🏛️ Overview

**Miqu** is an ambitious, unified desktop environment engineered from the ground up for Wayland on Linux. It eliminates desktop fragmentation by providing a dedicated wlroots-based compositor, a custom C++20 UI framework with instant theme propagation, a modular desktop shell, and cohesive first-party native applications.

All core desktop subsystems are housed in this **unified monorepo**, enabling atomic commits, in-tree build linking, and zero submodule overhead. Full documentation, architectural guides, and IPC references are available on the [**Miqu Project Documentation Portal**](https://miqu-project.github.io/#docs).

---

## 🚀 Quick Start (Build & Install)

### 1. Clone the Monorepo
Clone the entire desktop codebase with a single command (no submodules required):

```bash
git clone https://github.com/miqu-project/miqu.git
cd miqu
```

### 2. Build & Install Everything
Execute the unified monorepo installer to build and install `miqutoolkit`, `miquland`, `shell`, and `apps` in topological dependency order:

```bash
sudo ./install.sh
```

To build locally into a development build directory without installing to `/usr`:
```bash
./install.sh
```

---

## 📦 Monorepo Directory Structure

```
miqu/
├── CMakeLists.txt              # Root CMake orchestrator (in-tree linking)
├── install.sh                  # Master build & install script
├── README.md                   # Ecosystem documentation
├── AGENTS.md                   # Architectural contracts & guidelines
│
├── miquland/                   # Flagship wlroots Wayland compositor & miquctl IPC
├── miqutoolkit/                # Shared C++20 UI framework & Cairo/Pango widgets
│
├── shell/                      # Desktop Shell UI, Daemons & Protocols (11 components)
│   ├── miqubar/                # Wayland status bar, taskbar, system tray & flyouts
│   ├── miqudesk/               # Interactive desktop canvas, icons & widgets
│   ├── miqulauncher/           # Fast fuzzy app launcher & active window switcher
│   ├── miquoverview/           # GPU-accelerated workspace exposé plugin for miquland
│   ├── miqunotify/             # Native notification daemon & slide-out center
│   ├── miqupower/              # Session & power dialog with running app warning
│   ├── miqudm/                 # Wayland-native Display Manager & login greeter
│   ├── miqubg/                 # Independent wallpaper daemon & transitions
│   ├── miqulock/               # Session locker (ext-session-lock-v1)
│   ├── miquidle/               # Idle daemon (ext-idle-notify-v1)
│   └── miqupolkit/             # Native Polkit privilege escalation agent
│
└── apps/                       # Standalone First-Party Applications (6 apps)
    ├── miqucontrolpanel/       # Centralized settings, WireGuard VPN, DNS & privacy
    ├── miqufiles/              # Modern Yazi-style Miller columns file manager
    ├── miqumusic/              # Fast, lightweight native MPD audio player
    ├── miqugallery/            # Hardware-accelerated image & media viewer
    ├── miqutheme/              # Visual appearance & dynamic wallpaper theme manager
    └── miquinfo/               # Real-time hardware telemetry & resource monitor
```

---

## ⚙️ Subsystem Breakdown

| Subsystem | Directory | Technology | Description |
| :--- | :--- | :---: | :--- |
| **Compositor** | [`miquland`](miquland/) | C++20 / wlroots | GPU-accelerated tiling compositor with dynamic layout engines, scenefx blur, and IPC daemon (`miquctl`). |
| **UI Toolkit** | [`miqutoolkit`](miqutoolkit/) | C++20 / Cairo / Pango | Unified Wayland UI library with declarative widgets, fluid animations, and live inotify theme reload. |
| **Desktop Shell** | [`shell/`](shell/) | C++20 / miqutoolkit | Core layer-shell UI components and system daemons (`miqubar`, `miqudesk`, `miqulauncher`, `miquoverview`, `miqunotify`, `miqupower`, `miqudm`, `miqubg`, `miqulock`, `miquidle`, `miqupolkit`). |
| **Desktop Apps** | [`apps/`](apps/) | C++20 / miqutoolkit | Standalone client applications (`miqucontrolpanel`, `miqufiles`, `miqumusic`, `miqugallery`, `miqutheme`, `miquinfo`). |

---

## 🛠️ Developer Workflow

### In-Tree Incremental Builds
Because all components share the root `CMakeLists.txt`, editing `miqutoolkit` and building any downstream application rebuilds both in parallel without requiring `sudo make install`:

```bash
# Configure once
cmake -B build -G Ninja

# Rebuild a specific application instantly
cmake --build build --target miqucontrolpanel

# Run the fresh binary directly
./build/apps/miqucontrolpanel/miqucontrolpanel
```

### Standalone Component Compilation
Each shell component and application retains its own standalone `make.sh` and can still be compiled independently if desired:
```bash
cd apps/miqufiles
./make.sh
```

---

## 📸 Screenshots

![Screenshot 1](https://raw.githubusercontent.com/miqu-project/miqu-project.github.io/main/assets/screenshots/sc1.png)

![Screenshot 2](https://raw.githubusercontent.com/miqu-project/miqu-project.github.io/main/assets/screenshots/sc2.png)

![Screenshot 3](https://raw.githubusercontent.com/miqu-project/miqu-project.github.io/main/assets/screenshots/sc3.png)

![Screenshot 4](https://raw.githubusercontent.com/miqu-project/miqu-project.github.io/main/assets/screenshots/sc4.png)

![Screenshot 5](https://raw.githubusercontent.com/miqu-project/miqu-project.github.io/main/assets/screenshots/sc5.png)

---

## 📜 Architecture & Standards

All contributors and agents working across the Miqu ecosystem adhere strictly to the architectural contracts, configuration management doctrine, and coding craftsmanship standards specified in [**`AGENTS.md`**](AGENTS.md).
