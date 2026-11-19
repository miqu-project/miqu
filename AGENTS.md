# AGENTS.md — Miqu Ecosystem Guidelines, Standards & Roadmap

> This document is the authoritative single source of truth for architectural contracts, configuration management doctrine, craftsmanship standards, build protocols, and active roadmaps across the Miqu desktop ecosystem.

## Author's Goal
> A fully functional Desktop Environment is the goal, but we are slowly making progress.

## Platform Base
- Currently we are leveraging an **Arch Linux** base.
- Other Linux distribution bases have not been tested yet.

---

## Configuration Management Golden Rules

Configuration management is the most critical architectural contract across the Miqu desktop ecosystem. It enforces absolute uniformity, predictable user customization, strict lifecycle safety, and zero hidden C++ magic.

---

### The Uniform Two-Tier Ecosystem Architecture

Every component across the entire Miqu ecosystem (`miquland`, `miqutoolkit`, and all downstream applications like `miqubar`, `miqudesk`, `miqumusic`, `miqulauncher`, etc.) strictly adheres to the **exact same Two-Tier Single-File Authority Model**:

```
┌────────────────────────────────────────────────────────┐
│ Tier 1: System Baseline (/usr/share/<app>/<app>.conf)  │
│  • Fully functional out of the box                     │
│  • Loaded ONLY if user config does not exist           │
└──────────────────────────┬─────────────────────────────┘
                           │ (when user creates config)
                           ▼
┌────────────────────────────────────────────────────────┐
│ Tier 2: User Standalone (~/.config/<app>/<app>.conf)   │
│  • Generated on demand via '<app> --init-config'       │
│  • Safe timestamped archiving of existing config       │
│  • Loaded EXCLUSIVELY once it exists                   │
│  • Explicit 'source =' for dynamic themes/modules      │
└────────────────────────────────────────────────────────┘
```

#### 1. Zero-Config Out of the Box (Tier 1: System Baseline)
- On a fresh system where `~/.config/<component>/<component>.conf` does not exist, the component executes directly from the authoritative system baseline at `/usr/share/<component>/<component>.conf`.
- No configuration files are **ever** auto-seeded or written to `~/.config/` automatically on launch. The entire desktop works immediately without polluting the user's home directory.

#### 2. On-Demand Initialization with Safe Timestamped Archiving
- A user configuration is created strictly on-demand when the user invokes `<component> --init-config`.
- **Safe Archiving**: If `~/.config/<component>/<component>.conf` already exists when `--init-config` is run:
  - It is **never** silently overwritten.
  - It is safely archived to `~/.config/<component>/<component>.conf.bak.<YYYYMMDD-HHMMSS>`.
  - A fresh, fully documented template is then written to `~/.config/<component>/<component>.conf`.
  - This prevents accidental data loss even if `--init-config` is invoked repeatedly.

#### 3. Single-File Standalone Authority (Tier 2: User Configuration) & Token Inheritance
- **Intra-Component Authority (No System Baseline Layering)**: Once `~/.config/<component>/<component>.conf` exists, the component loads **only** that user file for its application settings. Tier 2 completely replaces Tier 1; there is no implicit merging between `/usr/share/<component>/<component>.conf` and `~/.config/<component>/<component>.conf`.
- **Ecosystem Token Inheritance**: Downstream applications (`miqufiles`, `miqumusic`, `miqubar`, etc.) automatically inherit shared design tokens (colors, fonts, metrics, icon themes) from `miqutoolkit.conf` via `Config::get()`. Applications do not need to duplicate toolkit color palettes. Any styling or color tokens explicitly declared in the application's `.conf` file cleanly overlay on top of the `miqutoolkit` baseline.

#### 4. Decoupled Dynamic Theming via Explicit `source`
- `miqutoolkit` C++ code contains **zero hardcoded internal connections to `miqusettings` or theming backends**.
- Instead, dynamic theming is completely decoupled and transparent:
  - Configuration files use the standard `source = ~/.config/miqusettings/current/miqutoolkit.conf` directive.
  - In `miqutoolkit.conf`, users who want dynamic wallpaper/palette changes from `miqusettings` explicitly include the `source` line.
  - Downstream applications automatically inherit these active palette tokens through `miqutoolkit`. Users can also specify `source = /path/to/custom-theme.conf` in any application config to decouple that specific app's colors.
  - Users who want completely static, pinned colors simply comment out or remove the `source` line and define their colors directly.
- Sourced files and directories are automatically tracked and watched via inotify for live dynamic reload.

---

### Canonical Component Lifecycle Sequence
All ecosystem binaries implement the uniform configuration lifecycle:
```cpp
// 1. Resolve configuration path (Tier 2 user authority > Tier 1 system baseline)
std::string user_conf = FsUtils::get_user_config_file(app_name);
std::string target_conf = fs::exists(user_conf) ? user_conf : FsUtils::get_system_config_file(app_name);

// 2. Load exclusive configuration file (processes internal 'source =' directives and overlays onto miqutoolkit)
Config::get()->load_from_file(target_conf);

// 3. Initialize AppEngine
auto engine = AppEngine::create();

// 4. Register inotify watcher on primary config and all sourced files for live reload
engine->setup_config_watcher();

// 5. Connect live theme changes (re-reads Config::get() tokens and updates active views)
engine->add_theme_change_listener([&app]() {
    app.sync_theme(); // or BarConfig::get().sync_defaults_from_toolkit()
});
```

---

### Inotify & Dynamic Live Reload Architectural Doctrine

All inotify config watchers across the entire ecosystem (`miquland`, `miqutoolkit`, and standalone utilities like `miquidle` and `miquoverview`) must strictly adhere to the five golden rules of inotify hygiene:

1. **`IN_IGNORED` is Strictly an Acknowledgment Event, Never a Content Change**:
   - `IN_IGNORED` is emitted by the Linux kernel when a watch is removed (`inotify_rm_watch`) or a monitored entity is unmounted.
   - Event handlers must explicitly intercept `event->mask & IN_IGNORED` to invalidate internal descriptors (`wd = -1`) and `continue` immediately.
   - **Never** mark `should_reload = true` or trigger a reload on `IN_IGNORED`. Treating watch teardowns as file modifications creates infinite reload feedback loops.

2. **Strict Write-Mask Filtering & Directory-Self Protection**:
   - Live reloads must only respond to genuine write and creation events: `IN_MODIFY`, `IN_CLOSE_WRITE`, `IN_MOVED_TO`, and `IN_CREATE`.
   - In directory watchers, events targeting the directory itself (`event->len == 0`, such as metadata adjustments or watch invalidations) must **never** trigger reloads. File reloads require `event->len > 0` matching the target filename or `.conf` extension.

3. **Inotify Watch Architectures (Directory Watching vs Inode Authority)**:
   - **`miqutoolkit` Apps (`AppEngine::setup_config_watcher()`)**: Uses directory-level watching (`dir_to_files[parent_dir]`). Watching parent directories with `IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE` ensures that atomic editor writes (`.tmp` -> `rename`) and symlink swapping (e.g. `current -> scheme`) are seamlessly captured without inode churn.
   - **`miquland` Compositor Core**: Uses dedicated single inode authority (`<app>.conf` tracked strictly through its dedicated primary watch descriptor, while dynamically included files are tracked in the sourced collection) to prevent shared descriptor invalidation.

4. **Zero Watch Descriptor Churn (Idempotent Watch Syncing)**:
   - On dynamic reload, components must diff the list of active sourced files against previous registrations (`new_sourced == m_watched_sourced_files`).
   - If the list of loaded sourced files has not changed, retain active kernel watches as-is. Avoid destroying and re-adding identical inotify watches on every reload cycle.

5. **Debounce Discipline & Synchronous IPC Coordination**:
   - Asynchronous filesystem modifications must be debounced (150–250ms) via a one-shot event-loop timer to coalesce rapid multi-chunk writes and atomic editor renames (`.tmp` -> `rename`).
   - Explicit IPC reload commands (such as `miquctl reload` or Unix domain socket signals) must immediately disarm any pending debounce timer (`wl_event_source_timer_update(timer, 0)`) to ensure instant, single-pass, synchronous execution without redundant delayed secondaries.

---

### Zero Source-Code Appearance Defaults & Dynamic Sync
- Shared appearance tokens (colors, fonts, metrics, `icon_theme`) must **NEVER** have fallback magic literals in C++ widget styling methods.
- All downstream applications must implement a theme synchronization method (`sync_theme()` in `miqusettings`, `miqufiles`, `miqumusic`; or `sync_defaults_from_toolkit()` in `miqubar`) that queries `Config::get()`.
- This sync must be executed unconditionally on application startup and dynamically whenever an `engine->add_theme_change_listener()` callback is triggered.

---

## Component Responsibilities

### Core & Toolkit
- **`miquland`**: Wayland window manager and compositor (wlroots-based), responsible for window management, tiling and floating layouts, input handling, Wayland protocol implementations (including layer-shell and session-lock), and the IPC/plugin architecture.
- **`miqutoolkit`**: Shared C++20 UI framework and application development toolkit (Cairo/Pango/Wayland abstraction), providing common widgets, design token theming, layout containers, event loop integration, and instant theme propagation across all applications.

### Desktop Shell & Daemons (`shell/`)
- **`miqubar`**: Wayland status bar and taskbar, featuring workspace indicators, window lists, system tray, battery/network telemetry modules, and interactive flyout menus.
- **`miqudesk`**: Interactive desktop canvas and widget layer that brings familiar desktop ergonomics (desktop icons, shortcuts, widgets) to a modern tiling Wayland environment.
- **`miqulauncher`**: Fast application launcher, fuzzy dmenu alternative, workspace navigator, and active window switcher.
- **`miquoverview`**: GPU-accelerated live workspace expose and overview plugin/utility for window management in `miquland`.
- **`miqunotify`**: Native desktop notification daemon and slide-out notification history center.
- **`miqupower`**: Desktop Environment power and session management dialog with running application detection, graceful teardown visual feedback, and systemd-logind / miquland IPC integration.
- **`miqudm`**: Wayland-native Display Manager and login greeter with PAM and logind session management.
- **`miqubg`**: Independent wallpaper daemon responsible for rendering, scaling, and transitioning desktop background images.
- **`miqulock`**: Session locking utility implementing the Wayland `ext-session-lock-v1` protocol.
- **`miquidle`**: Idle management daemon monitoring user activity and power events via the Wayland `ext-idle-notify-v1` protocol.
- **`miqupolkit`**: Native graphical Polkit authentication agent handling privilege escalation prompts.

### Standalone Desktop Applications (`apps/`)
- **`miqusettings`**: Modern Miller-columns centralized desktop settings manager, visual theme & appearance manager (color schemes, wallpapers, fonts, dark mode, electron/IDE sync, GTK styles), network security, WireGuard VPN, Private DNS (DoT), privacy center, and system configuration manager.
- **`miqufiles`**: Modern Yazi-style Miller columns file manager with contextual live preview (child directories, images, code/text with monospace formatting, binary file inspect) and instant keyboard/mouse navigation.
- **`miqumusic`**: Fast, lightweight native MPD (Music Player Daemon) client and audio playback interface.

### Mobile Applications (`mobile/`)
- **`doctor`**: Healthcare & medical diagnostic companion application (Android / Jetpack Compose).
- **`recitation`**: Audio recitation, study, and media companion application (Android / Jetpack Compose).

---

## Active & Future Roadmap (TODO List)


1. **Plugin System v2.0 **:
   -
2. **Next Utilities & Applications**:
   - `miqushot` (screenshot utility)
   - `miqusunset` (gamma/blue light control)
   - `miqusettings` (completed: modern Miller-columns centralized desktop settings manager)
   - Package manager front-end
   
4. **Privacy & Security Extensions in `miqusettings`**: OpenSnitch frontend, 1-click Tor enabler, browser privacy advisor, security auditor, LUKS advice, firewall (ufw/nftables) selection, system restore points (Timeshift/Snapper OS snapshots).
7. **Touch Screen Gestures**: Multi-touch gesture recognition (swipes, pinch-to-zoom, edge gestures) for touchscreen devices.
8. **Virtual Machine Front-End**: Front-end utility to try out Linux distributions.
9. **Firefox Live Preview Fix in `miquoverview`**: Resolve subsurfaces / popup preview bounding in overview cards.
10. **Floating Window Improvements**: Review move/resize mechanics and config window rules.
11. **Window Border Shading & Gradient Colors (Ecosystem Phase 2 - Client Decorations)**: Extend gradient colors, multi-stop borders, and border shading to `miqutoolkit` views (`ViewGroup`, `CardView`) and client decorations.
12. **`miqutoolkit` Internal Animation Framework Implementation**: Implement a first-class, frame-driven animation engine natively inside `miqutoolkit` (supporting configurable easing curves, spring dynamics, opacity fades, geometry interpolations, and hover/focus transitions) to provide smooth, high-framerate UI animations across all downstream applications.
13. **`miqubg` Direct In-Place Surface Updating & Internal Animation**: `miqubg` has eliminated the redundant secondary layer surface creation and detached 380ms sleep thread in favor of clean, direct in-place updates on persistent background surfaces. Future work: evaluate optional internal cross-fading directly inside `miqutoolkit::ImageView` on the persistent surface.
14. **Configuration Format Modernization (Lua / Advanced Scriptable Formats)**: Evaluate transitioning or expanding configuration formats across `miquland` and ecosystem components from custom key-value parsers to Lua (e.g. LuaJIT / Sol2) or other expressive configuration formats (enabling programmable rules, dynamic event callbacks, math expressions, and modular config architectures).
15. **Add separate `miqupluginmanager`**: Dedicated manager and utility to discover, install, update, enable/disable, and configure `miquland` plugins and extensions.
16. **Asset Layering & User Customization in `miqusettings`**: Theming and personalization are now fully unified inside `miqusettings` (loading schemes, wallpapers, and templates from system paths like `/usr/share/miqusettings/` or user paths in `~/.config/miqusettings/`). Ensure robust fallback layering and support user-defined custom schemes and wallpapers without polluting home config on startup.
17. **Gamma Control in `miqubar`**: Implement gamma / blue light control module and flyout slider in `miqubar` (integrating directly with native `miquland` IPC via `miquctl gamma`).

18. **Special / Scratchpad Workspace Support in `miquland`**: Implement special workspace (scratchpad) functionality in `miquland` (toggleable overlay workspace, dispatching windows to/from special workspace, keybindings, and IPC controls).

19. **Fix Idle Inhibitor Not Being Responded**: Fix the idle inhibitor not being responded to when set to "keep awake" from apps like Antigravity (ensure idle inhibitor protocol requests and surface/portal inhibition properly prevent idle transitions and screen sleep in `miquland` and `miquidle`).

20. **Fix Kitty Artifact on Launch with CSD**: Fix kitty artifact on launch which goes away on interaction when CSD is turned on (investigate initial configure geometry, frame sizing, or buffer commit synchronization when negotiating client-side decorations in `miquland`).



## Core Coding Craftsmanship Standards

### 1. Strict Layer Boundaries & Toolkit Sovereignty
- **Never bypass `miqutoolkit` in downstream apps**: Downstream applications must never interact directly with raw Cairo, Pango, or Wayland surface protocols when `miqutoolkit` provides (or should provide) the abstraction.
- **Push reusable controls into `miqutoolkit`**: If an application requires a UI widget (e.g. seekbars, dropdowns, cards, breadcrumbs), implement it inside `miqutoolkit` rather than authoring ad-hoc drawing routines in the application.
- **Permission required**: Seek user permission before editing `miqutoolkit` when developing downstream apps.
- **Full-stack evolution**: We are free to modify any layer of the stack—including the compositor `miquland`—if a protocol, hook, or capability is missing.

### 2. Pragmatic Modularization & Clean Craftsmanship Standards
- **Flexible & Domain-Focused Class Sizing**: Avoid dogmatic or arbitrary line-count limits on classes. Cohesive classes (such as `Window`, `MillerView`, complex custom views, or core engines) are permitted to grow as large as naturally required by their domain. Prioritize self-contained, cohesive implementations over excessive fragmentation into artificial micro-classes that introduce needless indirection and lifecycle overhead. Only split a class when it clearly crosses distinct architectural domains or conflates unrelated responsibilities (e.g. separating MPD network protocol handling from Cairo UI rendering).
- **Readable, Focused Functions**: Keep functions focused on a clear logical task. While ~40–60 lines is a healthy rule of thumb for general business logic, longer functions are entirely acceptable for declarative layout builders, event dispatch switches, rendering routines, and comprehensive state transitions where keeping the sequence contiguous preserves clarity and flow. Decompose heavy sub-calculations, input validation, and reusable helpers when doing so improves readability.
- **Deep Modules with Narrow Interfaces**: Implement rich, self-sufficient internal capability behind clean, minimal, and self-documenting public APIs.

### 3. Lifecycle & Pointer Safety (Strict RAII)
- **No Zombie States or Silent Swallowing**: Check all system call returns, Wayland protocol calls, and toolkit APIs. Never swallow exceptions or leave half-initialized objects.
- **Pointer & Lifecycle Safety**: Never hold raw pointers to dynamic entities (`View*`, `Workspace*`, surfaces) without destroy listeners or validity guards against unexpected destruction (crashes, SIGKILL, dynamic pruning).
- **Deterministic Cleanup (Strict RAII)**: Every scene node, buffer, timer, listener, and memory allocation must have a verified destruction path. Dynamic reload/unload (`dlclose`) must cleanly unbind everything without dangling references.
- **Scope-Bound RAII**: Immediately wrap raw allocations, file descriptors (`int fd`), Cairo contexts (`cairo_t*`), and Pango layouts in `std::unique_ptr` with custom deleters or RAII guards. Never rely on manual cleanup before early error returns.

### 4. Event-Driven Confinement & Thread Safety
- **Zero Sleep-Based Synchronization**: Never use `sleep()` or `usleep()` to wait for state transitions or race conditions. Always use Wayland protocol callbacks, frame callbacks, event listeners, or event-loop idle handlers.
- **Main-Thread UI Confinement**: All Wayland protocols, Cairo/Pango rendering, and widget hierarchy mutations must happen strictly on the main event-loop thread.
- **Worker Thread Isolation**: Background threads (MPD, telemetry, sockets, async I/O) must communicate with the UI thread via `eventfd`, pipes, or event-loop idle dispatches—never touching widgets directly.
- **Safe Container Iteration**: Never mutate a container while iterating over it. Always work on a copy or use deferred removal queues. Guard signal emissions against re-entrancy.

### 5. Strict DRY (Don't Repeat Yourself) & Zero Copy-Paste Duplication
- If a logic pattern, event dispatch loop, or geometry calculation repeats more than once, immediately extract it into a parameterized helper function.
- Never duplicate 30–50 line blocks across files with only trivial variable substitutions.

### 6. Reuse Existing Code & Convention-First Architecture
- **Never reinvent the wheel**: Before authoring a helper, math calculation, geometry converter, or string utility, inspect the existing codebase.
- **Reuse Existing Classes & Modules**: Check for existing domain classes, managers, or data structures across our source code before authoring parallel abstractions.
- **Refactor While Reusing**: If an existing class or helper to be reused violates craftsmanship standards, contains legacy anti-patterns, or has narrow unmodular interfaces, refactor it to meet the standard rather than duplicating it or building on top of technical debt.
- **Respect Proven Platform Conventions**: Default strictly to industry-standard platform contracts, protocols, and proven ergonomics unless explicitly designing a novel innovation.
- **Mathematical & Structural Rigor**: Ground systems in established structural, mathematical, and behavioral conventions to prevent idiosyncratic, bespoke maintenance traps.
- **Generalize existing helpers**: If an existing function provides 90% of what is needed, extend or generalize it cleanly rather than writing duplicate private variants.

### 7. Accurate Naming & Renaming Hygiene
- Names must accurately describe what the code does. Avoid vague identifiers (`temp`, `data2`, `helper`, `process`).
- If refactoring alters a method's scope or responsibility, immediately rename the method and update all call sites.

### 8. Zero Tolerance for Dead or Abandoned Code
- Immediately excise obsolete functions, unused variables, abandoned header declarations, and commented-out code blocks. Never leave "zombie code" behind.

### 9. Ripple-Effect & Blast-Radius Awareness (Zero Myopic Fixes)
- **System-Wide Impact Tracing**: Before modifying any shared structure, hook, or method signature, proactively trace its potential ripple effects across the entire ecosystem.
- **Proactive Ecosystem Impact Tracing**: Ensure localized enhancements never compromise cross-layer stability, contracts, or dependent components.
- **Zero Band-Aid Edits**: Never make isolated edits that solve an immediate symptom while breaking distant subsystems or introducing regressions.
- **Verify All Invariants & Call Sites**: Inspect all dependent call sites and downstream components to guarantee end-to-end architectural integrity.

### 10. Radical Candor, Technical Feasibility & System Primitives
- **Learn to Say "I Could Not Find", "Cannot Do", or "Sorry"**: Never fabricate non-existent APIs, hallucinate symbols, or invent broken workarounds.
- **Respect Technical Feasibility**: Respect what is technically feasible within the authoritative cost models and physical boundaries of the underlying platform stack.
- **Grounding in System Primitives**: Never attempt to circumvent hardware or protocol realities with pseudo-solutions; isolate global pipeline costs from local entity parameters.
- **Upfront Protocol Reality Checks**: State hard protocol or low-level limitations upfront (e.g. stock wlroots surface clipping limitations without custom fragment shaders or scenefx).
- **Zero Pseudo-Solutions**: If a requirement is unfeasible or risks stability, state the limitations honestly and propose genuine architectural prerequisites.

### 11. Autonomous End-to-End Ownership (Anti-Half-Ass Implementation)
- **No Superficial Patches or Partial MVPs**: Never stop at a superficial patch or partial MVP; autonomously audit the full lifecycle, failure modes, and edge cases.
- **Complete Engineering Contracts**: Deliver complete, professional solutions with verified resting states, eliminating the need for piecemeal user prompting or supervisory hand-holding.

### 12. Full-Stack Ecosystem Sovereignty & Zero Defensive Fallbacks
- **End-to-End Stack Control**: We design, build, and maintain the entire Miqu ecosystem (`miquland`, `miqutoolkit`, and all native apps).
- **No Speculative Fallbacks**: Never introduce defensive fallback branches, heuristic guesswork, or redundant fallback mechanisms for behaviors that we natively control and guarantee across our own stack.
- **Single Authoritative Pipeline**: Design one clean, protocol-driven contract across layers. Implement missing capabilities cleanly at their authoritative source layer.

### 13. Interface-First Design, Symmetry & Cognitive Ergonomics
- **Contract Precedes Implementation**: Always design and validate public class interfaces, method signatures, data structures, and lifecycle contracts before writing implementation logic.
- **Architectural Symmetry & Parity**: Maintain strict feature parity, structural symmetry, and predictable mental models across parallel subsystems and configuration domains.
- **Cognitive Ergonomics & Zero Hidden Magic**: Design clean, intuitive, and self-documenting interfaces with zero hidden background magic, undocumented assumptions, or arbitrary quirks.

### 14. Opportunistic Refactoring
- If non-conforming or unmodular code is spotted during analysis of an unrelated task, offer to refactor that part.

### 15. Zero Idle CPU Usage & Frame Loop Hygiene (Strict Event-Driven Inactivity)
- **Absolute 0% Idle CPU in `miquland` & Ecosystem**: The compositor (`miquland`) and all desktop applications must sleep completely in the Wayland event loop when there are no active input events, damaged scene buffers, or running animations. At steady-state idle, CPU utilization must be strictly 0.0%.
- **Strict Event-Driven Dormancy**: Enforce absolute event-driven dormancy at steady-state idle—never permit continuous polling, unthrottled ticks, or redundant surface commits.
- **Zero Polling & Tick Loops**: Never use continuous timers, tick loops, periodic wakeups, or unthrottled frame callbacks when the UI is static. Frame scheduling must be strictly damage-driven (`wlr_output_schedule_frame` invoked only when actual damage occurs).
- **Instant Animation Timer Teardown**: Any frame callbacks, animators, and timer event sources must be immediately disarmed and destroyed the exact moment an animation completes; they must never remain active or spin in the event loop.
- **Buffer Commit Discipline**: Never trigger output frame repaints or damage tracking on redundant client surface commits if buffer contents, subsurface states, or view positions have not changed.
- **Inotify & Watcher Dormancy**: Inotify event loops and config watchers must settle completely into dormancy after a single debounced reload. Never allow descriptor invalidation events (`IN_IGNORED`) or teardown routines to re-arm debounce timers or spin the event loop in background reload loops.

### 16. Dual-Input UX Parity — Keyboard Enthusiasts & Mouse/Touch Users
Every application built on `miqutoolkit` must be equally effortless for both keyboard-driven and pointer-driven users. Neither input mode is an afterthought.

- **Keyboard-First Spatial Navigation**: All multi-pane applications (Miller columns layout) implement full four-directional spatial navigation:
  - `↑` / `↓` — move selection within the focused pane.
  - `→` / `Enter` — dive into / expand (move focus to the next pane to the right, or open the item).
  - `←` / `Backspace` — ascend / collapse (move focus back to the left pane).
  - Focus ownership is always unambiguous — exactly one pane is active at any time and visually indicates it.
- **Yazi-Style Pane Interplay**: The right pane is a live contextual preview that updates instantly as the selection changes in the middle pane — no extra keypress required. This is a core UX contract, not an optional enhancement.
- **Mouse & Touch Parity**: A single click/tap on any item in any pane transfers focus to that pane and selects the item — identical in outcome to the keyboard path. There must be zero features accessible only by keyboard and zero features accessible only by pointer.
- **Delivered Through `miqutoolkit`, Not Per-App**: Navigation focus management, pane routing, and input dispatch for Miller columns layouts live inside `miqutoolkit` (e.g. `MillerView`, `SelectionModel`). Applications wire up callbacks and content — they never re-implement spatial routing logic themselves.
- **New Apps Must Inherit This**: Any new application that presents a list, tree, or hierarchical content structure must adopt the Miller columns + dual-input pattern from `miqutoolkit` from day one — not bolt it on later.

---

## Build System & Installation Standards (`CMakeLists.txt` & `install.sh`)

The ecosystem uses a **unified monorepo build** orchestrated by a single root `install.sh`. All components — `miqutoolkit`, `miquland`, the full `shell/` suite, and all `apps/` — are configured and compiled together in a single CMake invocation. Per-component `make.sh` scripts no longer exist.

### 1. Unified Monorepo Orchestration (`install.sh`)

The root `install.sh` is the **sole** entry point for building and installing the entire ecosystem:

```bash
# Build only (no install) — run as a normal user for local verification:
./install.sh

# Build + system-wide install to /usr and /etc:
sudo ./install.sh
```

**What `install.sh` does:**
1. Configures the entire monorepo with CMake (Ninja preferred, falls back to Unix Makefiles).
2. Compiles all components in parallel (`-j$(nproc)`) in a single Ninja graph — maximising incremental rebuild speed.
3. When run as root: installs everything to `/usr` and `/etc`, then runs post-install hooks (`ldconfig`, `update-desktop-database`, `gtk-update-icon-cache`, `systemctl daemon-reload`).
4. When run as a normal user: validates the build locally in `build/` without touching system directories.

**Build directory:** `<repo_root>/build/` (unified, monorepo-wide).

**CMake build order (enforced by `add_subdirectory` ordering in root `CMakeLists.txt`):**
1. `miqutoolkit` — shared UI library, headers, pkg-config file, in-tree CMake target (`PkgConfig::MIQUTOOLKIT`)
2. `miquland` — Wayland compositor, scenefx, `miquctl` IPC tool
3. `shell/` — all 11 desktop shell components and daemons
4. `apps/` — all 6 standalone desktop applications

**Selective subsystem builds** (via CMake options, forwarded through `install.sh "$@"`):
```bash
# Build and install only the toolkit and apps, skip compositor and shell:
sudo ./install.sh -DBUILD_COMPOSITOR=OFF -DBUILD_SHELL=OFF
```
Available flags: `-DBUILD_TOOLKIT=ON/OFF`, `-DBUILD_COMPOSITOR=ON/OFF`, `-DBUILD_SHELL=ON/OFF`, `-DBUILD_APPS=ON/OFF`.

**Debug builds:**
```bash
BUILD_TYPE=Debug ./install.sh
```

### 2. The Standard `CMakeLists.txt` Pattern

Every downstream component and application follows an identical, minimal structure. This ensures zero-warning monorepo builds and correct runtime library resolution.

1. **Core Standards & Compiler Flags**:
   ```cmake
   cmake_minimum_required(VERSION 3.20)
   project(<app_name> LANGUAGES C CXX)
   set(CMAKE_CXX_STANDARD 20)
   set(CMAKE_CXX_STANDARD_REQUIRED ON)
   set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
   ```

2. **Deterministic RPATH Configuration** *(must be exactly this — no `CMAKE_BUILD_RPATH`)*:
   ```cmake
   # Guarantees the binary locates libmiqutoolkit.so without manual LD_LIBRARY_PATH hacks.
   # CMAKE_BUILD_WITH_INSTALL_RPATH TRUE is mandatory in the monorepo — it prevents CMake
   # from adding the monorepo build directory AND the source-tree miqutoolkit/build to the
   # RPATH simultaneously, which would create an unresolvable cycle in the constraint graph.
   set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib;$ORIGIN/../lib")
   set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
   ```
   > [!CAUTION]
   > **Never** use `CMAKE_BUILD_RPATH` in combination with `CMAKE_BUILD_WITH_INSTALL_RPATH FALSE`
   > for any component that also links against `miqutoolkit`. Doing so causes a CMake RPATH
   > cycle warning during monorepo configure because both `<build>/miqutoolkit` (the in-tree
   > CMake target output) and `miqutoolkit/build` (the source-tree fallback absolute path)
   > would both be added to RPATH with conflicting ordering constraints.

3. **Dual-Mode Toolkit Resolution**:
   The `if(NOT TARGET PkgConfig::MIQUTOOLKIT)` guard makes each component self-contained:
   - **Inside the monorepo**: `miqutoolkit/CMakeLists.txt` defines `add_library(PkgConfig::MIQUTOOLKIT ALIAS miqutoolkit)` before any downstream target is processed, so the guard short-circuits `pkg_check_modules` and the in-tree CMake target is used directly.
   - **Standalone build** (component built independently outside the monorepo): the guard falls through, `pkg_check_modules` resolves the installed system `miqutoolkit`, and the absolute-path fallback provides the local dev-tree `.so`.
   ```cmake
   find_package(PkgConfig REQUIRED)
   if(NOT TARGET PkgConfig::MIQUTOOLKIT)
       pkg_check_modules(MIQUTOOLKIT REQUIRED IMPORTED_TARGET miqutoolkit)
   endif()

   # Local in-tree dev-tree fallback (only active in standalone builds outside the monorepo)
   if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/build/libmiqutoolkit.so")
       target_link_libraries(<app_name> PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/build/libmiqutoolkit.so")
   elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/build/libmiqutoolkit.so")
       target_link_libraries(<app_name> PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/build/libmiqutoolkit.so")
   endif()
   if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/include")
       target_include_directories(<app_name> BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/include")
   elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/include")
       target_include_directories(<app_name> BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/include")
   endif()
   ```

4. **Standard GNU Installations (`GNUInstallDirs`)**:
   ```cmake
   include(GNUInstallDirs)
   install(TARGETS <app_name> RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
   install(FILES assets/<app_name>.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/<app_name>)
   install(FILES assets/<app_name>.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
   install(FILES assets/<app_name>.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
   # Scaled PNG icons installed with RENAME <app_name>.png
   ```

---

## Tool Execution & Safety Restrictions

### 1. Sudo Command Restriction
- The AI agent must **never execute commands with `sudo`**.
- System installation commands requiring root privileges are always provided as instructions for the user to execute manually.

### 2. Local Test Builds Only
- For compilation verification and syntax checking during development, always build into the unified monorepo `build/` directory **without sudo**:
  ```bash
  ./install.sh
  ```
- This runs the full configure + compile cycle (Ninja, parallel) but skips installation, leaving all outputs in `build/`. Do not use `cmake -B build` or `cmake --build build` directly — always go through `install.sh` to stay consistent.

### 3. Downstream Application Build Dependency Order
- All downstream applications dynamically link against `miqutoolkit`. The monorepo CMake graph enforces the correct build order automatically — `miqutoolkit` is always compiled before `shell/` and `apps/`.
- **Authoring vs. Building**:
  - You **may** write, edit, and refactor code for any downstream application or `miqutoolkit` freely.
  - You **must NEVER** run a local build (even `./install.sh` without sudo) until all edits are consistent and compile-ready.
- **When `miqutoolkit` headers or the library ABI are changed**:
  1. Make all edits to `miqutoolkit` and dependent downstream components.
  2. Validate the full ecosystem compiles locally (`./install.sh`).
  3. **STOP immediately**.
  4. Prompt the user to install the updated ecosystem system-wide:
     ```bash
     sudo ./install.sh
     ```
  5. **Wait for user confirmation** that installation has completed before running any downstream binaries or further tests.

---

## Workflow & Collaboration Protocols

- **Git Staging, Commits & Push**: Always explicitly ask the user before staging, committing, or pushing git changes.
- **Documentation & README Synchronization**: Before committing or pushing changes to Git, always ensure that all relevant `README.md` files (root umbrella, repository, or application-specific) are fully updated to accurately reflect any new features, altered CLI commands, configuration options, or architectural changes.
- **Visual Feedback Verification**: Visual output (colors, margins, padding, animations) cannot always be reliably verified programmatically. Ask the user directly for verification (e.g., *"Is the background showing red as expected?"*).
- **Configuration File Synchronization**: Whenever editing `/assets/*.conf`, ask the user whether to sync changes to `~/.config/` and vice versa.
- **Human Readability & Audit Pass**: For all vibecoded features and non-trivial patches, perform a dedicated cleanup pass to ensure the code is immediately human-readable, cleanly commented, free of cryptic boilerplate, and easily maintainable by human developers before submitting for review.

