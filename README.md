<p align="center">
  <img src="https://raw.githubusercontent.com/miqu-project/.github/main/assets/logo.png" alt="Miqu Project Logo" width="200" />
</p>

<h1 align="center">Miqu Desktop Ecosystem</h1>

<p align="center">
  <b>The unified monorepo for the Miqu Wayland desktop environment, UI toolkit, shell, and applications.</b><br>
  Website: <a href="https://miqu-project.github.io"><b>miqu-project.github.io</b></a> • Documentation: <a href="https://miqu-project.github.io/#docs"><b>miqu-project.github.io/#docs</b></a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Language-C%2B%2B20-blue?style=flat-square" alt="C++20" />
  <img src="https://img.shields.io/badge/Display-Wayland%20Native-orange?style=flat-square" alt="Wayland Native" />
  <img src="https://img.shields.io/badge/Toolkit-miqutoolkit-purple?style=flat-square" alt="miqutoolkit" />
  <img src="https://img.shields.io/badge/Architecture-Monorepo-green?style=flat-square" alt="Monorepo" />
  <img src="https://img.shields.io/badge/Build-CMake%20%2F%20Ninja-red?style=flat-square" alt="Ninja" />
</p>

---

### What is Miqu?
Miqu is an independent, cohesive desktop environment engineered from the ground up for Wayland on Linux. It eliminates desktop fragmentation by pairing a custom wlroots-based compositor with a dedicated C++20 UI framework (`miqutoolkit`), a modular layer-shell suite, and native first-party applications.

Full documentation, architecture guides, and interactive visualizers are available on our official website: **[miqu-project.github.io](https://miqu-project.github.io)**.

---

### Quick Start

#### 1. Build & Install Everything
```bash
git clone https://github.com/miqu-project/miqu.git
cd miqu
sudo ./install.sh
```

#### 2. Local Developer Build (Unprivileged)
```bash
./install.sh    # builds all components into build/ without installing to /usr
```

#### 3. Complete Uninstallation
```bash
sudo ./uninstall.sh
```

---

### What is What

| Subsystem | Directory | Description |
| :--- | :--- | :--- |
| **Compositor** | [`miquland/`](miquland/) | Dynamic tiling Wayland compositor built with wlroots, scenefx GPU blur, and `miquctl` IPC. |
| **UI Toolkit** | [`miqutoolkit/`](miqutoolkit/) | Shared C++20 Cairo/Pango UI framework with RAII lifecycle safety and live theme reload. |
| **Desktop Shell** | [`shell/`](shell/) | 11 layer-shell daemons and controls (`miqubar`, `miqudesk`, `miqulauncher`, `miquoverview`, `miqunotify`, `miqupower`, `miqudm`, `miqubg`, `miqulock`, `miquidle`, `miqupolkit`). |
| **Desktop Apps** | [`apps/`](apps/) | 6 standalone windowed applications (`miqucontrolpanel`, `miqufiles`, `miqumusic`, `miqugallery`, `miqutheme`, `miquinfo`). |
| **Mobile Apps** | [`mobile/`](mobile/) | Native Android companion applications (`doctor`, `recitation`) built in Kotlin & Jetpack Compose. |

---

### Monorepo Structure

```
miqu/
├── miquland/           # Wayland compositor & window manager
├── miqutoolkit/        # Shared C++20 UI framework
├── shell/              # Desktop shell daemons & layer-shell UIs (11 components)
├── apps/               # Standalone desktop applications (6 apps)
├── mobile/             # Android companion applications (2 apps)
├── CMakeLists.txt      # Root CMake orchestrator (in-tree linking)
├── install.sh          # Master installer script
├── uninstall.sh        # Complete system uninstaller & purge script
├── AGENTS.md           # Authoritative architectural contracts & guidelines
└── README.md
```

---

<p align="center">
  <sub>For full documentation, screenshots, and roadmap, visit <a href="https://miqu-project.github.io"><b>miqu-project.github.io</b></a>.</sub>
</p>
