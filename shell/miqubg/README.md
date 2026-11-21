# miqubg

> **Independent Wayland Wallpaper Daemon & Background Renderer**

`miqubg` is a lightweight, dedicated wallpaper daemon for Wayland compositors, rendering and scaling background images across all connected displays via `wlr-layer-shell`.

Website & Documentation: [**miqu-project.github.io**](https://miqu-project.github.io)

---

## 🔍 What is What

- **Layer Shell Background**: Maps directly to the Wayland background layer (`wlr-layer-shell-unstable-v1`) with zero exclusive zone reservation.
- **Multi-Monitor Display Scaling**: Supports `fill`, `fit`, `stretch`, `center`, and `tile` scaling modes across individual outputs.
- **Dynamic Hotplugging**: Automatically detects newly attached monitors and immediately applies the active wallpaper configuration.
- **Zero Idle CPU**: Renders once to the compositor surface and sleeps; consumes 0.0% CPU when static.

---

## ⚙️ Usage & Configuration

### Starting the Daemon
```bash
# Start daemon with default system asset wallpaper:
miqubg

# Start daemon with custom wallpaper:
miqubg img /path/to/wallpaper.png
```

### Querying Wallpapers
```bash
# Query active wallpaper and settings across all connected monitors:
miqubg query
miqubg list

# Query active wallpaper for a specific monitor:
miqubg query -o HDMI-A-1

# Print raw wallpaper path for scripts, pywal, or lockscreens:
miqubg get
miqubg get -o eDP-1
```

### Setting Wallpapers
```bash
# Set custom wallpaper on all monitors:
miqubg img /path/to/wallpaper.png
miqubg set /path/to/wallpaper.png

# Set custom wallpaper with scaling mode:
miqubg img /path/to/wallpaper.png -m fit -c '#1e1e2e'

# Set custom wallpaper on a specific monitor:
miqubg img /path/to/monitor.png -o HDMI-A-1 -m fill

# Clear wallpaper to a solid background color:
miqubg clear '#1e1e2e'
miqubg clear '#000000' -o HDMI-A-1

# Reset to system default asset wallpaper:
miqubg default
```

### Managing Daemon
```bash
# Reload active wallpapers across all monitors in place:
miqubg reload

# Check if daemon is running and view IPC socket path:
miqubg status
miqubg ping

# Stop the running daemon gracefully:
miqubg kill
miqubg quit
```

---

## 📋 Command & Option Reference

| Command / Option | Shorthand | Description |
| :--- | :--- | :--- |
| `query` / `list` | `-q`, `--query` | Queries active wallpaper, fit mode, and color across all or targeted monitor(s). |
| `get` | `-g`, `--get` | Prints raw wallpaper path (e.g. for `pywal`, `waybar`, lockscreens). |
| `img` / `set` | `-i`, `--image <path>` | Sets wallpaper image across all or targeted monitor(s) (persists to `miqubg.conf`). |
| `clear [color]` | — | Clears image and shows solid background color (default `#000000`). |
| `default` | `-d`, `--default` | Resets wallpaper to system default asset (`/usr/share/miqubg/wallpaper.png`). |
| `reload` | `-r`, `--reload` | Reloads active wallpapers across all monitors in place. |
| `status` / `ping` | — | Checks if daemon is running and displays UNIX socket path. |
| `kill` / `quit` | — | Gracefully stops the running daemon via IPC. |
| `--init-config` | — | Initializes user config `~/.config/miqubg/miqubg.conf` with safe timestamped archiving. |
| `-C, --config <path>`| — | Loads custom configuration file. |
| `-o, --output <name>`| — | Target specific monitor (e.g. `eDP-1`, `HDMI-A-1`, or `*` for all). |
| `-m, --mode <mode>` | — | Scaling mode: `fill` / `cover`, `fit` / `contain`, `stretch`, `center`, `tile`. |
| `-c, --color <#hex>`| — | Solid background color in hex (default: `#000000`). |
| `-v, --version` | `version` | Shows version information. |
| `-h, --help` | `help` | Shows help message. |

### Configuration File (`miqubg.conf`)

`miqubg` adheres to the ecosystem **Two-Tier Configuration Model** defined in `AGENTS.md`:
- **Tier 1 (System Baseline)**: `/usr/share/miqubg/miqubg.conf`
- **Tier 2 (User Config)**: `~/.config/miqubg/miqubg.conf`

Create or reset your user configuration at any time:
```bash
miqubg --init-config
```

#### Hyprland-Compatible Syntax
Wallpapers and monitor overrides are written in unified Hyprland / Miqu syntax:
```ini
[general]
# Default wallpaper across all connected displays
# Syntax: wallpaper = [monitor], <path>, [fit_mode], [bg_color]
wallpaper = , /usr/share/miqubg/wallpaper.png, cover, #000000

# Per-monitor overrides:
wallpaper = HDMI-A-1, ~/Pictures/wallpapers/nature.jpg, fit, #1e1e2e
wallpaper = eDP-1, ~/Pictures/wallpapers/anime.png, cover, #000000
```

#### Automatic Persistence
Whenever you change a wallpaper using CLI commands (e.g., `miqubg img <path>`) or via `miqusettings`, `miqubg` automatically updates `~/.config/miqubg/miqubg.conf` so your desktop backgrounds persist seamlessly across reboots and restarts.

### Scaling Modes
- **`fill` / `cover`** (default): Scale to fill output, cropping if aspect ratio differs.
- **`fit` / `contain`**: Scale to fit inside output, letterboxing with solid background color.
- **`stretch`**: Stretch image to fill output, ignoring aspect ratio.
- **`center`**: Center image at 1:1 scale without scaling.
- **`tile`**: Tile the image repeatedly across the output.

Default asset wallpaper location: `/usr/share/miqubg/wallpaper.png`.  
Config location: `~/.config/miqubg/miqubg.conf`.

---

## 🔨 Build & Run

```bash
# In monorepo root:
cmake --build build --target miqubg

# Standalone build:
./make.sh

# Install system-wide:
sudo ./make.sh
```
