<p align="center">
  <img src="https://raw.githubusercontent.com/miqu-project/.github/main/assets/logo.png" alt="Miqu Project Logo" width="220" />
</p>

<h1 align="center">Miqu Desktop Ecosystem</h1>

<p align="center">
  <b>The official umbrella repository for the Miqu Wayland desktop environment and application suite.</b>
</p>

<p align="center">
  <a href="https://github.com/miqu-project/miquland"><img src="https://img.shields.io/badge/Compositor-miquland-blue?style=flat-square" alt="miquland" /></a>
  <a href="https://github.com/miqu-project/miqutoolkit"><img src="https://img.shields.io/badge/Toolkit-miqutoolkit-purple?style=flat-square" alt="miqutoolkit" /></a>
  <a href="https://github.com/miqu-project/miqu-linux-apps"><img src="https://img.shields.io/badge/Apps-miqu--linux--apps-green?style=flat-square" alt="miqu-linux-apps" /></a>
  <a href="https://github.com/miqu-project/miqu-android"><img src="https://img.shields.io/badge/Mobile-miqu--android-orange?style=flat-square" alt="miqu-android" /></a>
</p>

<p align="center">
  <img src="https://raw.githubusercontent.com/miqu-project/miqu-project.github.io/main/assets/screenshots/sc1.png" alt="Miqu Desktop" width="100%" />
</p>

---

## 🏛️ Overview

**Miqu** is an ambitious, unified desktop environment engineered from the ground up for Wayland on Linux. It eliminates desktop fragmentation by providing a dedicated wlroots-based compositor, a custom C++ UI toolkit with instant theme propagation, a cohesive first-party native desktop application suite, and companion mobile applications.

This repository serves as the **meta-repository / umbrella distribution** for the entire Miqu ecosystem, synchronizing all primary subsystems via Git submodules.

---

## 🚀 Quick Start (Build & Install)

### 1. Clone the Ecosystem
Clone the umbrella repository along with all components:

```bash
git clone --recurse-submodules https://github.com/miqu-project/miqu.git
cd miqu
```

> [!TIP]
> If you cloned without `--recurse-submodules`, you can initialize the submodules at any time by running:
> ```bash
> git submodule update --init --recursive
> ```
> `install_all.sh` will also auto-detect and initialize submodules automatically if they are missing.

### 2. Build & Install Everything
Execute the ecosystem orchestrator to build and install `miquland`, `miqutoolkit`, and all `miqu-linux-apps` in topological dependency order:

```bash
sudo ./install_all.sh
```

To build locally into development build directories without installing to `/usr`:
```bash
./install_all.sh
```

---

## 📦 Repository Structure

The Miqu ecosystem codebase is structured across the following repositories:

| Repository | Role | Technology | Description |
| :--- | :--- | :---: | :--- |
| [**`miquland`**](https://github.com/miqu-project/miquland) | **Compositor & Window Manager** | C++20 / wlroots | GPU-accelerated tiling compositor with dynamic layout engines, overview plugin, and IPC daemon (`miquctl`). |
| [**`miqutoolkit`**](https://github.com/miqu-project/miqutoolkit) | **Shared UI Toolkit** | C++20 / Cairo / Pango | Unified Wayland application toolkit providing declarative widgets, fluid animations, and live theme propagation. |
| [**`miqu-linux-apps`**](https://github.com/miqu-project/miqu-linux-apps) | **Native Desktop Suite** | C++20 / miqutoolkit | Cohesive first-party desktop applications (`miqudesk`, `miqubar`, `miqusecure`, `miqumusic`, `miqulauncher`, `miqunotify`, `miqugallery`, `miqudm`, `miqubg`, `miqulock`, `miquidle`, `miqupolkit`, etc.). |
| [**`miqu-android`**](https://github.com/miqu-project/miqu-android) | **Mobile Applications** | Kotlin / Jetpack Compose | Native Android apps and mobile companions designed to complement the Miqu environment. |
| [**`miqu-project.github.io`**](https://github.com/miqu-project/miqu-project.github.io) | **Documentation & Web** | HTML / CSS / JS | Official landing website and documentation portal. |
| [**`.github`**](https://github.com/miqu-project/.github) | **Organization Profile & Assets** | Markdown / Assets | GitHub organization profile landing page (`profile/README.md`) and ecosystem assets. |

---

## 🛠️ Developer Submodule Workflow

### Pulling the Latest Changes from All Repositories
To fetch and update all submodules to the latest commits on their tracking branches:
```bash
git submodule update --remote --merge
```

### Working on Individual Components
Each subfolder (`miquland`, `miqutoolkit`, `miqu-linux-apps`) remains a complete, independent Git repository:
```bash
cd miquland
git checkout main
# make code edits...
git commit -m "feat(core): example improvement"
git push origin main
```

To lock in the updated component versions in the parent meta-repository:
```bash
cd ..
git add miquland
git commit -m "chore(deps): update miquland submodule pointer"
git push origin main
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

All contributors and agents working across the Miqu ecosystem adhere strictly to the architectural standards and configuration rules specified in [**`AGENTS.md`**](AGENTS.md).
