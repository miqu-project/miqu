#include "miller_view.hpp"
#include "preview_common.hpp"
#include "monitor_backend.hpp"
#include <miqutoolkit/view/popup_window.hpp>
#include <miqutoolkit/view/edit_text.hpp>
#include <miqutoolkit/view/frame_layout.hpp>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <filesystem>
#include <thread>
#include <cstring>
#include <unistd.h>
#include <dlfcn.h>

namespace fs = std::filesystem;

namespace miqu {
void MillerView::render_tiling_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    if (!item || !item->tiling_cfg) return;
    bool show_layout = (item->id == "tile_mode");
    bool show_windows = (item->id == "tile_windows");
    bool show_split = (item->id == "tile_split");
    bool show_focus = (item->id == "tile_focus");

    auto main_card = create_form_card(config);

    // 1. Layout Mode
    if (show_layout) {
        int cur_mode_idx = 0;
        for (size_t i = 0; i < item->tiling_cfg->available_layout_modes.size(); ++i) {
            if (item->tiling_cfg->available_layout_modes[i] == item->tiling_cfg->layout_mode) {
                cur_mode_idx = static_cast<int>(i);
                break;
            }
        }

        add_form_spinner(
            main_card, config,
            "Tiling algorithm",
            "",
            item->tiling_cfg->available_layout_modes,
            cur_mode_idx,
            [item](int idx, const std::string& val) {
                (void)idx;
                item->tiling_cfg->layout_mode = val;
            }
        );
    }

    // 2. Windows
    if (show_windows) {
        add_form_slider(
            main_card, config,
            "Inner window gap",
            "Spacing between adjacent tiled windows (in pixels)",
            static_cast<float>(item->tiling_cfg->inner_gap) / 32.0f,
            std::to_string(item->tiling_cfg->inner_gap) + " px",
            [item](float val) -> std::string {
                int px = static_cast<int>(std::round(val * 32.0f));
                item->tiling_cfg->inner_gap = px;
                return std::to_string(px) + " px";
            }
        );

        add_form_slider(
            main_card, config,
            "Outer screen edge gap",
            "Margin between window borders and monitor screen edges",
            static_cast<float>(item->tiling_cfg->outer_gap) / 48.0f,
            std::to_string(item->tiling_cfg->outer_gap) + " px",
            [item](float val) -> std::string {
                int px = static_cast<int>(std::round(val * 48.0f));
                item->tiling_cfg->outer_gap = px;
                return std::to_string(px) + " px";
            }
        );

        add_form_toggle(
            main_card, config,
            "Smart gaps",
            "Automatically collapse gaps and borders when only one window is open",
            item->tiling_cfg->smart_gaps,
            [item](bool val) {
                item->tiling_cfg->smart_gaps = val;
            }
        );

        add_form_slider(
            main_card, config,
            "Window border width",
            "Border stroke thickness in pixels (0 disables window borders)",
            static_cast<float>(item->tiling_cfg->border_width) / 8.0f,
            item->tiling_cfg->border_width == 0 ? "Disabled (0 px)" : (std::to_string(item->tiling_cfg->border_width) + " px"),
            [item](float val) -> std::string {
                int px = static_cast<int>(std::round(val * 8.0f));
                item->tiling_cfg->border_width = px;
                return px == 0 ? "Disabled (0 px)" : (std::to_string(px) + " px");
            }
        );

        add_form_slider(
            main_card, config,
            "Window corner radius",
            "Rounding curvature applied to window frames and drop shadows",
            static_cast<float>(item->tiling_cfg->corner_radius) / 32.0f,
            std::to_string(item->tiling_cfg->corner_radius) + " px",
            [item](float val) -> std::string {
                int px = static_cast<int>(std::round(val * 32.0f));
                item->tiling_cfg->corner_radius = px;
                return std::to_string(px) + " px";
            }
        );

        int cur_act_idx = 0;
        for (size_t i = 0; i < item->tiling_cfg->active_border_presets.size(); ++i) {
            if (item->tiling_cfg->active_border_presets[i] == item->tiling_cfg->active_border_preset) {
                cur_act_idx = static_cast<int>(i);
                break;
            }
        }
        add_form_spinner(
            main_card, config,
            "Active window border color",
            "Border color or multi-stop linear gradient for focused window",
            item->tiling_cfg->active_border_presets,
            cur_act_idx,
            [item](int idx, const std::string& val) {
                (void)idx;
                item->tiling_cfg->active_border_preset = val;
                if (val.find("Yellow-Red") != std::string::npos) item->tiling_cfg->active_border_value = "#ffff00 #ff0000 45deg";
                else if (val.find("Cyan-Blue") != std::string::npos) item->tiling_cfg->active_border_value = "#00ffcc #0066ff 45deg";
                else if (val.find("Purple-Blue") != std::string::npos) item->tiling_cfg->active_border_value = "#ff007f #7928ca #0066ff 135deg";
                else if (val.find("Neon") != std::string::npos) item->tiling_cfg->active_border_value = "#00ff88 #00ccff 90deg";
                else if (val.find("Monochrome") != std::string::npos) item->tiling_cfg->active_border_value = "#ffffff";
                else item->tiling_cfg->active_border_value = "#0066ff";
            }
        );

        int cur_inact_idx = 0;
        for (size_t i = 0; i < item->tiling_cfg->inactive_border_presets.size(); ++i) {
            if (item->tiling_cfg->inactive_border_presets[i] == item->tiling_cfg->inactive_border_preset) {
                cur_inact_idx = static_cast<int>(i);
                break;
            }
        }
        add_form_spinner(
            main_card, config,
            "Inactive window border color",
            "Border styling for unfocused background windows",
            item->tiling_cfg->inactive_border_presets,
            cur_inact_idx,
            [item](int idx, const std::string& val) {
                (void)idx;
                item->tiling_cfg->inactive_border_preset = val;
                if (val.find("Dark Slate") != std::string::npos) item->tiling_cfg->inactive_border_value = "#334455 #222d38 45deg";
                else if (val.find("Graphite") != std::string::npos) item->tiling_cfg->inactive_border_value = "#2d3748";
                else if (val.find("Muted Amber") != std::string::npos) item->tiling_cfg->inactive_border_value = "#aa9900 #aa3333 45deg";
                else if (val.find("Subtle") != std::string::npos) item->tiling_cfg->inactive_border_value = "#ffffff33";
                else item->tiling_cfg->inactive_border_value = "#00000000";
            }
        );

        add_form_toggle(
            main_card, config,
            "Border specular shading",
            "Subtle 3D lighting and specular bevel depth across the border stroke",
            item->tiling_cfg->border_shading,
            [item](bool val) {
                item->tiling_cfg->border_shading = val;
            }
        );

        if (item->tiling_cfg->border_shading) {
            add_form_slider(
                main_card, config,
                "Shading intensity",
                "Specular bevel strength across the window frame",
                item->tiling_cfg->border_shading_strength,
                std::to_string(static_cast<int>(item->tiling_cfg->border_shading_strength * 100)) + "%",
                [item](float val) -> std::string {
                    item->tiling_cfg->border_shading_strength = val;
                    return std::to_string(static_cast<int>(val * 100)) + "%";
                }
            );
        }

        add_form_toggle(
            main_card, config,
            "Direct border drag resizing",
            "Resize windows by dragging directly on window borders without modifier key",
            item->tiling_cfg->resize_on_border,
            [item](bool val) {
                item->tiling_cfg->resize_on_border = val;
            }
        );

        add_form_toggle(
            main_card, config,
            "Auto-float fixed-size dialogs",
            "Automatically floats splash screens and dialogs declaring fixed min/max size constraints",
            item->tiling_cfg->auto_float_fixed_size,
            [item](bool val) {
                item->tiling_cfg->auto_float_fixed_size = val;
            }
        );
    }

    // 3. Dwindle (BSP) Tuning
    if (show_split) {
        float initial_ratio_norm = (item->tiling_cfg->default_split_ratio - 0.20f) / 0.60f;
        int r_pct = static_cast<int>(std::round(item->tiling_cfg->default_split_ratio * 100.0f));
        add_form_slider(
            main_card, config,
            "Default split ratio",
            "Initial screen division ratio when a new window partitions a workspace",
            initial_ratio_norm,
            std::to_string(r_pct) + "% / " + std::to_string(100 - r_pct) + "%",
            [item](float val) -> std::string {
                float ratio = 0.20f + val * 0.60f;
                item->tiling_cfg->default_split_ratio = ratio;
                int pct = static_cast<int>(std::round(ratio * 100.0f));
                return std::to_string(pct) + "% / " + std::to_string(100 - pct) + "%";
            }
        );

        add_form_toggle(
            main_card, config,
            "Smart cursor-aware splitting",
            "Cursor position inside target window dictates horizontal or vertical split side",
            item->tiling_cfg->smart_split,
            [item](bool val) {
                item->tiling_cfg->smart_split = val;
            }
        );

        add_form_toggle(
            main_card, config,
            "Split focused window",
            "New windows partition the currently active window instead of spawning at the spiral tail",
            item->tiling_cfg->split_focus,
            [item](bool val) {
                item->tiling_cfg->split_focus = val;
            }
        );

        add_form_toggle(
            main_card, config,
            "Preserve split orientation",
            "Retains horizontal or vertical split axes when windows are moved or closed",
            item->tiling_cfg->preserve_split,
            [item](bool val) {
                item->tiling_cfg->preserve_split = val;
            }
        );

        float initial_mult_norm = (item->tiling_cfg->split_width_multiplier - 0.8f) / 1.7f;
        auto format_mult = [](float m) -> std::string {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%.1f×", m);
            return std::string(buf);
        };
        add_form_slider(
            main_card, config,
            "Widescreen split width multiplier",
            "Aspect ratio threshold to favor side-by-side columns (width >= height × mult)",
            initial_mult_norm,
            format_mult(item->tiling_cfg->split_width_multiplier),
            [item, format_mult](float val) -> std::string {
                float mult = 0.8f + val * 1.7f;
                item->tiling_cfg->split_width_multiplier = mult;
                return format_mult(mult);
            }
        );
    }

    // 4. Focus Behaviour
    if (show_focus) {
        add_form_toggle(
            main_card, config,
            "Focus follows mouse (Sloppy focus)",
            "Moving mouse cursor over a window automatically shifts keyboard focus to it",
            item->tiling_cfg->focus_follows_mouse,
            [item](bool val) {
                item->tiling_cfg->focus_follows_mouse = val;
            }
        );

        add_form_toggle(
            main_card, config,
            "Click to focus and raise",
            "Clicking anywhere on an unfocused window brings it to the top of its layer and activates it",
            item->tiling_cfg->click_to_focus,
            [item](bool val) {
                item->tiling_cfg->click_to_focus = val;
            }
        );

        add_form_toggle(
            main_card, config,
            "Warp cursor on focus change",
            "Automatically moves mouse pointer to the center of the focused window on keyboard navigation",
            item->tiling_cfg->warp_cursor_on_focus,
            [item](bool val) {
                item->tiling_cfg->warp_cursor_on_focus = val;
            }
        );

        add_form_toggle(
            main_card, config,
            "Focus on workspace switch",
            "Automatically restores focus to the last active window when switching workspaces",
            item->tiling_cfg->focus_on_workspace_switch,
            [item](bool val) {
                item->tiling_cfg->focus_on_workspace_switch = val;
            }
        );
    }

    add_preview_card(main_card);
}

static std::string expand_wallpaper_path(const std::string& path) {
    if (path.empty() || path[0] != '~') return path;
    const char* home = std::getenv("HOME");
    if (!home) return path;
    if (path.size() == 1) return std::string(home);
    if (path[1] == '/') return std::string(home) + path.substr(1);
    return path;
}

static FitMode wallpaper_str_to_fit_mode(const std::string& mode_str) {
    std::string s = mode_str;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (s == "fit" || s == "contain") return FitMode::Contain;
    if (s == "stretch" || s == "fill") return FitMode::Fill;
    if (s == "center") return FitMode::Center;
    if (s == "tile") return FitMode::Tile;
    return FitMode::Cover;
}

static std::string trim_val(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n\"'");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n\"'");
    return str.substr(first, (last - first + 1));
}

static bool load_miqubg_wallpaper_config(std::string& image_path, std::string& fit_mode, std::string& bg_color, const std::string& target_monitor = "All Displays") {
    const char* home = std::getenv("HOME");
    fs::path target_conf;
    if (home) {
        fs::path user_conf = fs::path(home) / ".config" / "miqubg" / "miqubg.conf";
        if (fs::exists(user_conf)) {
            target_conf = user_conf;
        }
    }
    if (target_conf.empty()) {
        for (const char* p : {"/usr/share/miqubg/miqubg.conf", "/usr/local/share/miqubg/miqubg.conf", "assets/miqubg.conf", "shell/miqubg/assets/miqubg.conf"}) {
            if (fs::exists(p)) {
                target_conf = p;
                break;
            }
        }
    }
    if (target_conf.empty() || !fs::exists(target_conf)) return false;

    std::ifstream in(target_conf);
    if (!in.is_open()) return false;

    bool is_global_target = (target_monitor.empty() || target_monitor == "All Displays" || target_monitor == "*");
    std::string current_section = "general";

    std::string global_img, global_mode, global_color;
    bool found_global = false;

    std::string specific_img, specific_mode, specific_color;
    bool found_specific = false;

    std::string line;
    while (std::getline(in, line)) {
        std::string trimmed = trim_val(line);
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') continue;

        if (trimmed.front() == '[' && trimmed.back() == ']') {
            std::string sec = trim_val(trimmed.substr(1, trimmed.size() - 2));
            std::string sec_lower = sec;
            std::transform(sec_lower.begin(), sec_lower.end(), sec_lower.begin(), ::tolower);
            if (sec_lower.rfind("output:", 0) == 0) {
                current_section = sec.substr(7);
            } else {
                current_section = sec_lower;
            }
            continue;
        }

        size_t eq = trimmed.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim_val(trimmed.substr(0, eq));
        std::string val = trim_val(trimmed.substr(eq + 1));
        std::string k_lower = key;
        std::transform(k_lower.begin(), k_lower.end(), k_lower.begin(), ::tolower);

        if (k_lower == "wallpaper") {
            std::istringstream ss(val);
            std::string tok;
            std::vector<std::string> tokens;
            while (std::getline(ss, tok, ',')) {
                tokens.push_back(trim_val(tok));
            }

            std::string entry_mon = "";
            std::string entry_img = "";
            std::string entry_mode = "Cover";
            std::string entry_col = "#000000";

            if (tokens.size() == 1) {
                entry_img = tokens[0];
                if (current_section != "general") entry_mon = current_section;
            } else if (tokens.size() >= 2) {
                if (tokens[0].rfind('/', 0) == 0 || tokens[0].rfind('~', 0) == 0) {
                    entry_img = tokens[0];
                    entry_mode = tokens[1];
                    if (tokens.size() >= 3) entry_col = tokens[2];
                    if (current_section != "general") entry_mon = current_section;
                } else {
                    entry_mon = tokens[0];
                    entry_img = tokens[1];
                    if (tokens.size() >= 3) entry_mode = tokens[2];
                    if (tokens.size() >= 4) entry_col = tokens[3];
                }
            }

            if (entry_mon.empty() || entry_mon == "*") {
                global_img = entry_img;
                global_mode = entry_mode;
                global_color = entry_col;
                found_global = true;
            } else if (!is_global_target && entry_mon == target_monitor) {
                specific_img = entry_img;
                specific_mode = entry_mode;
                specific_color = entry_col;
                found_specific = true;
            }
        }
    }

    if (!is_global_target && found_specific) {
        image_path = specific_img;
        fit_mode = specific_mode;
        bg_color = specific_color;
        return true;
    } else if (found_global) {
        image_path = global_img;
        fit_mode = global_mode;
        bg_color = global_color;
        return true;
    }

    return false;
}

static bool sync_miqubg_wallpaper_config(const std::string& image_path, const std::string& fit_mode, const std::string& bg_color = "#000000", const std::string& target_monitor = "All Displays") {
    const char* home = std::getenv("HOME");
    if (!home) return false;

    fs::path bg_dir = fs::path(home) / ".config" / "miqubg";
    std::error_code ec;
    if (!fs::exists(bg_dir, ec) || !fs::exists(bg_dir / "miqubg.conf", ec)) {
        // Fresh install: initialize complete standard configuration via miqubg
        std::system("miqubg --init-config >/dev/null 2>&1");
    }
    if (!fs::exists(bg_dir, ec)) {
        fs::create_directories(bg_dir, ec);
    }
    fs::path miqubg_conf = bg_dir / "miqubg.conf";
    if (!fs::exists(miqubg_conf, ec)) {
        for (const char* p : {"/usr/share/miqubg/miqubg.conf", "/usr/local/share/miqubg/miqubg.conf", "assets/miqubg.conf", "shell/miqubg/assets/miqubg.conf"}) {
            if (fs::exists(p, ec)) {
                fs::copy_file(p, miqubg_conf, fs::copy_options::overwrite_existing, ec);
                break;
            }
        }
    }

    std::string mode_str = "cover";
    std::string lower = fit_mode;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower == "fit" || lower == "contain") mode_str = "contain";
    else if (lower == "stretch" || lower == "fill") mode_str = "fill";
    else if (lower == "center") mode_str = "center";
    else if (lower == "tile") mode_str = "tile";
    else mode_str = "cover";

    bool is_global = (target_monitor.empty() || target_monitor == "All Displays" || target_monitor == "*");

    std::vector<std::string> lines;
    bool replaced = false;
    std::string current_section = "general";

    if (fs::exists(miqubg_conf)) {
        std::ifstream in(miqubg_conf);
        if (in.is_open()) {
            std::string line;
            while (std::getline(in, line)) {
                std::string t = trim_val(line);
                if (t.front() == '[' && t.back() == ']') {
                    std::string sec = trim_val(t.substr(1, t.size() - 2));
                    std::string sec_lower = sec;
                    std::transform(sec_lower.begin(), sec_lower.end(), sec_lower.begin(), ::tolower);
                    if (sec_lower.rfind("output:", 0) == 0) {
                        current_section = sec.substr(7);
                    } else {
                        current_section = sec_lower;
                    }
                    lines.push_back(line);
                    continue;
                }

                std::string t_lower = t;
                std::transform(t_lower.begin(), t_lower.end(), t_lower.begin(), ::tolower);
                if (t_lower.rfind("wallpaper", 0) == 0) {
                    size_t eq = t.find('=');
                    if (eq != std::string::npos) {
                        std::string val = trim_val(t.substr(eq + 1));
                        std::istringstream ss(val);
                        std::string tok;
                        std::vector<std::string> tokens;
                        while (std::getline(ss, tok, ',')) {
                            tokens.push_back(trim_val(tok));
                        }

                        std::string entry_mon = "";
                        if (tokens.size() == 1) {
                            if (current_section != "general") entry_mon = current_section;
                        } else if (tokens.size() >= 2) {
                            if (tokens[0].rfind('/', 0) == 0 || tokens[0].rfind('~', 0) == 0) {
                                if (current_section != "general") entry_mon = current_section;
                            } else {
                                entry_mon = tokens[0];
                            }
                        }

                        if (is_global && (entry_mon.empty() || entry_mon == "*")) {
                            lines.push_back("wallpaper = , " + image_path + ", " + mode_str + ", " + bg_color);
                            replaced = true;
                            continue;
                        } else if (!is_global && entry_mon == target_monitor) {
                            lines.push_back("wallpaper = " + target_monitor + ", " + image_path + ", " + mode_str + ", " + bg_color);
                            replaced = true;
                            continue;
                        }
                    }
                }
                lines.push_back(line);
            }
        }
    }

    if (!replaced) {
        if (is_global) {
            lines.push_back("wallpaper = , " + image_path + ", " + mode_str + ", " + bg_color);
        } else {
            lines.push_back("wallpaper = " + target_monitor + ", " + image_path + ", " + mode_str + ", " + bg_color);
        }
    }

    std::ofstream out(miqubg_conf);
    if (!out.is_open()) return false;
    for (const auto& l : lines) {
        out << l << "\n";
    }
    out.flush();
    out.close();

    // If miqubg daemon is not currently running, launch it in the background
    if (std::system("miqubg status >/dev/null 2>&1") != 0) {
        std::system("miqubg >/dev/null 2>&1 &");
    }

    return true;
}

struct ColorPreset {
    std::string id;
    std::string name;
    Color primary;
    Color secondary;
    Color tertiary;
};

static bool is_dark_mode_enabled() {
    std::vector<std::string> search_paths;
    const char* home = std::getenv("HOME");
    if (home) {
        search_paths.push_back(std::string(home) + "/.config/miqusettings/darkmode");
    }
    search_paths.push_back("assets/darkmode");
    search_paths.push_back("../assets/darkmode");
    search_paths.push_back("miqu/apps/miqusettings/assets/darkmode");
    search_paths.push_back("/usr/share/miqusettings/darkmode");
    search_paths.push_back("/usr/local/share/miqusettings/darkmode");

    for (const auto& path : search_paths) {
        std::error_code ec;
        if (fs::exists(path, ec) && fs::is_regular_file(path, ec)) {
            std::ifstream f(path);
            std::string content;
            if (f >> content) {
                std::transform(content.begin(), content.end(), content.begin(), ::tolower);
                if (content == "on" || content == "true" || content == "1" || content == "dark") {
                    return true;
                } else if (content == "off" || content == "false" || content == "0" || content == "light") {
                    return false;
                }
            }
        }
    }

    return true;
}

static void set_dark_mode_enabled(bool enabled) {
    std::string val = enabled ? "on\n" : "off\n";

    const char* home = std::getenv("HOME");
    if (home) {
        std::string user_dir = std::string(home) + "/.config/miqusettings";
        std::error_code ec;
        fs::create_directories(user_dir, ec);
        std::ofstream f(user_dir + "/darkmode");
        if (f.is_open()) {
            f << val;
        }
    }
}

static int get_transparency() {
    const char* home = std::getenv("HOME");
    if (home) {
        std::ifstream f(std::string(home) + "/.config/miqusettings/transparency");
        std::string s;
        if (f >> s && !s.empty()) {
            std::string num_str;
            for (char c : s) if (std::isdigit(c)) num_str += c;
            if (!num_str.empty()) {
                return std::clamp(std::stoi(num_str), 0, 100);
            }
        }
    }
    return 100;
}

static void set_transparency(int percent) {
    int val = std::clamp(percent, 0, 100);
    const char* home = std::getenv("HOME");
    if (home) {
        std::string user_dir = std::string(home) + "/.config/miqusettings";
        std::error_code ec;
        fs::create_directories(user_dir, ec);
        std::ofstream f(user_dir + "/transparency");
        if (f.is_open()) {
            f << val << "%\n";
        }
    }
}

static std::string find_schemes_toml_path() {
    std::vector<std::string> search_paths;
    const char* home = std::getenv("HOME");
    if (home) {
        search_paths.push_back(std::string(home) + "/.config/miqusettings/schemes.toml");
    }
    search_paths.push_back("assets/schemes.toml");
    search_paths.push_back("../assets/schemes.toml");
    search_paths.push_back("miqu/apps/miqusettings/assets/schemes.toml");
    search_paths.push_back("/usr/share/miqusettings/schemes.toml");
    search_paths.push_back("/usr/local/share/miqusettings/schemes.toml");

    for (const auto& path : search_paths) {
        std::error_code ec;
        if (fs::exists(path, ec) && fs::is_regular_file(path, ec)) {
            return path;
        }
    }
    return "";
}

static std::vector<ColorPreset> load_preset_schemes_from_toml(const std::string& path, bool dark_mode) {
    std::ifstream file(path);
    if (!file.is_open()) return {};

    std::string target_mode = dark_mode ? "dark" : "light";

    struct SchemeData {
        std::string id;
        std::string display_name;
        std::string primary;
        std::string accent;
        std::string surface;
    };

    std::vector<SchemeData> schemes;
    std::unordered_map<std::string, size_t> id_to_index;

    std::string line;
    std::string current_section;

    auto trim = [](const std::string& s) -> std::string {
        size_t first = s.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = s.find_last_not_of(" \t\r\n");
        return s.substr(first, (last - first + 1));
    };

    auto strip_quotes = [](const std::string& s) -> std::string {
        if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
            return s.substr(1, s.size() - 2);
        }
        return s;
    };

    while (std::getline(file, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        if (trimmed.front() == '[' && trimmed.back() == ']') {
            current_section = trim(trimmed.substr(1, trimmed.size() - 2));
            if (current_section.find('.') == std::string::npos) {
                std::string sid = current_section;
                if (id_to_index.find(sid) == id_to_index.end()) {
                    id_to_index[sid] = schemes.size();
                    SchemeData sd;
                    sd.id = sid;
                    sd.display_name = sid;
                    schemes.push_back(sd);
                }
            }
            continue;
        }

        size_t eq_pos = trimmed.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = trim(trimmed.substr(0, eq_pos));
        std::string val = strip_quotes(trim(trimmed.substr(eq_pos + 1)));

        if (current_section.find('.') == std::string::npos) {
            std::string sid = current_section;
            if (id_to_index.find(sid) != id_to_index.end()) {
                if (key == "display_name") {
                    schemes[id_to_index[sid]].display_name = val;
                }
            }
        } else {
            std::stringstream ss(current_section);
            std::string sid, smode, sapp;
            if (std::getline(ss, sid, '.') && std::getline(ss, smode, '.') && std::getline(ss, sapp, '.')) {
                if (smode == target_mode) {
                    if (id_to_index.find(sid) == id_to_index.end()) {
                        id_to_index[sid] = schemes.size();
                        SchemeData sd;
                        sd.id = sid;
                        sd.display_name = sid;
                        schemes.push_back(sd);
                    }
                    size_t idx = id_to_index[sid];
                    if (sapp == "miqutoolkit") {
                        if (key == "primary" && schemes[idx].primary.empty()) schemes[idx].primary = val;
                        if (key == "surface" && schemes[idx].surface.empty()) schemes[idx].surface = val;
                        if (key == "surface_variant" && schemes[idx].accent.empty()) schemes[idx].accent = val;
                    } else if (sapp == "waybar") {
                        if (key == "accent") schemes[idx].accent = val;
                        if (key == "primary" && schemes[idx].primary.empty()) schemes[idx].primary = val;
                    }
                }
            }
        }
    }

    std::vector<ColorPreset> result;
    for (const auto& sd : schemes) {
        if (!sd.primary.empty()) {
            Color p = Color::from_hex(sd.primary);
            Color a = !sd.accent.empty() ? Color::from_hex(sd.accent) : p;
            Color s = !sd.surface.empty() ? Color::from_hex(sd.surface) : Color::from_hex("#333333");
            result.push_back({sd.id, sd.display_name, p, a, s});
        }
    }
    return result;
}

static std::vector<ColorPreset> load_active_preset_schemes() {
    bool dark = is_dark_mode_enabled();
    std::string toml_path = find_schemes_toml_path();
    if (!toml_path.empty()) {
        auto loaded = load_preset_schemes_from_toml(toml_path, dark);
        if (!loaded.empty()) {
            return loaded;
        }
    }
    return {
        {"sapphire",   "Sapphire",   Color::from_hex("#3584e4"), Color::from_hex("#78aeed"), Color::from_hex("#1c3d5a")},
        {"emerald",    "Emerald",    Color::from_hex("#2ec27e"), Color::from_hex("#8ff0a4"), Color::from_hex("#1b4332")},
        {"sunset",     "Sunset",     Color::from_hex("#e66100"), Color::from_hex("#f6a65e"), Color::from_hex("#5c2600")},
        {"lavender",   "Lavender",   Color::from_hex("#9141ac"), Color::from_hex("#dc8add"), Color::from_hex("#3d1a47")},
        {"arctic",     "Arctic",     Color::from_hex("#00a896"), Color::from_hex("#02c39a"), Color::from_hex("#05383b")},
        {"rose",       "Rose",       Color::from_hex("#e01b24"), Color::from_hex("#f88379"), Color::from_hex("#5c060a")},
        {"caramel",    "Caramel",    Color::from_hex("#c678dd"), Color::from_hex("#e5c07b"), Color::from_hex("#2c2530")},
        {"monochrome", "Monochrome", Color::from_hex("#7f8c8d"), Color::from_hex("#bdc3c7"), Color::from_hex("#2c3e50")}
    };
}

static int s_selected_scheme_idx = 0;

struct SchemeFullData {
    std::string id;
    std::string display_name;
    std::unordered_map<std::string, std::string> miqutoolkit;
    std::vector<std::pair<std::string, std::string>> gtk;
    std::unordered_map<std::string, std::string> kitty;
    std::unordered_map<std::string, std::string> waybar;
};

static std::string get_current_scheme_id() {
    const char* home = std::getenv("HOME");
    if (home) {
        std::ifstream f(std::string(home) + "/.config/miqusettings/current_scheme");
        std::string id;
        if (f >> id && !id.empty()) {
            return id;
        }
    }
    return "catppuccin";
}

static bool load_scheme_full_data(const std::string& toml_path, const std::string& target_id, bool dark_mode, SchemeFullData& out_data) {
    std::ifstream file(toml_path);
    if (!file.is_open()) return false;

    std::string target_mode = dark_mode ? "dark" : "light";
    out_data.id = target_id;
    out_data.display_name = target_id;

    std::string line;
    std::string current_section;

    auto trim = [](const std::string& s) -> std::string {
        size_t first = s.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = s.find_last_not_of(" \t\r\n");
        return s.substr(first, (last - first + 1));
    };

    auto strip_quotes = [](const std::string& s) -> std::string {
        if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
            return s.substr(1, s.size() - 2);
        }
        return s;
    };

    while (std::getline(file, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        if (trimmed.front() == '[' && trimmed.back() == ']') {
            current_section = trim(trimmed.substr(1, trimmed.size() - 2));
            continue;
        }

        size_t eq_pos = trimmed.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = trim(trimmed.substr(0, eq_pos));
        std::string val = strip_quotes(trim(trimmed.substr(eq_pos + 1)));

        if (current_section == target_id) {
            if (key == "display_name") out_data.display_name = val;
        } else {
            std::stringstream ss(current_section);
            std::string sid, smode, sapp;
            if (std::getline(ss, sid, '.') && std::getline(ss, smode, '.') && std::getline(ss, sapp, '.')) {
                if (sid == target_id && smode == target_mode) {
                    if (sapp == "miqutoolkit") {
                        out_data.miqutoolkit[key] = val;
                    } else if (sapp == "gtk") {
                        out_data.gtk.emplace_back(key, val);
                    } else if (sapp == "kitty") {
                        out_data.kitty[key] = val;
                    } else if (sapp == "waybar") {
                        out_data.waybar[key] = val;
                    }
                }
            }
        }
    }

    return !out_data.miqutoolkit.empty();
}

static void update_electron_ide_theme(bool dark_mode, const SchemeFullData& data) {
    const char* home = std::getenv("HOME");
    if (!home) return;
    fs::path home_path(home);
    fs::path gemini_cfg = home_path / ".gemini/config/config.json";

    std::string bg_hex = dark_mode ? "#111318" : "#f6faff";
    std::string primary_hex = dark_mode ? "#a9c7ff" : "#003853";
    std::string fg_hex = dark_mode ? "#e2e2e9" : "#000000";

    auto it_bg = data.miqutoolkit.find("background");
    if (it_bg != data.miqutoolkit.end() && it_bg->second.size() >= 7 && it_bg->second[0] == '#') {
        bg_hex = it_bg->second.substr(0, 7);
    }
    auto it_pri = data.miqutoolkit.find("primary");
    if (it_pri != data.miqutoolkit.end() && it_pri->second.size() >= 7 && it_pri->second[0] == '#') {
        primary_hex = it_pri->second.substr(0, 7);
    }
    auto it_fg = data.miqutoolkit.find("on_background");
    if (it_fg == data.miqutoolkit.end()) it_fg = data.miqutoolkit.find("on_surface");
    if (it_fg != data.miqutoolkit.end() && it_fg->second.size() >= 7 && it_fg->second[0] == '#') {
        fg_hex = it_fg->second.substr(0, 7);
    }

    std::string target_mode_str = dark_mode ? "THEME_MODE_DARK" : "THEME_MODE_LIGHT";
    std::string seed_key = dark_mode ? "customThemeSeedsDark" : "customThemeSeedsLight";

    // 1. Update config.json on disk
    std::error_code ec;
    if (fs::exists(gemini_cfg, ec)) {
        std::ifstream in(gemini_cfg);
        if (in.is_open()) {
            std::stringstream buf;
            buf << in.rdbuf();
            std::string content = buf.str();
            in.close();

            // Patch themeMode
            size_t pos = content.find("\"themeMode\"");
            if (pos != std::string::npos) {
                size_t colon = content.find(':', pos);
                size_t q1 = content.find('"', colon);
                size_t q2 = content.find('"', q1 + 1);
                if (q1 != std::string::npos && q2 != std::string::npos) {
                    content.replace(q1 + 1, q2 - q1 - 1, target_mode_str);
                }
            }

            // Patch seed_key values
            size_t s_pos = content.find("\"" + seed_key + "\"");
            if (s_pos != std::string::npos) {
                auto patch_field = [&](const std::string& field, const std::string& val) {
                    size_t f_pos = content.find("\"" + field + "\"", s_pos);
                    if (f_pos != std::string::npos && f_pos < s_pos + 200) {
                        size_t col = content.find(':', f_pos);
                        size_t val_q1 = content.find('"', col);
                        size_t val_q2 = content.find('"', val_q1 + 1);
                        if (val_q1 != std::string::npos && val_q2 != std::string::npos) {
                            content.replace(val_q1 + 1, val_q2 - val_q1 - 1, val);
                        }
                    }
                };
                patch_field("background", bg_hex);
                patch_field("primary", primary_hex);
                patch_field("foregroundOverride", fg_hex);
            }

            std::ofstream out(gemini_cfg);
            if (out.is_open()) {
                out << content;
            }
        }
    }

    // 2. Live update running Antigravity instance via Connect-RPC
    std::string port, token;

    // Try finding port from language_server.log
    fs::path lang_log = home_path / ".config/Antigravity/logs/language_server.log";
    if (fs::exists(lang_log, ec)) {
        std::ifstream lf(lang_log);
        if (lf.is_open()) {
            std::string line;
            std::vector<std::string> llines;
            while (std::getline(lf, line)) {
                llines.push_back(line);
            }
            for (auto rit = llines.rbegin(); rit != llines.rend(); ++rit) {
                if (rit->find("Language server listening on random port at ") != std::string::npos &&
                    rit->find("for HTTPS") != std::string::npos) {
                    size_t at_pos = rit->find(" at ");
                    if (at_pos != std::string::npos) {
                        size_t sp = rit->find(' ', at_pos + 4);
                        if (sp != std::string::npos) {
                            port = rit->substr(at_pos + 4, sp - (at_pos + 4));
                            break;
                        }
                    }
                }
            }
        }
    }

    // Try finding live csrf token from /proc/*/cmdline
    for (const auto& entry : fs::directory_iterator("/proc", ec)) {
        if (!entry.is_directory(ec)) continue;
        std::string filename = entry.path().filename().string();
        if (filename.empty() || !std::isdigit(static_cast<unsigned char>(filename[0]))) continue;
        fs::path cmdline_path = entry.path() / "cmdline";
        std::ifstream cf(cmdline_path, std::ios::binary);
        if (!cf.is_open()) continue;
        std::string arg;
        bool found_csrf = false;
        while (std::getline(cf, arg, '\0')) {
            if (found_csrf) {
                token = arg;
                break;
            }
            if (arg == "--csrf_token") {
                found_csrf = true;
            }
        }
        if (!token.empty()) break;
    }

    // Fallback: search main.log if port or token still empty
    if (port.empty() || token.empty()) {
        fs::path main_log = home_path / ".config/Antigravity/logs/main.log";
        if (fs::exists(main_log, ec)) {
            std::ifstream mf(main_log);
            if (mf.is_open()) {
                std::string line;
                std::vector<std::string> mlines;
                while (std::getline(mf, line)) {
                    mlines.push_back(line);
                }
                for (auto rit = mlines.rbegin(); rit != mlines.rend(); ++rit) {
                    if (port.empty()) {
                        size_t p_found = rit->find("Port changed! Reloading all windows with URL: https://127.0.0.1:");
                        if (p_found != std::string::npos) {
                            size_t colon = rit->find("127.0.0.1:", p_found);
                            if (colon != std::string::npos) {
                                size_t start = colon + 10;
                                size_t slash = rit->find('/', start);
                                if (slash != std::string::npos) {
                                    port = rit->substr(start, slash - start);
                                }
                            }
                        }
                    }
                    if (token.empty()) {
                        size_t t_found = rit->find("--csrf_token");
                        if (t_found != std::string::npos) {
                            size_t start = t_found + 12;
                            while (start < rit->length() && (rit->at(start) == ' ' || rit->at(start) == '\t')) start++;
                            size_t end = start;
                            while (end < rit->length() && rit->at(end) != ' ' && rit->at(end) != '\t' && rit->at(end) != '\r' && rit->at(end) != '\n') end++;
                            token = rit->substr(start, end - start);
                        }
                    }
                    if (!port.empty() && !token.empty()) break;
                }
            }
        }
    }

    if (!port.empty() && !token.empty()) {
        std::string rpc_url = "https://127.0.0.1:" + port + "/exa.language_server_pb.LanguageServerService/JetboxWriteState";
        std::string payload = "{\"userConfig\":{\"userSettings\":{\"themeMode\":\"" + target_mode_str +
                              "\",\"" + seed_key + "\":{\"background\":\"" + bg_hex +
                              "\",\"primary\":\"" + primary_hex +
                              "\",\"foregroundOverride\":\"" + fg_hex + "\"}}}}";

        std::string cmd = "curl -k -s --connect-timeout 1 -m 2 -X POST '" + rpc_url +
                          "' -H 'Content-Type: application/json' -H 'Connect-Protocol-Version: 1' -H 'X-Codeium-Csrf-Token: " +
                          token + "' -d '" + payload + "' >/dev/null 2>&1 &";
        std::system(cmd.c_str());
    }
}

static bool apply_scheme_preset(const std::string& scheme_id, bool dark_mode, bool show_notification = true) {
    std::string toml_path = find_schemes_toml_path();
    if (toml_path.empty()) return false;

    SchemeFullData data;
    if (!load_scheme_full_data(toml_path, scheme_id, dark_mode, data)) {
        return false;
    }

    const char* home = std::getenv("HOME");
    if (!home) return false;

    std::string current_dir = std::string(home) + "/.config/miqusettings/current";
    std::error_code ec;
    fs::create_directories(current_dir, ec);

    // Save persistent current_scheme
    {
        std::ofstream f(std::string(home) + "/.config/miqusettings/current_scheme");
        if (f.is_open()) {
            f << scheme_id << "\n";
        }
    }

    std::string mode_str = dark_mode ? "dark" : "light";

    int trans_pct = get_transparency();
    double opacity_dec = trans_pct / 100.0;
    std::ostringstream oss_dec;
    oss_dec.precision(2);
    oss_dec << std::fixed << opacity_dec;
    std::string op_dec_str = (trans_pct >= 100) ? "1.0" : (trans_pct <= 0 ? "0.0" : oss_dec.str());

    int alpha_int = (trans_pct * 255 + 50) / 100;
    char hex_buf[4];
    std::snprintf(hex_buf, sizeof(hex_buf), "%02X", alpha_int);
    std::string alpha_hex(hex_buf);

    // 1. miqutoolkit.conf
    {
        std::ofstream f(current_dir + "/miqutoolkit.conf");
        if (f.is_open()) {
            f << "# Generated by miqusettings — Scheme: " << data.display_name << " (" << mode_str << ")\n";
            f << "[colors]\n";
            const std::vector<std::string> keys = {
                "background", "on_background", "surface", "on_surface",
                "surface_variant", "on_surface_variant", "primary", "on_primary",
                "primary_container", "on_primary_container", "outline", "outline_variant", "backdrop"
            };
            for (const auto& k : keys) {
                auto it = data.miqutoolkit.find(k);
                if (it != data.miqutoolkit.end()) {
                    std::string val = it->second;
                    if (trans_pct < 100 && val.size() >= 7 && val[0] == '#') {
                        if (k == "background" || k == "surface" || k == "surface_variant" || k == "backdrop") {
                            val = val.substr(0, 7) + alpha_hex;
                        }
                    }
                    f << k << " = " << val << "\n";
                }
            }
        }
    }

    // 2. gtk.css
    {
        std::ofstream f(current_dir + "/gtk.css");
        if (f.is_open()) {
            f << "/* Generated by miqusettings — Scheme: " << data.display_name << " (" << mode_str << ") */\n";
            static const std::unordered_set<std::string> alpha_keys = {
                "window_bg_color", "theme_bg_color",
                "view_bg_color", "theme_base_color",
                "headerbar_bg_color", "headerbar_backdrop_color",
                "sidebar_bg_color", "sidebar_backdrop_color",
                "popover_bg_color", "theme_tooltip_bg_color",
                "card_bg_color", "dialog_bg_color"
            };
            for (const auto& [k, v] : data.gtk) {
                std::string out_val = v;
                if (trans_pct < 100 && v.size() >= 7 && v[0] == '#' && alpha_keys.count(k)) {
                    out_val = "alpha(" + v.substr(0, 7) + ", " + op_dec_str + ")";
                }
                f << "@define-color " << k << " " << out_val << ";\n";
            }
        }
    }

    // 3. kitty.conf
    {
        std::ofstream f(current_dir + "/kitty.conf");
        if (f.is_open()) {
            f << "# Generated by miqusettings — Scheme: " << data.display_name << " (" << mode_str << ")\n";
            f << "background_opacity " << op_dec_str << "\n";
            const std::vector<std::string> keys = {
                "background", "foreground", "cursor", "cursor_text_color",
                "selection_foreground", "selection_background",
                "active_border_color", "inactive_border_color",
                "active_tab_foreground", "active_tab_background",
                "inactive_tab_foreground", "inactive_tab_background",
                "tab_bar_background"
            };
            for (const auto& k : keys) {
                auto it = data.kitty.find(k);
                if (it != data.kitty.end()) {
                    f << k << " " << it->second << "\n";
                }
            }
            for (int i = 0; i < 16; ++i) {
                std::string ck = "color" + std::to_string(i);
                auto it = data.kitty.find(ck);
                if (it != data.kitty.end()) {
                    f << ck << " " << it->second << "\n";
                }
            }
        }
    }

    // 4. waybar.css
    {
        std::ofstream f(current_dir + "/waybar.css");
        if (f.is_open()) {
            f << "/* Generated by miqusettings — Scheme: " << data.display_name << " (" << mode_str << ") */\n";
            if (data.waybar.count("background")) {
                std::string bg = data.waybar["background"];
                if (trans_pct < 100 && bg.size() >= 7 && bg[0] == '#') {
                    f << "@define-color bar_bg alpha(" << bg.substr(0, 7) << ", " << op_dec_str << ");\n";
                } else {
                    f << "@define-color bar_bg " << bg << ";\n";
                }
            }
            if (data.waybar.count("foreground")) f << "@define-color bar_fg " << data.waybar["foreground"] << ";\n";
            if (data.waybar.count("primary")) f << "@define-color bar_primary " << data.waybar["primary"] << ";\n";
            if (data.waybar.count("accent")) f << "@define-color bar_accent " << data.waybar["accent"] << ";\n";
            if (data.waybar.count("border")) f << "@define-color bar_border " << data.waybar["border"] << ";\n";
        }
    }

    std::string scheme = dark_mode ? "prefer-dark" : "prefer-light";
    std::string opp_scheme = dark_mode ? "prefer-light" : "prefer-dark";
    std::string gtk_theme = dark_mode ? "Adwaita-dark" : "Adwaita";
    std::string opp_gtk_theme = dark_mode ? "Adwaita" : "Adwaita-dark";

    // Touch user gtk.css files so file monitors detect the updated stylesheet
    std::string touch_cmd = "touch " + std::string(home) + "/.config/gtk-3.0/gtk.css " + std::string(home) + "/.config/gtk-4.0/gtk.css 2>/dev/null; ";

    // Pulse the theme settings to force GTK 3 & 4 D-Bus reload signals across all windows
    std::string gsettings_cmd =
        touch_cmd +
        "gsettings set org.gnome.desktop.interface color-scheme '" + opp_scheme + "' 2>/dev/null; "
        "gsettings set org.gnome.desktop.interface gtk-theme '" + opp_gtk_theme + "' 2>/dev/null; "
        "sleep 0.05; "
        "gsettings set org.gnome.desktop.interface color-scheme '" + scheme + "' 2>/dev/null; "
        "gsettings set org.gnome.desktop.interface gtk-theme '" + gtk_theme + "' 2>/dev/null &";
    std::system(gsettings_cmd.c_str());

    // Restart xdg-desktop-portal-gtk daemon so file chooser dialogs immediately reflect the active theme
    std::system("systemctl --user restart xdg-desktop-portal-gtk 2>/dev/null || pkill -f xdg-desktop-portal-gtk 2>/dev/null &");

    // Live-reload running kitty terminals with updated colors and transparency
    std::system("killall -SIGUSR1 kitty 2>/dev/null &");

    // Send desktop notification via notify-send
    if (show_notification) {
        std::string notif_cmd = "notify-send -t 2000 -i preferences-desktop-theme \"Theme Applied\" \"Active Scheme: " + data.display_name + " (" + mode_str + ")\" 2>/dev/null &";
        std::system(notif_cmd.c_str());
    }

    // Update Electron / IDE theme
    update_electron_ide_theme(dark_mode, data);

    return true;
}

void ensure_active_theme_initialized() {
    const char* home = std::getenv("HOME");
    if (!home) return;
    std::string current_gtk = std::string(home) + "/.config/miqusettings/current/gtk.css";
    std::error_code ec;
    if (!fs::exists(current_gtk, ec)) {
        std::string cur_scheme = get_current_scheme_id();
        bool dark = is_dark_mode_enabled();
        apply_scheme_preset(cur_scheme, dark);
    }
}

class ColorSchemePreviewView : public View {
public:
    ColorSchemePreviewView(Color c1, Color c2, Color c3, std::string name, bool selected = false)
        : m_col1(c1), m_col2(c2), m_col3(c3), m_name(std::move(name)), m_selected(selected) {
        set_focusable(true);
        set_draw_focus_ring(false);
    }

    void set_selected(bool sel) {
        if (m_selected != sel) {
            m_selected = sel;
            request_redraw();
        }
    }
    bool is_selected() const { return m_selected; }
    const std::string& get_name() const { return m_name; }

    bool on_mouse_move(int lx, int ly, const Rect& bounds) override {
        bool in = bounds.contains(Point(lx, ly));
        if (in != m_hovered) {
            m_hovered = in;
            request_redraw();
        }
        return in;
    }

    bool on_mouse_button(int lx, int ly, MouseButton button, bool pressed, const Rect& bounds) override {
        if (!is_visible() || !is_enabled()) return false;
        if (button == MouseButton::Left && pressed && bounds.contains(Point(lx, ly))) {
            request_focus();
            if (m_on_click) {
                m_on_click();
            }
            return true;
        }
        return false;
    }

    void draw(cairo_t* cr, const Rect& bounds) override {
        if (!is_visible() || !cr || bounds.width <= 0 || bounds.height <= 0) return;
        auto config = Config::get();
        double r = get_inner_radius(config);

        int text_h = 16;
        int swatch_h = std::max(10, bounds.height - text_h - 4);
        int swatch_w = bounds.width;
        int sx = bounds.x;
        int sy = bounds.y;

        // Clip rounded rectangle for the swatch
        cairo_save(cr);
        CardView::draw_rounded_rect(cr, sx, sy, swatch_w, swatch_h, r);
        cairo_clip(cr);

        // 1. Left half (50%)
        double mid_x = sx + swatch_w / 2.0;
        cairo_rectangle(cr, sx, sy, mid_x - sx, swatch_h);
        cairo_set_source_rgba(cr, m_col1.r, m_col1.g, m_col1.b, m_col1.a);
        cairo_fill(cr);

        // 2. Right top quarter (25%)
        double mid_y = sy + swatch_h / 2.0;
        cairo_rectangle(cr, mid_x, sy, (sx + swatch_w) - mid_x, mid_y - sy);
        cairo_set_source_rgba(cr, m_col2.r, m_col2.g, m_col2.b, m_col2.a);
        cairo_fill(cr);

        // 3. Right bottom quarter (25%)
        cairo_rectangle(cr, mid_x, mid_y, (sx + swatch_w) - mid_x, (sy + swatch_h) - mid_y);
        cairo_set_source_rgba(cr, m_col3.r, m_col3.g, m_col3.b, m_col3.a);
        cairo_fill(cr);

        cairo_restore(cr);

        // Stroke border
        cairo_save(cr);
        CardView::draw_rounded_rect(cr, sx, sy, swatch_w, swatch_h, r);
        if (has_focus() || m_selected) {
            cairo_set_source_rgba(cr, config->colors.primary.r, config->colors.primary.g, config->colors.primary.b, 1.0f);
            cairo_set_line_width(cr, 2.0);
        } else if (m_hovered) {
            cairo_set_source_rgba(cr, config->colors.primary.r, config->colors.primary.g, config->colors.primary.b, 0.6f);
            cairo_set_line_width(cr, 1.5);
        } else {
            cairo_set_source_rgba(cr, config->colors.outline.r, config->colors.outline.g, config->colors.outline.b, 0.35f);
            cairo_set_line_width(cr, 1.0);
        }
        cairo_stroke(cr);
        cairo_restore(cr);

        // Name text
        if (!m_name.empty()) {
            cairo_save(cr);
            if (has_focus() || m_selected) {
                cairo_set_source_rgba(cr, config->colors.primary.r, config->colors.primary.g, config->colors.primary.b, 1.0f);
            } else {
                cairo_set_source_rgba(cr, config->colors.on_surface.r, config->colors.on_surface.g, config->colors.on_surface.b, 0.85f);
            }
            cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, (has_focus() || m_selected) ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
            cairo_set_font_size(cr, config->metrics.caption_size > 0 ? config->metrics.caption_size : 11.0);

            cairo_text_extents_t ext;
            cairo_text_extents(cr, m_name.c_str(), &ext);
            double tx = bounds.x + (bounds.width - ext.width) / 2.0 - ext.x_bearing;
            double ty = sy + swatch_h + 3 + ext.height;
            cairo_move_to(cr, tx, ty);
            cairo_show_text(cr, m_name.c_str());
            cairo_restore(cr);
        }
    }

private:
    Color m_col1, m_col2, m_col3;
    std::string m_name;
    bool m_selected = false;
    bool m_hovered = false;
};

class PlaceholderSchemeView : public View {
public:
    PlaceholderSchemeView(std::string label = "") : m_label(std::move(label)) {
        set_focusable(true);
        set_draw_focus_ring(false);
    }

    bool on_mouse_move(int lx, int ly, const Rect& bounds) override {
        bool in = bounds.contains(Point(lx, ly));
        if (in != m_hovered) {
            m_hovered = in;
            request_redraw();
        }
        return in;
    }

    bool on_mouse_button(int lx, int ly, MouseButton button, bool pressed, const Rect& bounds) override {
        if (!is_visible() || !is_enabled()) return false;
        if (button == MouseButton::Left && pressed && bounds.contains(Point(lx, ly))) {
            request_focus();
            if (m_on_click) {
                m_on_click();
            }
            return true;
        }
        return false;
    }

    void draw(cairo_t* cr, const Rect& bounds) override {
        if (!is_visible() || !cr || bounds.width <= 0 || bounds.height <= 0) return;
        auto config = Config::get();
        double r = get_inner_radius(config);

        int text_h = 16;
        int swatch_h = std::max(10, bounds.height - text_h - 4);
        int swatch_w = bounds.width;
        int sx = bounds.x;
        int sy = bounds.y;

        // Background
        cairo_save(cr);
        CardView::draw_rounded_rect(cr, sx, sy, swatch_w, swatch_h, r);
        cairo_set_source_rgba(cr, config->colors.surface_variant.r, config->colors.surface_variant.g, config->colors.surface_variant.b, 0.35f);
        cairo_fill_preserve(cr);

        // Dashed border
        const double dashes[] = {4.0, 3.0};
        cairo_set_dash(cr, dashes, 2, 0);
        if (has_focus()) {
            cairo_set_source_rgba(cr, config->colors.primary.r, config->colors.primary.g, config->colors.primary.b, 1.0f);
            cairo_set_line_width(cr, 1.8);
        } else if (m_hovered) {
            cairo_set_source_rgba(cr, config->colors.outline.r, config->colors.outline.g, config->colors.outline.b, 0.6f);
            cairo_set_line_width(cr, 1.2);
        } else {
            cairo_set_source_rgba(cr, config->colors.outline.r, config->colors.outline.g, config->colors.outline.b, 0.3f);
            cairo_set_line_width(cr, 1.0);
        }
        cairo_stroke(cr);
        cairo_restore(cr);

        // Placeholder dots inside
        cairo_save(cr);
        cairo_set_source_rgba(cr, config->colors.on_surface_variant.r, config->colors.on_surface_variant.g, config->colors.on_surface_variant.b, 0.25f);
        double cx1 = sx + swatch_w * 0.3;
        double cx2 = sx + swatch_w * 0.5;
        double cx3 = sx + swatch_w * 0.7;
        double cy = sy + swatch_h * 0.5;
        cairo_arc(cr, cx1, cy, 3.5, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_arc(cr, cx2, cy, 3.5, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_arc(cr, cx3, cy, 3.5, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_restore(cr);

        if (!m_label.empty()) {
            cairo_save(cr);
            if (has_focus()) {
                cairo_set_source_rgba(cr, config->colors.primary.r, config->colors.primary.g, config->colors.primary.b, 1.0f);
            } else {
                cairo_set_source_rgba(cr, config->colors.on_surface_variant.r, config->colors.on_surface_variant.g, config->colors.on_surface_variant.b, 0.5f);
            }
            cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, has_focus() ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
            cairo_set_font_size(cr, config->metrics.caption_size > 0 ? config->metrics.caption_size : 11.0);
            cairo_text_extents_t ext;
            cairo_text_extents(cr, m_label.c_str(), &ext);
            double tx = bounds.x + (bounds.width - ext.width) / 2.0 - ext.x_bearing;
            double ty = sy + swatch_h + 3 + ext.height;
            cairo_move_to(cr, tx, ty);
            cairo_show_text(cr, m_label.c_str());
            cairo_restore(cr);
        }
    }

private:
    std::string m_label;
    bool m_hovered = false;
};

void MillerView::render_wallpaper_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    if (!item || !item->wallpaper_cfg) return;

    // Query connected monitors
    std::vector<std::string> monitors = {"All Displays"};
    auto detected = MonitorBackend::get_monitors();
    for (const auto& mon : detected) {
        if (!mon.name.empty()) {
            monitors.push_back(mon.name);
        }
    }
    item->wallpaper_cfg->available_monitors = monitors;
    if (std::find(monitors.begin(), monitors.end(), item->wallpaper_cfg->target_monitor) == monitors.end()) {
        item->wallpaper_cfg->target_monitor = "All Displays";
    }

    // Load active wallpaper from miqubg config if user doesn't have an unapplied browse selection
    if (!item->wallpaper_cfg->has_pending_selection) {
        std::string active_path, active_mode, active_color;
        if (load_miqubg_wallpaper_config(active_path, active_mode, active_color, item->wallpaper_cfg->target_monitor)) {
            if (!active_path.empty()) {
                item->wallpaper_cfg->wallpaper_path = active_path;
                item->wallpaper_cfg->current_wallpaper = fs::path(active_path).filename().string();
            }
            if (!active_mode.empty()) {
                std::string cap = active_mode;
                if (!cap.empty()) cap[0] = std::toupper(cap[0]);
                if (cap == "Fill") cap = "Stretch";
                else if (cap == "Contain") cap = "Fit";
                item->wallpaper_cfg->fit_mode = cap;
            }
        } else if (item->wallpaper_cfg->wallpaper_path.empty() || !fs::exists(expand_wallpaper_path(item->wallpaper_cfg->wallpaper_path))) {
            for (const char* wp : {"/usr/share/miqubg/wallpaper.png", "/usr/local/share/miqubg/wallpaper.png", "shell/miqubg/assets/wallpaper.png"}) {
                if (fs::exists(wp)) {
                    item->wallpaper_cfg->wallpaper_path = wp;
                    item->wallpaper_cfg->current_wallpaper = "wallpaper.png";
                    break;
                }
            }
        }
    }

    auto main_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 16)
        ->spacing(14)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    // 1. Display Aspect Ratio Rectangle (16:9 Ratio - 320x180 px ImageView)
    std::string expanded_path = expand_wallpaper_path(item->wallpaper_cfg->wallpaper_path);

    auto img_view = ImageViewBuilder::create()
        ->source(expanded_path)
        ->fitMode(wallpaper_str_to_fit_mode(item->wallpaper_cfg->fit_mode))
        ->cornerRadius(get_inner_radius(config))
        ->border(1, config->colors.outline.with_alpha(0.35f))
        ->backgroundColor(config->colors.surface_variant.with_alpha(0.45f))
        ->build();

    LayoutParams d_lp;
    d_lp.width = 320;
    d_lp.height = 180;
    d_lp.gravity = Gravity::CenterHorizontal;
    main_card->add_view(img_view, d_lp);

    // Current wallpaper filename label
    std::string display_name = item->wallpaper_cfg->current_wallpaper.empty()
        ? "No wallpaper selected"
        : item->wallpaper_cfg->current_wallpaper;
    auto wp_name = TextViewBuilder::create()
        ->text(display_name)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->center(true)
        ->build();
    LayoutParams name_lp(static_cast<int>(LayoutDimension::MatchParent), static_cast<int>(LayoutDimension::WrapContent));
    name_lp.gravity = Gravity::CenterHorizontal;
    main_card->add_view(wp_name, name_lp);

    // 2. Bottom Row: Monitor Spinner, Browse Button, Reset Button, Fit Mode Spinner, Apply Button (Centered)
    auto bottom_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->gravity(Gravity::Center)
        ->spacing(10)
        ->build();

    // Monitor Selector Spinner
    int cur_mon_idx = 0;
    for (size_t i = 0; i < item->wallpaper_cfg->available_monitors.size(); ++i) {
        if (item->wallpaper_cfg->available_monitors[i] == item->wallpaper_cfg->target_monitor) {
            cur_mon_idx = static_cast<int>(i);
            break;
        }
    }

    auto mon_spinner = SpinnerBuilder::create()
        ->items(item->wallpaper_cfg->available_monitors)
        ->selectedIndex(cur_mon_idx)
        ->fontSize(config->metrics.font_size)
        ->padding(12, 6)
        ->cornerRadius(get_btn_radius(config))
        ->onItemSelected([this, item](int idx, const std::string& val) {
            (void)idx;
            item->wallpaper_cfg->target_monitor = val;
            item->wallpaper_cfg->has_pending_selection = false;
            std::string mon_path, mon_mode, mon_color;
            if (load_miqubg_wallpaper_config(mon_path, mon_mode, mon_color, val)) {
                if (!mon_path.empty()) {
                    item->wallpaper_cfg->wallpaper_path = mon_path;
                    item->wallpaper_cfg->current_wallpaper = fs::path(mon_path).filename().string();
                }
                if (!mon_mode.empty()) {
                    std::string cap = mon_mode;
                    if (!cap.empty()) cap[0] = std::toupper(cap[0]);
                    if (cap == "Fill") cap = "Stretch";
                    else if (cap == "Contain") cap = "Fit";
                    item->wallpaper_cfg->fit_mode = cap;
                }
            }
            item->last_action_message = "";
            rebuild_preview_column();
            notify_state_changed();
        })
        ->build();
    LayoutParams m_lp(static_cast<int>(LayoutDimension::WrapContent), 36);
    m_lp.gravity = Gravity::CenterVertical;
    bottom_row->add_view(mon_spinner, m_lp);

    auto on_browse = [this, item]() {
        std::thread([this, item]() {
            std::string cmd;
            if (system("which zenity >/dev/null 2>&1") == 0) {
                cmd = "zenity --file-selection --title=\"Select Wallpaper\" "
                      "--file-filter=\"Image files | *.png *.jpg *.jpeg *.webp *.svg *.bmp *.avif *.jxl\" "
                      "--file-filter=\"All files | *\" 2>/dev/null";
            } else if (system("which kdialog >/dev/null 2>&1") == 0) {
                cmd = "kdialog --getopenfilename : \"Images (*.png *.jpg *.jpeg *.webp *.svg *.bmp *.avif *.jxl)\" 2>/dev/null";
            }
            if (cmd.empty()) return;

            FILE* fp = popen(cmd.c_str(), "r");
            std::string selected_path;
            if (fp) {
                char buf[2048];
                if (fgets(buf, sizeof(buf), fp) != nullptr) {
                    selected_path = buf;
                }
                pclose(fp);
            }
            while (!selected_path.empty() && (selected_path.back() == '\n' || selected_path.back() == '\r' || selected_path.back() == ' ')) {
                selected_path.pop_back();
            }
            if (!selected_path.empty() && fs::exists(selected_path)) {
                if (auto* eng = AppEngine::instance()) {
                    eng->post_delayed(0, [this, item, selected_path]() {
                        item->wallpaper_cfg->wallpaper_path = selected_path;
                        item->wallpaper_cfg->current_wallpaper = fs::path(selected_path).filename().string();
                        item->wallpaper_cfg->has_pending_selection = true;
                        item->last_action_message = "";
                        rebuild_preview_column();
                        notify_state_changed();
                    });
                }
            }
        }).detach();
    };

    auto browse_btn = ButtonBuilder::create()
        ->text("Browse")
        ->style(ButtonStyle::Outlined)
        ->padding(14, 6)
        ->cornerRadius(get_btn_radius(config))
        ->onClick(on_browse)
        ->build();

    LayoutParams b_lp(static_cast<int>(LayoutDimension::WrapContent), 36);
    b_lp.gravity = Gravity::CenterVertical;
    bottom_row->add_view(browse_btn, b_lp);

    if (item->wallpaper_cfg->has_pending_selection) {
        auto reset_btn = ButtonBuilder::create()
            ->text("Reset")
            ->style(ButtonStyle::Outlined)
            ->padding(14, 6)
            ->cornerRadius(get_btn_radius(config))
            ->onClick([this, item]() {
                item->wallpaper_cfg->has_pending_selection = false;
                item->last_action_message = "";
                rebuild_preview_column();
                notify_state_changed();
            })
            ->build();
        LayoutParams r_lp(static_cast<int>(LayoutDimension::WrapContent), 36);
        r_lp.gravity = Gravity::CenterVertical;
        bottom_row->add_view(reset_btn, r_lp);
    }

    int cur_fit_idx = 0;
    for (size_t i = 0; i < item->wallpaper_cfg->fit_modes.size(); ++i) {
        if (item->wallpaper_cfg->fit_modes[i] == item->wallpaper_cfg->fit_mode) {
            cur_fit_idx = static_cast<int>(i);
            break;
        }
    }

    auto fit_spinner = SpinnerBuilder::create()
        ->items(item->wallpaper_cfg->fit_modes)
        ->selectedIndex(cur_fit_idx)
        ->fontSize(config->metrics.font_size)
        ->padding(12, 6)
        ->cornerRadius(get_btn_radius(config))
        ->onItemSelected([item, img_view](int idx, const std::string& val) {
            (void)idx;
            item->wallpaper_cfg->fit_mode = val;
            if (img_view) {
                img_view->set_fit_mode(wallpaper_str_to_fit_mode(val));
            }
        })
        ->build();
    LayoutParams s_lp(static_cast<int>(LayoutDimension::WrapContent), 36);
    s_lp.gravity = Gravity::CenterVertical;
    bottom_row->add_view(fit_spinner, s_lp);

    auto on_apply = [this, item]() {
        std::string expanded = expand_wallpaper_path(item->wallpaper_cfg->wallpaper_path);
        if (!expanded.empty() && fs::exists(expanded)) {
            bool ok = sync_miqubg_wallpaper_config(expanded, item->wallpaper_cfg->fit_mode, "#000000", item->wallpaper_cfg->target_monitor);
            if (ok) {
                item->wallpaper_cfg->has_pending_selection = false;
                std::string target_desc = (item->wallpaper_cfg->target_monitor.empty() || item->wallpaper_cfg->target_monitor == "All Displays")
                    ? "all displays" : item->wallpaper_cfg->target_monitor;
                item->last_action_message = "Wallpaper applied to " + target_desc + " & saved to miqubg.conf";
            } else {
                item->last_action_message = "Failed to write miqubg configuration";
            }
        } else {
            item->last_action_message = "Image not found: " + item->wallpaper_cfg->current_wallpaper;
        }
        rebuild_preview_column();
        notify_state_changed();
    };

    auto apply_btn = ButtonBuilder::create()
        ->text("Apply")
        ->primary(true)
        ->padding(16, 6)
        ->cornerRadius(get_btn_radius(config))
        ->onClick(on_apply)
        ->build();

    LayoutParams a_lp(static_cast<int>(LayoutDimension::WrapContent), 36);
    a_lp.gravity = Gravity::CenterVertical;
    bottom_row->add_view(apply_btn, a_lp);

    LayoutParams row_lp(static_cast<int>(LayoutDimension::WrapContent), static_cast<int>(LayoutDimension::WrapContent));
    row_lp.gravity = Gravity::CenterHorizontal;
    main_card->add_view(bottom_row, row_lp);

    if (!item->last_action_message.empty()) {
        auto fb = TextViewBuilder::create()
            ->text(item->last_action_message)
            ->caption(true)
            ->textColor(config->colors.primary)
            ->center(true)
            ->build();
        LayoutParams fb_lp(static_cast<int>(LayoutDimension::MatchParent), static_cast<int>(LayoutDimension::WrapContent));
        fb_lp.gravity = Gravity::CenterHorizontal;
        main_card->add_view(fb, fb_lp);
    }

    add_preview_card(main_card);

    // ==============================================================================
    // 2. Dark Section Card
    // ==============================================================================
    auto dark_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 16)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto dark_header = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->gravity(Gravity::CenterVertical)
        ->spacing(8)
        ->build();

    auto dark_icon = ImageViewBuilder::create()
        ->source("weather-clear-night")
        ->targetSize(20)
        ->fitMode(FitMode::Contain)
        ->build();
    dark_header->add_view(dark_icon, LayoutParams(20, 20, Gravity::CenterVertical));

    auto dark_title = TextViewBuilder::create()
        ->text("Dark")
        ->textSize(static_cast<int>(config->metrics.font_size * 1.15f))
        ->bold(true)
        ->textColor(config->colors.on_surface)
        ->build();
    dark_header->add_view(dark_title);

    dark_card->add_view(dark_header);

    auto dark_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->gravity(Gravity::CenterVertical)
        ->padding(0, 4)
        ->spacing(12)
        ->build();

    auto dark_label = TextViewBuilder::create()
        ->text("Dark Mode")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    LayoutParams lbl_lp(static_cast<int>(LayoutDimension::MatchParent), static_cast<int>(LayoutDimension::WrapContent));
    lbl_lp.weight = 1.0f;
    lbl_lp.gravity = Gravity::CenterVertical;
    dark_row->add_view(dark_label, lbl_lp);

    bool is_dark = is_dark_mode_enabled();
    auto dark_switch = SwitchBuilder::create()
        ->checked(is_dark)
        ->build();

    dark_switch->set_on_checked_changed_listener([this, item](bool checked) {
        set_dark_mode_enabled(checked);
        std::string cur_scheme = get_current_scheme_id();
        apply_scheme_preset(cur_scheme, checked);
        item->last_action_message = checked ? "Dark mode enabled" : "Light mode enabled";
        rebuild_preview_column();
        notify_state_changed();
    });

    LayoutParams sw_lp(44, 24);
    sw_lp.gravity = Gravity::CenterVertical;
    dark_row->add_view(dark_switch, sw_lp);

    dark_card->add_view(dark_row);

    add_preview_card(dark_card);

    // ==============================================================================
    // 3. Style Section Card
    // ==============================================================================
    auto style_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 16)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto style_header = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->gravity(Gravity::CenterVertical)
        ->spacing(8)
        ->build();

    auto style_icon = ImageViewBuilder::create()
        ->source("preferences-desktop-theme")
        ->targetSize(20)
        ->fitMode(FitMode::Contain)
        ->build();
    style_header->add_view(style_icon, LayoutParams(20, 20, Gravity::CenterVertical));

    auto style_title = TextViewBuilder::create()
        ->text("Style")
        ->textSize(static_cast<int>(config->metrics.font_size * 1.15f))
        ->bold(true)
        ->textColor(config->colors.on_surface)
        ->build();
    style_header->add_view(style_title);

    style_card->add_view(style_header);

    // Segmented TabBar for switching between Preset Color and Wallpaper Color
    auto tab_bar = TabBarBuilder::create()
        ->tabs({"Preset Color", "Wallpaper Color"})
        ->selectedIndex(0)
        ->style(TabBarStyle::Segmented)
        ->equalWidths(true)
        ->build();

    LayoutParams tab_lp(300, 32);
    tab_lp.gravity = Gravity::CenterHorizontal;
    style_card->add_view(tab_bar, tab_lp);

    // Page 0: Preset Color
    auto preset_page = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(8)
        ->gravity(Gravity::Center)
        ->padding(2, 4)
        ->build();

    auto row1 = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(8)
        ->gravity(Gravity::Center)
        ->build();

    auto row2 = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(8)
        ->gravity(Gravity::Center)
        ->build();

    auto preset_schemes = load_active_preset_schemes();
    std::string cur_scheme = get_current_scheme_id();
    for (size_t i = 0; i < preset_schemes.size(); ++i) {
        if (preset_schemes[i].id == cur_scheme) {
            s_selected_scheme_idx = static_cast<int>(i);
            break;
        }
    }

    const char* home_env = std::getenv("HOME");
    if (home_env) {
        std::string current_gtk = std::string(home_env) + "/.config/miqusettings/current/gtk.css";
        std::error_code ec;
        if (!fs::exists(current_gtk, ec)) {
            bool dark = is_dark_mode_enabled();
            apply_scheme_preset(cur_scheme, dark);
        }
    }

    size_t split_idx = (preset_schemes.size() + 1) / 2;
    int swatch_w = preset_schemes.size() <= 6 ? 85 : 70;

    auto scheme_views = std::make_shared<std::vector<std::shared_ptr<ColorSchemePreviewView>>>();

    for (size_t i = 0; i < preset_schemes.size(); ++i) {
        const auto& s = preset_schemes[i];
        auto sv = std::make_shared<ColorSchemePreviewView>(
            s.primary, s.secondary, s.tertiary, s.name, static_cast<int>(i) == s_selected_scheme_idx
        );
        scheme_views->push_back(sv);

        sv->set_on_click_listener([this, item, i, scheme_views, preset_schemes]() {
            s_selected_scheme_idx = static_cast<int>(i);
            for (size_t j = 0; j < scheme_views->size(); ++j) {
                (*scheme_views)[j]->set_selected(static_cast<int>(j) == s_selected_scheme_idx);
            }
            bool dark = is_dark_mode_enabled();
            apply_scheme_preset(preset_schemes[i].id, dark);
            item->last_action_message = "Preset applied: " + preset_schemes[i].name;
            rebuild_preview_column();
            notify_state_changed();
        });

        if (i < split_idx) {
            row1->add_view(sv, LayoutParams(swatch_w, 54));
        } else {
            row2->add_view(sv, LayoutParams(swatch_w, 54));
        }
    }

    preset_page->add_view(row1, LayoutParams(static_cast<int>(LayoutDimension::WrapContent), 54, Gravity::CenterHorizontal));
    preset_page->add_view(row2, LayoutParams(static_cast<int>(LayoutDimension::WrapContent), 54, Gravity::CenterHorizontal));

    // Page 1: Wallpaper Color (Placeholder grid + coming soon message)
    auto wp_page = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(12)
        ->gravity(Gravity::Center)
        ->padding(4, 4)
        ->build();

    auto wp_placeholder_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(8)
        ->gravity(Gravity::Center)
        ->build();

    for (int i = 1; i <= 4; ++i) {
        auto ph = std::make_shared<PlaceholderSchemeView>("Scheme " + std::to_string(i));
        ph->set_on_click_listener([this, item, i]() {
            item->last_action_message = "Wallpaper color extraction coming soon (Scheme " + std::to_string(i) + ")";
            rebuild_preview_column();
            notify_state_changed();
        });
        wp_placeholder_row->add_view(ph, LayoutParams(70, 54));
    }
    wp_page->add_view(wp_placeholder_row, LayoutParams(static_cast<int>(LayoutDimension::WrapContent), 54, Gravity::CenterHorizontal));

    auto coming_soon_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->gravity(Gravity::Center)
        ->spacing(2)
        ->build();

    auto cs_title = TextViewBuilder::create()
        ->text("Coming soon")
        ->bold(true)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.primary)
        ->center(true)
        ->build();
    coming_soon_box->add_view(cs_title);

    auto cs_desc = TextViewBuilder::create()
        ->text("Dynamic palette extraction from current wallpaper")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->center(true)
        ->build();
    coming_soon_box->add_view(cs_desc);

    wp_page->add_view(coming_soon_box, LayoutParams(static_cast<int>(LayoutDimension::MatchParent), static_cast<int>(LayoutDimension::WrapContent), Gravity::CenterHorizontal));

    // Inactive page is hidden so ViewGroup::collect_focusable_views only collects active page items
    wp_page->set_visibility(Visibility::Gone);

    // ViewPager with the two pages
    auto view_pager = ViewPagerBuilder::create()
        ->addPage(preset_page)
        ->addPage(wp_page)
        ->currentPage(0)
        ->build();

    tab_bar->set_on_tab_selected_listener([view_pager, preset_page, wp_page](int idx) {
        view_pager->set_current_page(idx);
        preset_page->set_visibility(idx == 0 ? Visibility::Visible : Visibility::Gone);
        wp_page->set_visibility(idx == 1 ? Visibility::Visible : Visibility::Gone);
    });

    view_pager->set_on_page_changed_listener([tab_bar, preset_page, wp_page](int old_idx, int new_idx) {
        (void)old_idx;
        tab_bar->set_selected_index(new_idx);
        preset_page->set_visibility(new_idx == 0 ? Visibility::Visible : Visibility::Gone);
        wp_page->set_visibility(new_idx == 1 ? Visibility::Visible : Visibility::Gone);
    });

    LayoutParams vp_lp(static_cast<int>(LayoutDimension::MatchParent), 135);
    style_card->add_view(view_pager, vp_lp);

    add_preview_card(style_card);

    // Window Transparency Card
    int curr_transparency = get_transparency();

    auto trans_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto trans_header = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->gravity(Gravity::CenterVertical)
        ->spacing(8)
        ->build();

    auto trans_icon = ImageViewBuilder::create()
        ->source("view-opacity-symbolic")
        ->targetSize(20)
        ->fitMode(FitMode::Contain)
        ->build();
    trans_header->add_view(trans_icon, LayoutParams(20, 20, Gravity::CenterVertical));

    auto trans_title = TextViewBuilder::create()
        ->text("Window Transparency")
        ->textSize(static_cast<int>(config->metrics.font_size * 1.15f))
        ->bold(true)
        ->textColor(config->colors.on_surface)
        ->build();

    auto trans_title_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->gravity(Gravity::CenterVertical)
        ->build();
    LayoutParams lp_trans_title(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f);
    lp_trans_title.gravity = Gravity::CenterVertical;
    trans_title_box->add_view(trans_title);
    trans_header->add_view(trans_title_box, lp_trans_title);

    auto transparency_badge = TextViewBuilder::create()
        ->text(std::to_string(curr_transparency) + "%")
        ->bold(true)
        ->textColor(config->colors.primary)
        ->build();
    trans_header->add_view(transparency_badge);

    trans_card->add_view(trans_header);

    auto transparency_slider = SliderBuilder::create()
        ->value(curr_transparency / 100.0f)
        ->onValueChanged([this, item, transparency_badge](float val, bool from_user) {
            int pct = std::clamp(static_cast<int>(std::round(val * 100.0f)), 0, 100);
            if (transparency_badge) {
                transparency_badge->set_text(std::to_string(pct) + "%");
                transparency_badge->request_redraw();
            }
            if (from_user) {
                set_transparency(pct);
                std::string cur_scheme = get_current_scheme_id();
                bool dark = is_dark_mode_enabled();
                apply_scheme_preset(cur_scheme, dark, false);
                item->last_action_message = "Transparency set: " + std::to_string(pct) + "%";
                notify_state_changed();
            }
        })
        ->build();
    LayoutParams slider_lp(static_cast<int>(LayoutDimension::MatchParent), 28);
    trans_card->add_view(transparency_slider, slider_lp);

    add_preview_card(trans_card);
}

namespace {

struct ParsedKeybinding {
    std::string action_title;
    std::string description;
    std::string command;
    std::vector<std::string> keys;
};

struct KeybindingsCategorySection {
    std::string title;
    std::vector<ParsedKeybinding> bindings;
};

static std::string trim_str(std::string s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) s.pop_back();
    return s;
}

static void resolve_keybinding_info(const std::string& action, std::string& title, std::string& desc) {
    if (action == "miqupower") { title = "Power & Session Dialog"; desc = "Open system shutdown, restart and suspend dialog"; }
    else if (action == "shutdown") { title = "Graceful Shutdown"; desc = "Closes open windows and exits miquland safely"; }
    else if (action == "exit") { title = "Immediate Exit"; desc = "Forced immediate exit from current session"; }
    else if (action == "miquctl reload") { title = "Reload Compositor"; desc = "Reload configuration and dynamic theme files without restart"; }
    else if (action == "miqulock") { title = "Lock Screen"; desc = "Lock session immediately via Wayland lock protocol"; }
    else if (action == "overview_toggle") { title = "Toggle Overview"; desc = "Interactive live workspace expose and window switcher"; }
    else if (action == "kitty --single-instance") { title = "Terminal Emulator"; desc = "Spawn default terminal emulator (Kitty)"; }
    else if (action == "miqulauncher -list") { title = "Application Launcher"; desc = "Search and launch desktop applications"; }
    else if (action == "miqulauncher -show search") { title = "Universal Search"; desc = "Search commands, web, and system actions"; }
    else if (action == "miqusettings") { title = "Control Center"; desc = "System settings & control center"; }
    else if (action == "miqunotify -t") { title = "Notification Center"; desc = "Toggle slide-out notification drawer and history"; }
    else if (action == "/usr/lib/firefox/firefox") { title = "Web Browser"; desc = "Launch default web browser (Firefox)"; }
    else if (action == "kitty -e yazi") { title = "File Manager"; desc = "Open terminal file manager (Yazi)"; }
    else if (action == "close") { title = "Close Window"; desc = "Gracefully close the currently active focused window"; }
    else if (action == "toggle_floating") { title = "Toggle Floating"; desc = "Toggle window between floating and tiled layout modes"; }
    else if (action == "toggle_fullscreen") { title = "Toggle Fullscreen"; desc = "Expand window to fill display area"; }
    else if (action == "toggle_layout") { title = "Toggle Layout"; desc = "Cycle through available tiling layouts (Dwindle, Stack, etc.)"; }
    else if (action == "swap_main") { title = "Swap with Master"; desc = "Promote active window to dominant master position"; }
    else if (action == "toggle_split") { title = "Toggle Split"; desc = "Toggle horizontal or vertical binary space partitioning"; }
    else if (action == "next_window") { title = "Next Window"; desc = "Shift keyboard focus to next window in current workspace"; }
    else if (action == "prev_window") { title = "Previous Window"; desc = "Shift keyboard focus to previous window"; }
    else if (action == "focus_win_1") { title = "Focus Window 1"; desc = "Jump focus directly to window 1"; }
    else if (action == "focus_win_2") { title = "Focus Window 2"; desc = "Jump focus directly to window 2"; }
    else if (action == "prev_ws") { title = "Previous Workspace"; desc = "Switch to adjacent previous workspace"; }
    else if (action == "next_ws") { title = "Next Workspace"; desc = "Switch to adjacent next workspace"; }
    else if (action.rfind("ws_", 0) == 0) {
        std::string num = action.substr(3);
        title = "Switch to Workspace " + num;
        desc = "Jump directly to numbered workspace " + num;
    }
    else if (action.rfind("move_ws_", 0) == 0) {
        std::string num = action.substr(8);
        title = "Move Window to Workspace " + num;
        desc = "Send focused window to numbered workspace " + num;
    }
    else if (action == "playerctl play-pause") { title = "Play / Pause"; desc = "Toggle media playback"; }
    else if (action == "playerctl next") { title = "Next Track"; desc = "Skip to next media track"; }
    else if (action == "playerctl previous") { title = "Previous Track"; desc = "Return to previous media track"; }
    else if (action.rfind("wpctl set-volume", 0) == 0 && action.find("5%+") != std::string::npos) {
        title = "Volume Up (+5%)"; desc = "Increase default audio sink volume";
    }
    else if (action.rfind("wpctl set-volume", 0) == 0 && action.find("5%-") != std::string::npos) {
        title = "Volume Down (-5%)"; desc = "Decrease default audio sink volume";
    }
    else if (action.rfind("wpctl set-mute", 0) == 0 && action.find("DEFAULT_AUDIO_SINK") != std::string::npos) {
        title = "Mute Output"; desc = "Toggle master output speaker mute";
    }
    else if (action.rfind("wpctl set-mute", 0) == 0 && action.find("DEFAULT_AUDIO_SOURCE") != std::string::npos) {
        title = "Mute Microphone"; desc = "Toggle input microphone mute";
    }
    else if (action.find("brightnessctl set +") != std::string::npos) {
        title = "Brightness Up"; desc = "Increase display backlight level";
    }
    else if (action.find("brightnessctl set ") != std::string::npos && action.find("%-") != std::string::npos) {
        title = "Brightness Down"; desc = "Decrease display backlight level";
    }
    else if (action.find("slurp | grim") != std::string::npos) {
        title = "Interactive Screenshot"; desc = "Interactive region selection capture saved to ~/Pictures";
    }
    else if (action.find("grim") != std::string::npos) {
        title = "Fullscreen Screenshot"; desc = "Full display capture saved to ~/Pictures";
    }
    else if (action.find("gamma toggle") != std::string::npos) {
        title = "Night Light Toggle"; desc = "Toggle blue light filter warmth";
    }
    else {
        title = action;
        desc = "Execute: " + action;
    }
}

static std::vector<KeybindingsCategorySection> parse_keybindings_file() {
    std::vector<KeybindingsCategorySection> sections;
    const char* home = getenv("HOME");
    std::string path;
    if (home) {
        fs::path user_conf = fs::path(home) / ".config" / "miquland" / "keybindings.conf";
        std::error_code ec;
        if (fs::exists(user_conf, ec)) path = user_conf.string();
    }
    if (path.empty()) {
        for (const char* p : {"/usr/share/miquland/keybindings.conf", "/usr/local/share/miquland/keybindings.conf", "assets/keybindings.conf"}) {
            if (fs::exists(p)) {
                path = p;
                break;
            }
        }
    }
    if (path.empty()) return sections;

    std::ifstream in(path);
    if (!in.is_open()) return sections;

    std::string line;
    std::string current_section_name = "General Shortcuts";
    std::vector<ParsedKeybinding> current_items;

    auto flush_section = [&]() {
        if (!current_items.empty()) {
            sections.push_back({current_section_name, current_items});
            current_items.clear();
        }
    };

    while (std::getline(in, line)) {
        std::string t = trim_str(line);
        if (t.empty()) continue;

        if (t.rfind("# ---", 0) == 0) {
            size_t end_dashes = t.find("---", 5);
            if (end_dashes != std::string::npos) {
                std::string sec_name = trim_str(t.substr(5, end_dashes - 5));
                if (!sec_name.empty()) {
                    flush_section();
                    current_section_name = sec_name;
                    continue;
                }
            }
        }

        if (t.rfind("bind = ", 0) == 0) {
            std::string content = trim_str(t.substr(7));
            size_t comma = content.find(',');
            if (comma != std::string::npos) {
                std::string combo_str = trim_str(content.substr(0, comma));
                std::string action_str = trim_str(content.substr(comma + 1));
                if (!combo_str.empty() && !action_str.empty()) {
                    ParsedKeybinding pb;
                    pb.command = action_str;
                    resolve_keybinding_info(action_str, pb.action_title, pb.description);

                    std::stringstream ss(combo_str);
                    std::string key_part;
                    while (std::getline(ss, key_part, '+')) {
                        std::string kp = trim_str(key_part);
                        if (!kp.empty()) pb.keys.push_back(kp);
                    }
                    if (pb.keys.empty()) pb.keys.push_back(combo_str);
                    current_items.push_back(pb);
                }
            }
        }
    }
    flush_section();
    return sections;
}

struct ParsedGesture {
    std::string pattern;
    int fingers = 3;
    std::string direction = "Left";
    std::string arrow = "←";
    std::string action_title;
    std::string description;
    std::string command;
};

struct GestureCategorySection {
    std::string title;
    std::vector<ParsedGesture> gestures;
};

static void resolve_gesture_info(const std::string& action, std::string& title, std::string& desc) {
    if (action == "next_ws") { title = "Next Workspace"; desc = "Switch to adjacent next workspace in order"; }
    else if (action == "prev_ws") { title = "Previous Workspace"; desc = "Switch to adjacent previous workspace in order"; }
    else if (action == "toggle_fullscreen") { title = "Toggle Fullscreen"; desc = "Expand active window to fill entire display"; }
    else if (action == "toggle_floating") { title = "Toggle Floating"; desc = "Toggle active window between floating and tiled layout"; }
    else if (action == "miqulauncher -list") { title = "Application Launcher"; desc = "Open desktop application launcher applet"; }
    else if (action == "miqulock") { title = "Lock Screen"; desc = "Lock Wayland compositor session immediately"; }
    else if (action == "overview_toggle") { title = "Workspace Overview"; desc = "Interactive live window expose and workspace switcher"; }
    else {
        title = action;
        desc = "Trigger command: " + action;
    }
}

static std::vector<GestureCategorySection> parse_gestures_file() {
    std::vector<GestureCategorySection> sections;
    const char* home = getenv("HOME");
    std::string path;
    if (home) {
        fs::path user_conf = fs::path(home) / ".config" / "miquland" / "gesture_binding.conf";
        std::error_code ec;
        if (fs::exists(user_conf, ec)) path = user_conf.string();
    }
    if (path.empty()) {
        for (const char* p : {"/usr/share/miquland/gesture_binding.conf", "/usr/local/share/miquland/gesture_binding.conf", "assets/gesture_binding.conf"}) {
            if (fs::exists(p)) {
                path = p;
                break;
            }
        }
    }
    if (path.empty()) return sections;

    std::ifstream in(path);
    if (!in.is_open()) return sections;

    std::string line;
    std::string current_section_name = "Touchpad Gestures";
    std::vector<ParsedGesture> current_items;

    auto flush_section = [&]() {
        if (!current_items.empty()) {
            sections.push_back({current_section_name, current_items});
            current_items.clear();
        }
    };

    while (std::getline(in, line)) {
        std::string t = trim_str(line);
        if (t.empty()) continue;

        if (t.rfind("# ---", 0) == 0) {
            size_t end_dashes = t.find("---", 5);
            if (end_dashes != std::string::npos) {
                std::string sec_name = trim_str(t.substr(5, end_dashes - 5));
                if (!sec_name.empty()) {
                    flush_section();
                    current_section_name = sec_name;
                    continue;
                }
            }
        }

        if (t.rfind("gesture = ", 0) == 0) {
            std::string content = trim_str(t.substr(10));
            size_t comma = content.find(',');
            if (comma != std::string::npos) {
                std::string pat = trim_str(content.substr(0, comma));
                std::string act = trim_str(content.substr(comma + 1));
                if (!pat.empty() && !act.empty()) {
                    ParsedGesture pg;
                    pg.pattern = pat;
                    pg.command = act;
                    resolve_gesture_info(act, pg.action_title, pg.description);

                    if (pat.rfind("swipe:", 0) == 0) {
                        std::string rest = pat.substr(6);
                        size_t c = rest.find(':');
                        if (c != std::string::npos) {
                            try {
                                pg.fingers = std::stoi(rest.substr(0, c));
                            } catch (...) {
                                pg.fingers = 3;
                            }
                            std::string dir = rest.substr(c + 1);
                            if (dir == "left") { pg.direction = "Swipe Left"; pg.arrow = "←"; }
                            else if (dir == "right") { pg.direction = "Swipe Right"; pg.arrow = "→"; }
                            else if (dir == "up") { pg.direction = "Swipe Up"; pg.arrow = "↑"; }
                            else if (dir == "down") { pg.direction = "Swipe Down"; pg.arrow = "↓"; }
                            else { pg.direction = dir; pg.arrow = "•"; }
                        }
                    }
                    current_items.push_back(pg);
                }
            }
        }
    }
    flush_section();
    return sections;
}

} // anonymous namespace

void MillerView::render_keybindings_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    if (!item) return;

    // Header info banner card
    auto header_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 16)
        ->spacing(4)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto h_title = TextViewBuilder::create()
        ->text("Keyboard Shortcuts & Hotkeys")
        ->textSize(config->metrics.font_size + 2)
        ->textColor(config->colors.on_surface)
        ->build();
    header_card->add_view(h_title);

    auto h_sub = TextViewBuilder::create()
        ->text("Global shortcuts configured in ~/.config/miquland/keybindings.conf. Hotkeys are handled directly by the compositor core.")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    header_card->add_view(h_sub);

    add_preview_card(header_card);

    // Read and parse sections directly from keybindings.conf
    std::vector<KeybindingsCategorySection> sections = parse_keybindings_file();

    // Fallback to static item keybindings if file parsing yielded nothing
    if (sections.empty() && !item->keybindings.empty()) {
        KeybindingsCategorySection s;
        s.title = item->title;
        for (const auto& kb : item->keybindings) {
            ParsedKeybinding pb;
            pb.action_title = kb.action;
            pb.description = kb.description;
            pb.keys = kb.keys;
            s.bindings.push_back(pb);
        }
        sections.push_back(s);
    }

    for (const auto& sec : sections) {
        auto sec_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->padding(16, 16)
            ->spacing(10)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(get_card_radius(config))
            ->build();

        auto sec_header = TextViewBuilder::create()
            ->text(sec.title)
            ->caption(true)
            ->textColor(config->colors.primary)
            ->build();
        sec_card->add_view(sec_header);

        for (size_t i = 0; i < sec.bindings.size(); ++i) {
            const auto& kb = sec.bindings[i];

            if (i > 0) {
                auto divider = LinearLayoutBuilder::create()
                    ->backgroundColor(config->colors.outline.with_alpha(0.10f))
                    ->build();
                LayoutParams dlp;
                dlp.width = static_cast<int>(LayoutDimension::MatchParent);
                dlp.height = 1;
                sec_card->add_view(divider, dlp);
            }

            auto row = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->gravity(Gravity::CenterVertical)
                ->spacing(12)
                ->build();

            auto text_box = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->spacing(2)
                ->build();

            auto act_lbl = TextViewBuilder::create()
                ->text(kb.action_title)
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.on_surface)
                ->build();
            text_box->add_view(act_lbl);

            if (!kb.description.empty()) {
                auto desc_lbl = TextViewBuilder::create()
                    ->text(kb.description)
                    ->caption(true)
                    ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
                    ->build();
                text_box->add_view(desc_lbl);
            }

            LayoutParams tb_lp;
            tb_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            tb_lp.height = static_cast<int>(LayoutDimension::WrapContent);
            tb_lp.weight = 1.0f;
            tb_lp.gravity = Gravity::CenterVertical;
            row->add_view(text_box, tb_lp);

            auto keys_box = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->gravity(Gravity::CenterVertical)
                ->spacing(4)
                ->build();

            for (size_t k = 0; k < kb.keys.size(); ++k) {
                if (k > 0) {
                    auto plus_lbl = TextViewBuilder::create()
                        ->text("+")
                        ->caption(true)
                        ->textColor(config->colors.on_surface_variant.with_alpha(0.6f))
                        ->build();
                    LayoutParams p_lp;
                    p_lp.width = static_cast<int>(LayoutDimension::WrapContent);
                    p_lp.gravity = Gravity::CenterVertical;
                    keys_box->add_view(plus_lbl, p_lp);
                }

                auto key_badge = CardViewBuilder::create()
                    ->style(CardStyle::Filled)
                    ->backgroundColor(config->colors.surface_variant.with_alpha(0.6f))
                    ->stroke(1, config->colors.outline.with_alpha(0.35f))
                    ->cornerRadius(get_btn_radius(config))
                    ->elevation(0)
                    ->addView(
                        TextViewBuilder::create()
                            ->text(kb.keys[k])
                            ->caption(true)
                            ->textColor(config->colors.primary)
                            ->padding(8, 4)
                            ->build()
                    )
                    ->build();

                LayoutParams k_lp;
                k_lp.width = static_cast<int>(LayoutDimension::WrapContent);
                k_lp.height = static_cast<int>(LayoutDimension::WrapContent);
                k_lp.gravity = Gravity::CenterVertical;
                keys_box->add_view(key_badge, k_lp);
            }

            LayoutParams r_lp;
            r_lp.width = static_cast<int>(LayoutDimension::WrapContent);
            r_lp.height = static_cast<int>(LayoutDimension::WrapContent);
            r_lp.gravity = Gravity::CenterVertical;
            row->add_view(keys_box, r_lp);

            sec_card->add_view(row);
        }

        add_preview_card(sec_card);
    }
}

void MillerView::render_gesture_bindings_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    if (!item) return;

    // Header info banner card
    auto header_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 16)
        ->spacing(4)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto h_title = TextViewBuilder::create()
        ->text("Trackpad & Touchscreen Gestures")
        ->textSize(config->metrics.font_size + 2)
        ->textColor(config->colors.on_surface)
        ->build();
    header_card->add_view(h_title);

    auto h_sub = TextViewBuilder::create()
        ->text("Multi-finger swipe shortcuts configured in ~/.config/miquland/gesture_binding.conf. Discrete gestures trigger immediate actions upon swipe completion.")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    header_card->add_view(h_sub);

    add_preview_card(header_card);

    // Read and parse sections directly from gesture_binding.conf
    std::vector<GestureCategorySection> sections = parse_gestures_file();

    for (const auto& sec : sections) {
        auto sec_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->padding(16, 16)
            ->spacing(10)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(get_card_radius(config))
            ->build();

        auto sec_header = TextViewBuilder::create()
            ->text(sec.title)
            ->caption(true)
            ->textColor(config->colors.primary)
            ->build();
        sec_card->add_view(sec_header);

        for (size_t i = 0; i < sec.gestures.size(); ++i) {
            const auto& g = sec.gestures[i];

            if (i > 0) {
                auto divider = LinearLayoutBuilder::create()
                    ->backgroundColor(config->colors.outline.with_alpha(0.10f))
                    ->build();
                LayoutParams dlp;
                dlp.width = static_cast<int>(LayoutDimension::MatchParent);
                dlp.height = 1;
                sec_card->add_view(divider, dlp);
            }

            auto row = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->gravity(Gravity::CenterVertical)
                ->spacing(12)
                ->build();

            auto text_box = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->spacing(2)
                ->build();

            auto act_lbl = TextViewBuilder::create()
                ->text(g.action_title)
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.on_surface)
                ->build();
            text_box->add_view(act_lbl);

            if (!g.description.empty()) {
                auto desc_lbl = TextViewBuilder::create()
                    ->text(g.description)
                    ->caption(true)
                    ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
                    ->build();
                text_box->add_view(desc_lbl);
            }

            LayoutParams tb_lp;
            tb_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            tb_lp.height = static_cast<int>(LayoutDimension::WrapContent);
            tb_lp.weight = 1.0f;
            tb_lp.gravity = Gravity::CenterVertical;
            row->add_view(text_box, tb_lp);

            auto badges_box = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->gravity(Gravity::CenterVertical)
                ->spacing(6)
                ->build();

            // Fingers badge
            auto fingers_badge = CardViewBuilder::create()
                ->style(CardStyle::Filled)
                ->backgroundColor(config->colors.surface_variant.with_alpha(0.6f))
                ->stroke(1, config->colors.outline.with_alpha(0.35f))
                ->cornerRadius(get_btn_radius(config))
                ->elevation(0)
                ->addView(
                    TextViewBuilder::create()
                        ->text(std::to_string(g.fingers) + " Fingers")
                        ->caption(true)
                        ->textColor(config->colors.primary)
                        ->padding(8, 4)
                        ->build()
                )
                ->build();
            LayoutParams fb_lp;
            fb_lp.width = static_cast<int>(LayoutDimension::WrapContent);
            fb_lp.height = static_cast<int>(LayoutDimension::WrapContent);
            fb_lp.gravity = Gravity::CenterVertical;
            badges_box->add_view(fingers_badge, fb_lp);

            // Direction badge
            auto dir_badge = CardViewBuilder::create()
                ->style(CardStyle::Filled)
                ->backgroundColor(config->colors.primary.with_alpha(0.12f))
                ->stroke(1, config->colors.primary.with_alpha(0.35f))
                ->cornerRadius(get_btn_radius(config))
                ->elevation(0)
                ->addView(
                    TextViewBuilder::create()
                        ->text(g.direction + " " + g.arrow)
                        ->caption(true)
                        ->textColor(config->colors.primary)
                        ->padding(8, 4)
                        ->build()
                )
                ->build();
            LayoutParams db_lp;
            db_lp.width = static_cast<int>(LayoutDimension::WrapContent);
            db_lp.height = static_cast<int>(LayoutDimension::WrapContent);
            db_lp.gravity = Gravity::CenterVertical;
            badges_box->add_view(dir_badge, db_lp);

            LayoutParams r_lp;
            r_lp.width = static_cast<int>(LayoutDimension::WrapContent);
            r_lp.height = static_cast<int>(LayoutDimension::WrapContent);
            r_lp.gravity = Gravity::CenterVertical;
            row->add_view(badges_box, r_lp);

            sec_card->add_view(row);
        }

        add_preview_card(sec_card);
    }
}

void MillerView::render_timeout_lock_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    if (!item) return;

    auto main_card = create_form_card(config);

    auto add_section_header = [&main_card, config](const std::string& title) {
        auto s_title = TextViewBuilder::create()
            ->text(title)
            ->caption(true)
            ->textColor(config->colors.primary)
            ->build();
        main_card->add_view(s_title);
    };

    // -------------------------------------------------------------
    // Section 1: Inactivity Timers
    // -------------------------------------------------------------
    add_section_header("Inactivity Timers");

    std::vector<int> lock_steps = {1, 2, 5, 10, 15, 30, 0};
    auto format_lock_mins = [](int mins) -> std::string {
        if (mins == 0) return "Never";
        if (mins == 1) return "1 minute";
        return std::to_string(mins) + " minutes";
    };
    int l_cur_idx = 0;
    for (size_t i = 0; i < lock_steps.size(); ++i) {
        if (lock_steps[i] == item->timeout_lock_cfg.lock_timeout_minutes) {
            l_cur_idx = static_cast<int>(i);
            break;
        }
    }
    add_form_slider(
        main_card, config,
        "Lock screen after inactivity",
        "Protects your session and hides open windows",
        static_cast<float>(l_cur_idx) / static_cast<float>(lock_steps.size() - 1),
        format_lock_mins(item->timeout_lock_cfg.lock_timeout_minutes),
        [item, lock_steps, format_lock_mins](float val) -> std::string {
            int idx = std::clamp(static_cast<int>(std::round(val * static_cast<float>(lock_steps.size() - 1))), 0, static_cast<int>(lock_steps.size() - 1));
            item->timeout_lock_cfg.lock_timeout_minutes = lock_steps[idx];
            return format_lock_mins(lock_steps[idx]);
        }
    );

    std::vector<int> disp_steps = {1, 2, 5, 10, 15, 30, 60, 0};
    auto format_disp_mins = [](int mins) -> std::string {
        if (mins == 0) return "Never";
        if (mins == 1) return "1 minute";
        if (mins == 60) return "1 hour";
        return std::to_string(mins) + " minutes";
    };
    int d_cur_idx = 0;
    for (size_t i = 0; i < disp_steps.size(); ++i) {
        if (disp_steps[i] == item->timeout_lock_cfg.display_off_minutes) {
            d_cur_idx = static_cast<int>(i);
            break;
        }
    }
    add_form_slider(
        main_card, config,
        "Turn off display after inactivity",
        "Powers down monitor backlight (DPMS power-off)",
        static_cast<float>(d_cur_idx) / static_cast<float>(disp_steps.size() - 1),
        format_disp_mins(item->timeout_lock_cfg.display_off_minutes),
        [item, disp_steps, format_disp_mins](float val) -> std::string {
            int idx = std::clamp(static_cast<int>(std::round(val * static_cast<float>(disp_steps.size() - 1))), 0, static_cast<int>(disp_steps.size() - 1));
            item->timeout_lock_cfg.display_off_minutes = disp_steps[idx];
            return format_disp_mins(disp_steps[idx]);
        }
    );

    std::vector<int> sleep_steps = {5, 10, 15, 30, 60, 120, 0};
    auto format_sleep_mins = [](int mins) -> std::string {
        if (mins == 0) return "Never";
        if (mins < 60) return std::to_string(mins) + " minutes";
        if (mins == 60) return "1 hour";
        return std::to_string(mins / 60) + " hours";
    };
    int s_cur_idx = 0;
    for (size_t i = 0; i < sleep_steps.size(); ++i) {
        if (sleep_steps[i] == item->timeout_lock_cfg.sleep_minutes) {
            s_cur_idx = static_cast<int>(i);
            break;
        }
    }
    add_form_slider(
        main_card, config,
        "Suspend system after inactivity",
        "Puts the computer into sleep mode to conserve energy",
        static_cast<float>(s_cur_idx) / static_cast<float>(sleep_steps.size() - 1),
        format_sleep_mins(item->timeout_lock_cfg.sleep_minutes),
        [item, sleep_steps, format_sleep_mins](float val) -> std::string {
            int idx = std::clamp(static_cast<int>(std::round(val * static_cast<float>(sleep_steps.size() - 1))), 0, static_cast<int>(sleep_steps.size() - 1));
            item->timeout_lock_cfg.sleep_minutes = sleep_steps[idx];
            return format_sleep_mins(sleep_steps[idx]);
        }
    );

    std::vector<int> hib_steps = {30, 60, 120, 240, 0};
    auto format_hib_mins = [](int mins) -> std::string {
        if (mins == 0) return "Disabled (Never Hibernate)";
        if (mins < 60) return std::to_string(mins) + " minutes after sleep";
        if (mins == 60) return "1 hour after sleep";
        return std::to_string(mins / 60) + " hours after sleep";
    };
    int h_cur_idx = 0;
    for (size_t i = 0; i < hib_steps.size(); ++i) {
        if (hib_steps[i] == item->timeout_lock_cfg.hibernate_delay_minutes) {
            h_cur_idx = static_cast<int>(i);
            break;
        }
    }
    add_form_slider(
        main_card, config,
        "Auto-hibernate to disk delay",
        "Saves RAM state to swap and powers off system",
        static_cast<float>(h_cur_idx) / static_cast<float>(hib_steps.size() - 1),
        format_hib_mins(item->timeout_lock_cfg.hibernate_delay_minutes),
        [item, hib_steps, format_hib_mins](float val) -> std::string {
            int idx = std::clamp(static_cast<int>(std::round(val * static_cast<float>(hib_steps.size() - 1))), 0, static_cast<int>(hib_steps.size() - 1));
            item->timeout_lock_cfg.hibernate_delay_minutes = hib_steps[idx];
            return format_hib_mins(hib_steps[idx]);
        }
    );

    // -------------------------------------------------------------
    // Section 2: Power Triggers & Sleep Modes (systemd-logind)
    // -------------------------------------------------------------
    add_form_divider(main_card, config);
    add_section_header("Power Triggers & Sleep Modes (systemd-logind)");

    std::vector<std::string> sleep_modes = {
        "Hybrid Sleep (Recommended)",
        "Sleep (Suspend to RAM)",
        "Hibernate (Suspend to Disk)"
    };
    int sm_sel = 0;
    for (size_t i = 0; i < sleep_modes.size(); ++i) {
        if (sleep_modes[i] == item->timeout_lock_cfg.sleep_mode) {
            sm_sel = static_cast<int>(i);
            break;
        }
    }
    add_form_spinner(
        main_card, config,
        "System sleep target",
        "Method used when system enters sleep",
        sleep_modes, sm_sel,
        [item, sleep_modes](int idx, const std::string& val) {
            (void)idx;
            item->timeout_lock_cfg.sleep_mode = val;
        }
    );

    std::vector<std::string> lid_actions = {
        "Sleep",
        "Lock only",
        "Hibernate",
        "Turn off display only",
        "Ignore (Do nothing)"
    };
    int la_sel = 0;
    for (size_t i = 0; i < lid_actions.size(); ++i) {
        if (lid_actions[i] == item->timeout_lock_cfg.lid_action) {
            la_sel = static_cast<int>(i);
            break;
        }
    }
    add_form_spinner(
        main_card, config,
        "When laptop lid is closed",
        "Action triggered when lid switch engages",
        lid_actions, la_sel,
        [item, lid_actions](int idx, const std::string& val) {
            (void)idx;
            item->timeout_lock_cfg.lid_action = val;
        }
    );

    std::vector<std::string> pwr_actions = {
        "Show power menu (miqupower)",
        "Sleep",
        "Lock",
        "Hibernate",
        "Power off"
    };
    int pa_sel = 0;
    for (size_t i = 0; i < pwr_actions.size(); ++i) {
        if (pwr_actions[i] == item->timeout_lock_cfg.power_button_action) {
            pa_sel = static_cast<int>(i);
            break;
        }
    }
    add_form_spinner(
        main_card, config,
        "When power button is pressed",
        "Physical chassis power key response",
        pwr_actions, pa_sel,
        [item, pwr_actions](int idx, const std::string& val) {
            (void)idx;
            item->timeout_lock_cfg.power_button_action = val;
        }
    );

    std::vector<std::string> crit_actions = {
        "Hibernate",
        "Sleep",
        "Power off"
    };
    int ca_sel = 0;
    for (size_t i = 0; i < crit_actions.size(); ++i) {
        if (crit_actions[i] == item->timeout_lock_cfg.critical_battery_action) {
            ca_sel = static_cast<int>(i);
            break;
        }
    }
    add_form_spinner(
        main_card, config,
        "When battery reaches critical level (<= 5%)",
        "Emergency safeguard to prevent sudden power cutoff",
        crit_actions, ca_sel,
        [item, crit_actions](int idx, const std::string& val) {
            (void)idx;
            item->timeout_lock_cfg.critical_battery_action = val;
        }
    );

    // -------------------------------------------------------------
    // Section 3: Display & Power Behavior
    // -------------------------------------------------------------
    add_form_divider(main_card, config);
    add_section_header("Display & Power Behavior");

    add_form_toggle(
        main_card, config,
        "Dim display before turning off",
        "Reduces backlight to 20% before turning screen off",
        item->timeout_lock_cfg.dim_screen,
        [item](bool c) { item->timeout_lock_cfg.dim_screen = c; }
    );

    add_form_toggle(
        main_card, config,
        "Prevent sleep while media is playing",
        "Wayland idle-inhibit lock when audio or video is active",
        item->timeout_lock_cfg.prevent_sleep_media,
        [item](bool c) { item->timeout_lock_cfg.prevent_sleep_media = c; }
    );

    add_form_toggle(
        main_card, config,
        "Energy saver timeouts on battery",
        "Shortens idle timeouts when running on battery power",
        item->timeout_lock_cfg.energy_saver_battery,
        [item](bool c) { item->timeout_lock_cfg.energy_saver_battery = c; }
    );

    add_form_toggle(
        main_card, config,
        "Clamshell mode (keep awake when docked)",
        "Prevents sleeping on lid close if external display & keyboard are connected",
        item->timeout_lock_cfg.clamshell_mode,
        [item](bool c) { item->timeout_lock_cfg.clamshell_mode = c; }
    );

    // -------------------------------------------------------------
    // Section 4: Security & Lockscreen (miqulock)
    // -------------------------------------------------------------
    add_form_divider(main_card, config);
    add_section_header("Security & Lockscreen (miqulock)");

    add_form_toggle(
        main_card, config,
        "Lock screen before sleep",
        "Ensures session is secured before system suspends",
        item->timeout_lock_cfg.lock_before_sleep,
        [item](bool c) { item->timeout_lock_cfg.lock_before_sleep = c; }
    );

    add_form_toggle(
        main_card, config,
        "Lock immediately when display turns off",
        "Locks session as soon as DPMS screen backlight powers down",
        item->timeout_lock_cfg.lock_on_display_off,
        [item](bool c) { item->timeout_lock_cfg.lock_on_display_off = c; }
    );

    add_form_toggle(
        main_card, config,
        "Require password to unlock",
        "Prompts for PAM authentication upon waking",
        item->timeout_lock_cfg.require_password_immediately,
        [this, item](bool c) {
            item->timeout_lock_cfg.require_password_immediately = c;
            rebuild_preview_column();
        }
    );

    if (item->timeout_lock_cfg.require_password_immediately) {
        std::vector<int> grace_steps = {0, 15, 30, 60, 120, 300};
        auto format_grace = [](int secs) -> std::string {
            if (secs == 0) return "Immediately (No grace period)";
            if (secs < 60) return std::to_string(secs) + " seconds";
            if (secs == 60) return "1 minute";
            return std::to_string(secs / 60) + " minutes";
        };
        int g_cur_idx = 0;
        for (size_t i = 0; i < grace_steps.size(); ++i) {
            if (grace_steps[i] == item->timeout_lock_cfg.grace_period_seconds) {
                g_cur_idx = static_cast<int>(i);
                break;
            }
        }
        add_form_slider(
            main_card, config,
            "Grace period before password is required",
            "Brief grace window right after display turns off",
            static_cast<float>(g_cur_idx) / static_cast<float>(grace_steps.size() - 1),
            format_grace(item->timeout_lock_cfg.grace_period_seconds),
            [item, grace_steps, format_grace](float val) -> std::string {
                int idx = std::clamp(static_cast<int>(std::round(val * static_cast<float>(grace_steps.size() - 1))), 0, static_cast<int>(grace_steps.size() - 1));
                item->timeout_lock_cfg.grace_period_seconds = grace_steps[idx];
                return format_grace(grace_steps[idx]);
            }
        );
    }

    add_form_toggle(
        main_card, config,
        "Show power actions on lockscreen",
        "Displays Sleep, Restart, and Shut Down buttons in miqulock",
        item->timeout_lock_cfg.lockscreen_show_power_actions,
        [item](bool c) { item->timeout_lock_cfg.lockscreen_show_power_actions = c; }
    );

    add_form_toggle(
        main_card, config,
        "24-hour clock format on lockscreen",
        "Uses 24-hour time notation (%H:%M) on miqulock clock",
        item->timeout_lock_cfg.lockscreen_24h_clock,
        [item](bool c) { item->timeout_lock_cfg.lockscreen_24h_clock = c; }
    );

    add_preview_card(main_card);

    // Terminology Card
    auto defs_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(8)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto defs_title_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(8)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto defs_icon = ImageViewBuilder::create()
        ->source("help-about")
        ->targetSize(20)
        ->fitMode(FitMode::Contain)
        ->build();
    LayoutParams deficon_lp;
    deficon_lp.width = 20;
    deficon_lp.height = 20;
    deficon_lp.gravity = Gravity::CenterVertical;
    defs_title_row->add_view(defs_icon, deficon_lp);

    auto defs_heading = TextViewBuilder::create()
        ->text("Power & Lock Terminology Explained")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    defs_title_row->add_view(defs_heading);
    defs_card->add_view(defs_title_row);

    auto add_def_entry = [&defs_card, config](const std::string& term, const std::string& meaning) {
        auto row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto t_label = TextViewBuilder::create()
            ->text(term)
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.primary)
            ->build();
        row->add_view(t_label);

        auto m_label = TextViewBuilder::create()
            ->text(meaning)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.85f))
            ->build();
        row->add_view(m_label);

        LayoutParams r_lp;
        r_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        r_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        defs_card->add_view(row, r_lp);
    };

    add_def_entry("Display Off (DPMS)", "Powers down the screen backlight only. The CPU, Wi-Fi, and background applications continue running normally at full speed.");
    add_def_entry("Sleep (Suspend to RAM)", "Pauses the CPU and freezes open applications in RAM with minimal power draw (~0.3W). Wakes back to your desktop in 1 to 2 seconds.");
    add_def_entry("Hibernate (Suspend to Disk)", "Writes a complete memory snapshot to swap disk and cuts 100% of power (0W). Preserves your open work indefinitely even on zero battery.");
    add_def_entry("Hybrid Sleep", "Enters Sleep immediately for fast 1-second wake, then automatically Hibernates to swap disk after prolonged inactivity to protect the battery.");
    add_def_entry("Lock Screen (miqulock)", "Obscures your workspace behind PAM authentication. Requires your password to unlock while apps continue running uninterrupted in the background.");
    add_def_entry("Grace Period", "A brief safety window right after the display turns off where pressing any key immediately resumes your desktop without asking for your password.");
    add_def_entry("Clamshell Docking", "Allows you to close your laptop lid without triggering sleep or lock when an external monitor and keyboard/mouse are connected.");

    add_preview_card(defs_card);
}

static std::string exec_command_trimmed(const std::string& cmd) {
    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) return "";
    char buf[512];
    std::string out;
    if (fgets(buf, sizeof(buf), fp)) {
        out = buf;
    }
    pclose(fp);
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' ' || out.back() == '\'')) {
        out.pop_back();
    }
    while (!out.empty() && (out.front() == '\'' || out.front() == ' ')) {
        out.erase(out.begin());
    }
    return out;
}

static std::vector<std::string> get_icon_roots() {
    std::vector<std::string> roots = {
        "/usr/share/icons",
        "/usr/local/share/icons"
    };
    const char* home = std::getenv("HOME");
    if (home && *home) {
        roots.push_back(std::string(home) + "/.local/share/icons");
        roots.push_back(std::string(home) + "/.icons");
    }
    const char* xdg_home = std::getenv("XDG_DATA_HOME");
    if (xdg_home && *xdg_home) {
        roots.push_back(std::string(xdg_home) + "/icons");
    }
    return roots;
}

static std::vector<std::string> get_theme_search_hierarchy(const std::string& theme_name, const std::vector<std::string>& roots, const std::string& fallback_theme) {
    std::string actual_theme = (theme_name == "Miqu (Default)") ? "miqu-cursors" : theme_name;
    std::vector<std::string> themes = {actual_theme};
    std::vector<std::string> cand_names = {actual_theme};
    if (actual_theme == "Default") cand_names.push_back("default");
    else if (actual_theme == "default") cand_names.push_back("Default");

    std::error_code ec;
    for (const auto& r : roots) {
        for (const auto& nm : cand_names) {
            fs::path idx_path = fs::path(r) / nm / "index.theme";
            if (fs::is_regular_file(idx_path, ec)) {
                std::ifstream f(idx_path);
                std::string line;
                while (std::getline(f, line)) {
                    if (line.rfind("Inherits=", 0) == 0) {
                        std::stringstream ss(line.substr(9));
                        std::string item;
                        while (std::getline(ss, item, ',')) {
                            size_t first = item.find_first_not_of(" \t\r\n");
                            size_t last = item.find_last_not_of(" \t\r\n");
                            if (first != std::string::npos && last != std::string::npos) {
                                std::string clean = item.substr(first, last - first + 1);
                                if (!clean.empty() && std::find(themes.begin(), themes.end(), clean) == themes.end()) {
                                    themes.push_back(clean);
                                }
                            }
                        }
                    }
                }
                break;
            }
        }
    }
    if (!fallback_theme.empty() && std::find(themes.begin(), themes.end(), fallback_theme) == themes.end()) {
        themes.push_back(fallback_theme);
    }
    return themes;
}

static std::vector<std::string> get_system_icon_packs() {
    auto roots = get_icon_roots();
    std::set<std::string> themes;
    std::error_code ec;
    for (const auto& root_dir : roots) {
        if (!fs::is_directory(root_dir, ec)) continue;
        for (const auto& entry : fs::directory_iterator(root_dir, ec)) {
            if (!entry.is_directory(ec)) continue;
            std::string id = entry.path().filename().string();
            if (id.empty() || id[0] == '.') continue;

            fs::path tdir = entry.path();
            fs::path idx_path = tdir / "index.theme";
            bool has_idx = fs::is_regular_file(idx_path, ec);
            bool has_icon_dirs = fs::is_directory(tdir / "scalable", ec) ||
                                 fs::is_directory(tdir / "48x48", ec) ||
                                 fs::is_directory(tdir / "32x32", ec) ||
                                 fs::is_directory(tdir / "apps", ec) ||
                                 fs::is_directory(tdir / "places", ec);
            if (has_icon_dirs || (has_idx && !fs::is_directory(tdir / "cursors", ec))) {
                themes.insert(id);
            }
        }
    }
    return std::vector<std::string>(themes.begin(), themes.end());
}

static std::vector<std::string> get_system_cursor_themes() {
    auto roots = get_icon_roots();
    std::set<std::string> themes;
    std::error_code ec;
    for (const auto& root_dir : roots) {
        if (!fs::is_directory(root_dir, ec)) continue;
        for (const auto& entry : fs::directory_iterator(root_dir, ec)) {
            if (!entry.is_directory(ec)) continue;
            std::string id = entry.path().filename().string();
            if (id.empty() || id[0] == '.') continue;
            if (id == "default" || id == "Default" || id == "miqu-cursors") continue;

            fs::path cursor_dir = entry.path() / "cursors";
            if (fs::is_directory(cursor_dir, ec)) {
                themes.insert(id);
            }
        }
    }
    std::vector<std::string> result = {"Miqu (Default)"};
    result.insert(result.end(), themes.begin(), themes.end());
    return result;
}

static std::vector<std::string> get_theme_sample_icons(const std::string& theme_name) {
    static const char* const categories[][6] = {
        {"folder", "system-file-manager", "inode-directory", nullptr},
        {"utilities-terminal", "terminal", "org.gnome.Terminal", nullptr},
        {"web-browser", "internet-web-browser", "firefox", "google-chrome", nullptr},
        {"accessories-text-editor", "text-editor", "gedit", "org.gnome.TextEditor", nullptr},
        {"preferences-system", "system-settings", "gnome-control-center", nullptr}
    };

    auto roots = get_icon_roots();
    auto themes_to_search = get_theme_search_hierarchy(theme_name, roots, "hicolor");

    const std::vector<std::string> subdirs = {
        "scalable/places", "48x48/places", "64x64/places", "32x32/places", "places",
        "scalable/apps", "48x48/apps", "64x64/apps", "32x32/apps", "128x128/apps", "256x256/apps", "apps",
        "scalable/categories", "48x48/categories", "categories",
        "scalable/devices", "48x48/devices", "devices",
        "symbolic/apps", "symbolic/places", "symbolic", ""
    };
    const std::vector<std::string> extensions = {".svg", ".png"};

    std::vector<std::string> result;
    std::error_code ec;
    for (const auto& cat : categories) {
        std::string found_path;
        for (const auto& th : themes_to_search) {
            for (const auto& r : roots) {
                fs::path th_dir = fs::path(r) / th;
                if (!fs::is_directory(th_dir, ec)) continue;

                for (int i = 0; cat[i] != nullptr; ++i) {
                    const char* icon_name = cat[i];
                    for (const auto& sub : subdirs) {
                        fs::path dir = sub.empty() ? th_dir : (th_dir / sub);
                        for (const auto& ext : extensions) {
                            fs::path cand = dir / (std::string(icon_name) + ext);
                            if (fs::is_regular_file(cand, ec)) {
                                found_path = cand.string();
                                break;
                            }
                        }
                        if (!found_path.empty()) break;
                    }
                    if (!found_path.empty()) break;
                }
                if (!found_path.empty()) break;
            }
            if (!found_path.empty()) break;
        }

        if (found_path.empty() && cat[0] != nullptr) {
            found_path = cat[0];
        }
        result.push_back(found_path);
    }
    return result;
}

typedef unsigned int XcursorPixel;
typedef unsigned int XcursorDim;
struct XcursorImageRaw {
    XcursorDim version;
    XcursorDim size;
    XcursorDim width;
    XcursorDim height;
    XcursorDim xhot;
    XcursorDim yhot;
    XcursorPixel delay;
    XcursorPixel *pixels;
};

static bool extract_cursor_to_tga(const std::string& cursor_file, const std::string& out_tga) {
    static void* lib_handle = dlopen("libXcursor.so.1", RTLD_LAZY);
    if (!lib_handle) return false;

    typedef XcursorImageRaw* (*FnLoad)(FILE*, int);
    typedef void (*FnDestroy)(XcursorImageRaw*);

    static auto load_fn = (FnLoad)dlsym(lib_handle, "XcursorFileLoadImage");
    static auto destroy_fn = (FnDestroy)dlsym(lib_handle, "XcursorImageDestroy");
    if (!load_fn || !destroy_fn) return false;

    FILE* fp = fopen(cursor_file.c_str(), "rb");
    if (!fp) return false;

    XcursorImageRaw* img = load_fn(fp, 32);
    fclose(fp);
    if (!img) return false;

    if (img->width == 0 || img->height == 0 || !img->pixels) {
        destroy_fn(img);
        return false;
    }

    FILE* out_fp = fopen(out_tga.c_str(), "wb");
    if (!out_fp) {
        destroy_fn(img);
        return false;
    }

    uint16_t w = static_cast<uint16_t>(img->width);
    uint16_t h = static_cast<uint16_t>(img->height);
    uint8_t tga_header[18] = {
        0, 0, 2, 0, 0, 0, 0, 0,
        0, 0, 0, 0,
        static_cast<uint8_t>(w & 0xFF), static_cast<uint8_t>((w >> 8) & 0xFF),
        static_cast<uint8_t>(h & 0xFF), static_cast<uint8_t>((h >> 8) & 0xFF),
        32, 0x28
    };

    fwrite(tga_header, 1, sizeof(tga_header), out_fp);
    fwrite(img->pixels, sizeof(uint32_t), img->width * img->height, out_fp);
    fclose(out_fp);

    destroy_fn(img);
    return true;
}

static std::vector<std::string> get_theme_sample_cursors(const std::string& theme_name) {
    struct CursorStateDef {
        const char* name;
        const char* aliases[4];
    };
    static const CursorStateDef cursor_states[] = {
        {"default", {"default", "left_ptr", "arrow", nullptr}},
        {"pointer", {"pointer", "hand2", "pointing_hand", nullptr}},
        {"text", {"text", "xterm", "ibeam", nullptr}},
        {"crosshair", {"crosshair", "cross", nullptr, nullptr}},
        {"wait", {"wait", "watch", "progress", nullptr}}
    };

    auto roots = get_icon_roots();
    auto themes_to_search = get_theme_search_hierarchy(theme_name, roots, "miqu-cursors");
    if (std::find(themes_to_search.begin(), themes_to_search.end(), "default") == themes_to_search.end()) {
        themes_to_search.push_back("default");
    }

    std::string safe_theme = theme_name;
    for (char& c : safe_theme) {
        if (!std::isalnum(c) && c != '-' && c != '_') c = '_';
    }

    std::vector<std::string> result_imgs;
    std::error_code ec;

    for (const auto& state : cursor_states) {
        std::string found_cursor_file;
        for (const auto& th : themes_to_search) {
            for (const auto& r : roots) {
                fs::path cur_dir = fs::path(r) / th / "cursors";
                if (!fs::is_directory(cur_dir, ec)) continue;

                for (int i = 0; state.aliases[i] != nullptr; ++i) {
                    fs::path cand = cur_dir / state.aliases[i];
                    if (fs::exists(cand, ec)) {
                        found_cursor_file = cand.string();
                        break;
                    }
                }
                if (!found_cursor_file.empty()) break;
            }
            if (!found_cursor_file.empty()) break;
        }

        std::string img_path = "/tmp/miqu_cur_" + safe_theme + "_" + state.name + ".tga";
        bool has_img = fs::is_regular_file(img_path, ec) && fs::file_size(img_path, ec) > 0;

        if (!has_img && !found_cursor_file.empty()) {
            if (extract_cursor_to_tga(found_cursor_file, img_path)) {
                has_img = true;
            }
        }

        if (has_img) {
            result_imgs.push_back(img_path);
        }
    }

    return result_imgs;
}

static void sync_miqutoolkit_icon_theme(const std::string& theme) {
    const char* home = std::getenv("HOME");
    if (!home) return;

    fs::path tk_dir = fs::path(home) / ".config" / "miqutoolkit";
    fs::path tk_conf = tk_dir / "miqutoolkit.conf";
    std::error_code ec;
    if (!fs::exists(tk_dir, ec) || !fs::exists(tk_conf, ec)) {
        std::system("miqutoolkit-cli --init-config >/dev/null 2>&1");
    }
    if (!fs::exists(tk_dir, ec)) {
        fs::create_directories(tk_dir, ec);
    }

    if (!fs::exists(tk_conf, ec)) {
        for (const char* p : {"/usr/share/miqutoolkit/miqutoolkit.conf", "/usr/local/share/miqutoolkit/miqutoolkit.conf", "/etc/miqutoolkit/miqutoolkit.conf", "assets/miqutoolkit.conf"}) {
            if (fs::exists(p, ec)) {
                fs::copy_file(p, tk_conf, fs::copy_options::overwrite_existing, ec);
                break;
            }
        }
    }

    if (fs::is_regular_file(tk_conf, ec)) {
        std::ifstream in(tk_conf);
        if (in.is_open()) {
            std::vector<std::string> lines;
            std::string line;
            bool found_icon_theme = false;

            while (std::getline(in, line)) {
                std::string trimmed = line;
                size_t s = trimmed.find_first_not_of(" \t");
                std::string t = (s != std::string::npos) ? trimmed.substr(s) : "";

                size_t c_pos = t.find('#');
                std::string no_comment = (c_pos != std::string::npos) ? t.substr(0, c_pos) : t;
                while (!no_comment.empty() && (no_comment.back() == ' ' || no_comment.back() == '\t')) no_comment.pop_back();

                size_t eq = no_comment.find('=');
                if (eq != std::string::npos) {
                    std::string k = no_comment.substr(0, eq);
                    while (!k.empty() && (k.back() == ' ' || k.back() == '\t')) k.pop_back();
                    while (!k.empty() && (k.front() == ' ' || k.front() == '\t')) k.erase(0, 1);

                    if (k == "icon_theme") {
                        lines.push_back("icon_theme = " + theme);
                        found_icon_theme = true;
                        continue;
                    }
                }
                lines.push_back(line);
            }
            in.close();

            if (!found_icon_theme) {
                lines.push_back("icon_theme = " + theme);
            }

            std::ofstream out(tk_conf);
            if (out.is_open()) {
                for (const auto& l : lines) out << l << "\n";
                out.flush();
            }
        }
    } else {
        std::ofstream out(tk_conf);
        if (out.is_open()) {
            out << "# miqutoolkit configuration\nicon_theme = " + theme + "\n";
            out.flush();
        }
    }
}

void MillerView::render_icon_packs_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    if (!item || !item->icon_packs_cfg) return;
    auto& cfg = *item->icon_packs_cfg;

    if (!cfg.loaded) {
        cfg.available_icon_packs = get_system_icon_packs();

        const char* home = std::getenv("HOME");
        std::string tk_conf_path;
        std::error_code ec;
        if (home) {
            fs::path user_conf = fs::path(home) / ".config" / "miqutoolkit" / "miqutoolkit.conf";
            if (fs::exists(user_conf, ec)) {
                tk_conf_path = user_conf.string();
            }
        }
        if (tk_conf_path.empty()) {
            for (const char* p : {"/usr/share/miqutoolkit/miqutoolkit.conf", "/usr/local/share/miqutoolkit/miqutoolkit.conf", "/etc/miqutoolkit/miqutoolkit.conf", "assets/miqutoolkit.conf"}) {
                if (fs::exists(p, ec)) {
                    tk_conf_path = p;
                    break;
                }
            }
        }

        std::string cur_tk = "";
        if (!tk_conf_path.empty() && fs::is_regular_file(tk_conf_path, ec)) {
            std::ifstream in(tk_conf_path);
            std::string line;
            while (std::getline(in, line)) {
                size_t c_pos = line.find('#');
                std::string no_comment = (c_pos != std::string::npos) ? line.substr(0, c_pos) : line;
                size_t eq = no_comment.find('=');
                if (eq != std::string::npos) {
                    std::string k = trim_str(no_comment.substr(0, eq));
                    if (k == "icon_theme") {
                        cur_tk = trim_str(no_comment.substr(eq + 1));
                        break;
                    }
                }
            }
        }
        if (cur_tk.empty()) {
            cur_tk = Config::get()->metrics.icon_theme;
        }

        std::string cur_gtk = exec_command_trimmed("gsettings get org.gnome.desktop.interface icon-theme 2>/dev/null");
        if (!cur_gtk.empty() && cur_gtk.front() == '\'' && cur_gtk.back() == '\'') {
            cur_gtk = cur_gtk.substr(1, cur_gtk.size() - 2);
        }

        std::string active_theme;
        if (!cur_tk.empty() && cur_tk == cur_gtk) {
            active_theme = cur_tk;
        } else if (!cur_tk.empty() && cur_tk != "hicolor") {
            active_theme = cur_tk;
        } else if (!cur_gtk.empty()) {
            active_theme = cur_gtk;
        } else if (!cur_tk.empty()) {
            active_theme = cur_tk;
        } else if (!cfg.available_icon_packs.empty()) {
            active_theme = cfg.available_icon_packs.front();
        } else {
            active_theme = "hicolor";
        }

        cfg.icon_theme = active_theme;
        cfg.loaded = true;
    }

    // --- SYSTEM ICON PACK SECTION ---
    auto sys_preview_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto p_title = TextViewBuilder::create()
        ->text("Preview (" + cfg.icon_theme + ")")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    sys_preview_card->add_view(p_title);

    auto icons_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(6)
        ->gravity(Gravity::Center)
        ->build();

    auto samples = get_theme_sample_icons(cfg.icon_theme);
    for (const auto& sample_src : samples) {
        auto item_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->padding(8, 8)
            ->gravity(Gravity::Center)
            ->backgroundColor(config->colors.on_surface.with_alpha(0.04f))
            ->stroke(1, config->colors.outline.with_alpha(0.15f))
            ->cornerRadius(get_inner_radius(config))
            ->build();

        auto ic = ImageViewBuilder::create()
            ->source(sample_src)
            ->targetSize(36)
            ->fitMode(FitMode::Contain)
            ->build();
        LayoutParams ic_lp(36, 36);
        ic_lp.gravity = Gravity::Center;
        item_box->add_view(ic, ic_lp);

        LayoutParams box_lp(static_cast<int>(LayoutDimension::MatchParent), static_cast<int>(LayoutDimension::WrapContent), 1.0f);
        icons_row->add_view(item_box, box_lp);
    }
    sys_preview_card->add_view(icons_row);
    add_preview_card(sys_preview_card);

    auto sys_main_card = create_form_card(config);

    int cur_icon_idx = 0;
    for (size_t i = 0; i < cfg.available_icon_packs.size(); ++i) {
        if (cfg.available_icon_packs[i] == cfg.icon_theme) {
            cur_icon_idx = static_cast<int>(i);
            break;
        }
    }
    if (cur_icon_idx == 0 && !cfg.available_icon_packs.empty()) {
        for (size_t i = 0; i < cfg.available_icon_packs.size(); ++i) {
            std::string a = cfg.available_icon_packs[i];
            std::string b = cfg.icon_theme;
            std::transform(a.begin(), a.end(), a.begin(), ::tolower);
            std::transform(b.begin(), b.end(), b.begin(), ::tolower);
            if (a == b) {
                cur_icon_idx = static_cast<int>(i);
                cfg.icon_theme = cfg.available_icon_packs[i];
                break;
            }
        }
    }

    add_form_spinner(
        sys_main_card, config,
        "System Icon Pack",
        "Select active application icon theme",
        cfg.available_icon_packs,
        cur_icon_idx,
        [this, item](int idx, const std::string& val) {
            (void)idx;
            if (!item || !item->icon_packs_cfg) return;
            item->icon_packs_cfg->icon_theme = val;
            std::string cmd = "gsettings set org.gnome.desktop.interface icon-theme '" + val + "' 2>/dev/null &";
            std::system(cmd.c_str());
            sync_miqutoolkit_icon_theme(val);
            item->last_action_message = "System icon pack set to: " + val;
            deferred_rebuild_preview(10);
        }
    );
    add_preview_card(sys_main_card);
}

static void load_cursor_config(CursorConfig& cfg) {
    if (cfg.loaded) return;
    cfg.available_cursor_themes = get_system_cursor_themes();
    cfg.available_cursor_sizes = {16, 24, 30, 32, 36, 48, 64};

    const char* home = std::getenv("HOME");
    std::string parsed_theme = "";
    int parsed_size = -1;

    std::string cconf_path;
    if (home) {
        fs::path user_conf = fs::path(home) / ".config" / "miquland" / "cursor.conf";
        std::error_code ec;
        if (fs::exists(user_conf, ec)) cconf_path = user_conf.string();
    }
    if (cconf_path.empty()) {
        for (const char* p : {"/usr/share/miquland/cursor.conf", "/usr/local/share/miquland/cursor.conf", "assets/cursor.conf"}) {
            if (fs::exists(p)) {
                cconf_path = p;
                break;
            }
        }
    }

    if (!cconf_path.empty()) {
        std::ifstream in(cconf_path);
        std::string line;
        while (std::getline(in, line)) {
            size_t c_pos = line.find('#');
            std::string t = (c_pos != std::string::npos) ? line.substr(0, c_pos) : line;
            size_t eq = t.find('=');
            if (eq != std::string::npos) {
                std::string k = trim_str(t.substr(0, eq));
                std::string v = trim_str(t.substr(eq + 1));
                if (k == "theme" && !v.empty()) {
                    parsed_theme = v;
                } else if (k == "size" && !v.empty()) {
                    try { parsed_size = std::stoi(v); } catch (...) {}
                }
            }
        }
    }

    if (!parsed_theme.empty()) {
        if (parsed_theme == "default" || parsed_theme == "Default" || parsed_theme == "miqu-cursors") {
            cfg.cursor_theme = "Miqu (Default)";
        } else {
            cfg.cursor_theme = parsed_theme;
        }
    } else {
        std::string cur_cur = exec_command_trimmed("gsettings get org.gnome.desktop.interface cursor-theme 2>/dev/null");
        if (!cur_cur.empty() && cur_cur.front() == '\'' && cur_cur.back() == '\'') {
            cur_cur = cur_cur.substr(1, cur_cur.size() - 2);
        }
        if (cur_cur.empty() || cur_cur == "default" || cur_cur == "Default" || cur_cur == "miqu-cursors") {
            cur_cur = "Miqu (Default)";
        }
        cfg.cursor_theme = cur_cur;
    }

    if (parsed_size > 0) {
        cfg.cursor_size = parsed_size;
    } else {
        std::string cur_sz = exec_command_trimmed("gsettings get org.gnome.desktop.interface cursor-size 2>/dev/null");
        try {
            if (!cur_sz.empty()) cfg.cursor_size = std::stoi(cur_sz);
        } catch (...) {
            cfg.cursor_size = 30;
        }
    }
    cfg.loaded = true;
}


static void sync_miquland_cursor_config(const std::string& theme, int size) {
    const char* home = std::getenv("HOME");
    if (!home) return;

    fs::path miqu_dir = fs::path(home) / ".config" / "miquland";
    std::error_code ec;
    if (!fs::exists(miqu_dir, ec) || !fs::exists(miqu_dir / "cursor.conf", ec)) {
        // Fresh install: initialize complete standard configuration via miquland
        std::system("miquland --init-config >/dev/null 2>&1");
    }
    if (!fs::exists(miqu_dir, ec)) {
        fs::create_directories(miqu_dir, ec);
    }

    fs::path cursor_conf = miqu_dir / "cursor.conf";
    fs::path miquland_conf = miqu_dir / "miquland.conf";

    // 1. Always write/update cursor.conf (the modular cursor configuration)
    {
        std::string new_content =
            "# ==============================================================================\n"
            "# cursor.conf — Cursor & Appearance (Modular Sourced File)\n"
            "# ==============================================================================\n"
            "cursor {\n"
            "    # XCursor theme name (located in /usr/share/icons or ~/.icons)\n"
            "    theme = " + theme + "\n\n"
            "    # Cursor size in pixels (typical values: 16, 24, 32, 48)\n"
            "    size = " + std::to_string(size) + "\n"
            "}\n";

        std::ofstream out(cursor_conf);
        if (out.is_open()) {
            out << new_content;
            out.flush();
        }
    }

    // 2. If miquland.conf has an inline cursor block, keep it updated as well
    if (fs::is_regular_file(miquland_conf, ec)) {
        std::ifstream in(miquland_conf);
        if (in.is_open()) {
            std::vector<std::string> lines;
            std::string line;
            bool inside_cursor = false;
            bool has_cursor_section = false;
            bool has_theme = false, has_size = false;

            while (std::getline(in, line)) {
                std::string trimmed = line;
                size_t s = trimmed.find_first_not_of(" \t");
                std::string t = (s != std::string::npos) ? trimmed.substr(s) : "";

                if (!inside_cursor) {
                    if (t.rfind("cursor", 0) == 0) {
                        size_t brace = t.find('{');
                        if (brace != std::string::npos) {
                            inside_cursor = true;
                            has_cursor_section = true;
                            lines.push_back(line);
                            continue;
                        }
                    }
                    lines.push_back(line);
                } else {
                    if (t.find('}') != std::string::npos) {
                        if (!has_theme) lines.push_back("    theme = " + theme);
                        if (!has_size) lines.push_back("    size = " + std::to_string(size));
                        inside_cursor = false;
                        lines.push_back(line);
                    } else if (t.rfind("theme", 0) == 0 && t.find('=') != std::string::npos) {
                        lines.push_back("    theme = " + theme);
                        has_theme = true;
                    } else if (t.rfind("size", 0) == 0 && t.find('=') != std::string::npos) {
                        lines.push_back("    size = " + std::to_string(size));
                        has_size = true;
                    } else {
                        lines.push_back(line);
                    }
                }
            }
            in.close();

            if (has_cursor_section) {
                std::ofstream out(miquland_conf);
                if (out.is_open()) {
                    for (const auto& l : lines) out << l << "\n";
                    out.flush();
                }
            }
        }
    }
}

void MillerView::render_cursor_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    if (!item || !item->cursor_cfg) return;
    auto& cfg = *item->cursor_cfg;
    load_cursor_config(cfg);

    // --- 1. CURSOR PREVIEW CARD ---
    auto cur_preview_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    std::string display_name = (cfg.cursor_theme == "Miqu (Default)") ? "Miqu" : cfg.cursor_theme;
    auto p_title = TextViewBuilder::create()
        ->text("Preview (" + display_name + ", " + std::to_string(cfg.cursor_size) + " px)")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    cur_preview_card->add_view(p_title);

    auto cursor_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(6)
        ->gravity(Gravity::Center)
        ->build();

    int target_disp_size = std::clamp(cfg.cursor_size, 20, 48);
    auto cursor_imgs = get_theme_sample_cursors(cfg.cursor_theme);
    for (const auto& img_path : cursor_imgs) {
        auto item_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->padding(8, 8)
            ->gravity(Gravity::Center)
            ->backgroundColor(config->colors.on_surface.with_alpha(0.04f))
            ->stroke(1, config->colors.outline.with_alpha(0.15f))
            ->cornerRadius(get_inner_radius(config))
            ->build();

        auto ic = ImageViewBuilder::create()
            ->source(img_path)
            ->targetSize(target_disp_size)
            ->fitMode(FitMode::Contain)
            ->build();
        LayoutParams ic_lp(target_disp_size, target_disp_size);
        ic_lp.gravity = Gravity::Center;
        item_box->add_view(ic, ic_lp);

        LayoutParams box_lp(static_cast<int>(LayoutDimension::MatchParent), static_cast<int>(LayoutDimension::WrapContent), 1.0f);
        cursor_row->add_view(item_box, box_lp);
    }
    cur_preview_card->add_view(cursor_row);
    add_preview_card(cur_preview_card);

    // --- 2. FORM CONTROLS (Theme & Size) ---
    auto cur_main_card = create_form_card(config);

    int cur_cursor_idx = 0;
    for (size_t i = 0; i < cfg.available_cursor_themes.size(); ++i) {
        if (cfg.available_cursor_themes[i] == cfg.cursor_theme ||
            ((cfg.cursor_theme == "default" || cfg.cursor_theme == "Default" || cfg.cursor_theme == "miqu-cursors") &&
             cfg.available_cursor_themes[i] == "Miqu (Default)")) {
            cur_cursor_idx = static_cast<int>(i);
            break;
        }
    }

    add_form_spinner(
        cur_main_card, config,
        "Cursor Theme",
        "Select active pointer theme",
        cfg.available_cursor_themes,
        cur_cursor_idx,
        [this, item](int idx, const std::string& val) {
            (void)idx;
            if (!item || !item->cursor_cfg) return;
            item->cursor_cfg->cursor_theme = val;
            std::string target = (val == "Miqu (Default)") ? "miqu-cursors" : val;
            std::string cmd = "gsettings set org.gnome.desktop.interface cursor-theme '" + target + "' 2>/dev/null &";
            std::system(cmd.c_str());
            sync_miquland_cursor_config(target, item->cursor_cfg->cursor_size);
            item->last_action_message = "Cursor theme set to: " + val;
            deferred_rebuild_preview(10);
        }
    );

    std::vector<std::string> size_labels;
    int cur_size_idx = 0;
    for (size_t i = 0; i < cfg.available_cursor_sizes.size(); ++i) {
        int sz = cfg.available_cursor_sizes[i];
        std::string lbl = std::to_string(sz) + " px";
        if (sz == 30) lbl += " (Default)";
        size_labels.push_back(lbl);
        if (sz == cfg.cursor_size) {
            cur_size_idx = static_cast<int>(i);
        }
    }

    add_form_spinner(
        cur_main_card, config,
        "Cursor Size",
        "Pointer scale in pixels",
        size_labels,
        cur_size_idx,
        [this, item](int idx, const std::string& val) {
            (void)val;
            if (!item || !item->cursor_cfg) return;
            if (idx >= 0 && idx < static_cast<int>(item->cursor_cfg->available_cursor_sizes.size())) {
                int sz = item->cursor_cfg->available_cursor_sizes[idx];
                item->cursor_cfg->cursor_size = sz;
                std::string cmd = "gsettings set org.gnome.desktop.interface cursor-size " + std::to_string(sz) + " 2>/dev/null &";
                std::system(cmd.c_str());
                std::string target = (item->cursor_cfg->cursor_theme == "Miqu (Default)") ? "miqu-cursors" : item->cursor_cfg->cursor_theme;
                sync_miquland_cursor_config(target, sz);
                item->last_action_message = "Cursor size set to: " + std::to_string(sz) + " px";
                deferred_rebuild_preview(10);
            }
        }
    );

    add_preview_card(cur_main_card);
}

static Color parse_border_display_color(const std::string& val, const Color& fallback) {
    if (val.empty()) return fallback;
    std::string s = val;
    size_t sp = s.find_first_of(" \t");
    if (sp != std::string::npos) {
        s = s.substr(0, sp);
    }
    return Color::from_hex(s, fallback);
}

static void load_gaps_border_config(GapsBorderConfig& cfg) {
    if (cfg.loaded) return;

    cfg.active_border_presets = {
        "Yellow-Red Gradient (45°)",
        "Cyan-Blue Gradient (45°)",
        "Purple-Blue Gradient (135°)",
        "Neon Emerald-Cyan (90°)",
        "Monochrome Bright (#FFFFFF)",
        "Sky Blue (#3584e4)",
        "Coral Flame (#ff6b6b)"
    };
    cfg.inactive_border_presets = {
        "Dark Slate Gradient (45°)",
        "Soft Slate (#99c2ff)",
        "Graphite Charcoal (#2D3748)",
        "Muted Amber-Red (45°)",
        "Subtle Outline (20% Opacity)",
        "Transparent (Zero Border)"
    };

    const char* home = std::getenv("HOME");
    if (!home) {
        cfg.loaded = true;
        return;
    }

    auto parse_file = [&](const fs::path& p) {
        std::error_code ec;
        if (!fs::is_regular_file(p, ec)) return;
        std::ifstream in(p);
        if (!in.is_open()) return;

        std::vector<std::string> stack;
        std::string line;
        while (std::getline(in, line)) {
            std::string trimmed = line;
            size_t s = trimmed.find_first_not_of(" \t");
            std::string t = (s != std::string::npos) ? trimmed.substr(s) : "";

            size_t c_pos = t.find('#');
            std::string no_comment = (c_pos != std::string::npos) ? t.substr(0, c_pos) : t;
            while (!no_comment.empty() && (no_comment.back() == ' ' || no_comment.back() == '\t')) no_comment.pop_back();

            if (!no_comment.empty() && no_comment.back() == '{') {
                std::string sec = no_comment.substr(0, no_comment.size() - 1);
                while (!sec.empty() && (sec.back() == ' ' || sec.back() == '\t')) sec.pop_back();
                stack.push_back(sec);
                continue;
            }
            if (!no_comment.empty() && no_comment.front() == '}') {
                if (!stack.empty()) stack.pop_back();
                continue;
            }

            size_t eq = no_comment.find('=');
            if (eq != std::string::npos) {
                std::string k = no_comment.substr(0, eq);
                while (!k.empty() && (k.back() == ' ' || k.back() == '\t')) k.pop_back();
                while (!k.empty() && (k.front() == ' ' || k.front() == '\t')) k.erase(0, 1);

                std::string v = no_comment.substr(eq + 1);
                while (!v.empty() && (v.front() == ' ' || v.front() == '\t' || v.front() == '"')) v.erase(0, 1);
                while (!v.empty() && (v.back() == ' ' || v.back() == '\t' || v.back() == '"')) v.pop_back();

                std::string top = stack.empty() ? "" : stack.back();
                if (top == "gaps") {
                    if (k == "inner") {
                        try { cfg.inner_gap = std::max(0, std::stoi(v)); } catch (...) {}
                    } else if (k == "outer") {
                        try { cfg.outer_gap = std::max(0, std::stoi(v)); } catch (...) {}
                    }
                } else if (top == "border") {
                    if (k == "width") {
                        try { cfg.border_width = std::max(0, std::stoi(v)); } catch (...) {}
                    } else if (k == "radius") {
                        try { cfg.corner_radius = std::max(0, std::stoi(v)); } catch (...) {}
                    } else if (k == "active") {
                        cfg.active_border_value = v;
                    } else if (k == "inactive") {
                        cfg.inactive_border_value = v;
                    } else if (k == "shading") {
                        cfg.border_shading = (v == "true" || v == "1" || v == "yes");
                    } else if (k == "shading_strength") {
                        try { cfg.border_shading_strength = std::clamp(std::stof(v), 0.0f, 1.0f); } catch (...) {}
                    }
                } else if (top == "general") {
                    if (k == "smart_gaps") {
                        cfg.smart_gaps = (v == "true" || v == "1" || v == "yes");
                    } else if (k == "resize_on_border") {
                        cfg.resize_on_border = (v == "true" || v == "1" || v == "yes");
                    } else if (k == "border_grab_area") {
                        try { cfg.border_grab_area = std::clamp(std::stoi(v), 1, 30); } catch (...) {}
                    }
                }
            }
        }
    };

    fs::path miqu_dir = fs::path(home) / ".config" / "miquland";
    std::error_code ec;
    bool has_user = fs::exists(miqu_dir / "window_gaps_border.conf", ec);
    if (has_user) {
        parse_file(miqu_dir / "miquland.conf");
        parse_file(miqu_dir / "window_gaps_border.conf");
    } else {
        for (const char* p : {"/usr/share/miquland/window_gaps_border.conf", "/usr/local/share/miquland/window_gaps_border.conf", "assets/window_gaps_border.conf"}) {
            if (fs::exists(p)) {
                parse_file(p);
                break;
            }
        }
    }

    auto match_active = [&](const std::string& val) -> std::string {
        if (val.find("#ffff00 #ff0000") != std::string::npos) return "Yellow-Red Gradient (45°)";
        if (val.find("#00ffcc #0066ff") != std::string::npos) return "Cyan-Blue Gradient (45°)";
        if (val.find("#ff007f") != std::string::npos) return "Purple-Blue Gradient (135°)";
        if (val.find("#00ff88") != std::string::npos) return "Neon Emerald-Cyan (90°)";
        if (val == "#ffffff" || val == "#FFFFFF") return "Monochrome Bright (#FFFFFF)";
        if (val == "#3584e4") return "Sky Blue (#3584e4)";
        if (val == "#ff6b6b") return "Coral Flame (#ff6b6b)";
        return "Custom (" + val + ")";
    };

    auto match_inactive = [&](const std::string& val) -> std::string {
        if (val.find("#334455 #222d38") != std::string::npos) return "Dark Slate Gradient (45°)";
        if (val == "#99c2ff") return "Soft Slate (#99c2ff)";
        if (val == "#2d3748" || val == "#2D3748") return "Graphite Charcoal (#2D3748)";
        if (val.find("#aa9900") != std::string::npos) return "Muted Amber-Red (45°)";
        if (val.find("ffffff33") != std::string::npos) return "Subtle Outline (20% Opacity)";
        if (val.find("00000000") != std::string::npos) return "Transparent (Zero Border)";
        return "Custom (" + val + ")";
    };

    cfg.active_border_preset = match_active(cfg.active_border_value);
    if (cfg.active_border_preset.rfind("Custom", 0) == 0) {
        cfg.active_border_presets.insert(cfg.active_border_presets.begin(), cfg.active_border_preset);
    }

    cfg.inactive_border_preset = match_inactive(cfg.inactive_border_value);
    if (cfg.inactive_border_preset.rfind("Custom", 0) == 0) {
        cfg.inactive_border_presets.insert(cfg.inactive_border_presets.begin(), cfg.inactive_border_preset);
    }

    cfg.loaded = true;
}

static void save_gaps_border_config(const GapsBorderConfig& cfg) {
    const char* home = std::getenv("HOME");
    if (!home) return;

    fs::path miqu_dir = fs::path(home) / ".config" / "miquland";
    std::error_code ec;
    if (!fs::exists(miqu_dir, ec) || !fs::exists(miqu_dir / "window_gaps_border.conf", ec)) {
        // Fresh install: initialize complete standard configuration via miquland
        std::system("miquland --init-config >/dev/null 2>&1");
    }
    if (!fs::exists(miqu_dir, ec)) {
        fs::create_directories(miqu_dir, ec);
    }

    fs::path gaps_border_conf = miqu_dir / "window_gaps_border.conf";
    fs::path miquland_conf = miqu_dir / "miquland.conf";

    // 1. Always write/update window_gaps_border.conf
    {
        std::ostringstream ss;
        ss << "# ==============================================================================\n"
           << "# window_gaps_border.conf — Window Borders, Gaps & Geometry (Modular Sourced File)\n"
           << "# ==============================================================================\n"
           << "general {\n"
           << "    smart_gaps = " << (cfg.smart_gaps ? "true" : "false") << "\n"
           << "    resize_on_border = " << (cfg.resize_on_border ? "true" : "false") << "\n"
           << "    border_grab_area = " << cfg.border_grab_area << "\n\n"
           << "    gaps {\n"
           << "        # Inner gap: Spacing between adjacent tiled windows (in pixels)\n"
           << "        inner = " << cfg.inner_gap << "\n\n"
           << "        # Outer gap: Margin between windows and monitor screen edges (in pixels)\n"
           << "        outer = " << cfg.outer_gap << "\n"
           << "    }\n\n"
           << "    border {\n"
           << "        # Border thickness in pixels (0 disables window borders)\n"
           << "        width = " << cfg.border_width << "\n\n"
           << "        # Corner radius rounding for windows (in pixels)\n"
           << "        radius = " << cfg.corner_radius << "\n\n"
           << "        # Border color for active focused window:\n"
           << "        active = " << cfg.active_border_value << "\n\n"
           << "        # Border color for inactive unfocused windows:\n"
           << "        inactive = " << cfg.inactive_border_value << "\n\n"
           << "        # Border shading: subtle 3D lighting / specular bevel depth across the border stroke\n"
           << "        shading = " << (cfg.border_shading ? "true" : "false") << "\n\n"
           << "        # Border shading intensity (range: 0.0 subtle to 1.0 strong, default: 0.35)\n"
           << "        shading_strength = " << cfg.border_shading_strength << "\n"
           << "    }\n"
           << "}\n";

        std::string content = ss.str();
        std::ofstream out(gaps_border_conf);
        if (out.is_open()) {
            out << content;
            out.flush();
        }
    }

    // 2. If miquland.conf exists and has an inline gaps/border block or smart_gaps, update them as well
    if (fs::is_regular_file(miquland_conf, ec)) {
        std::ifstream in(miquland_conf);
        if (in.is_open()) {
            std::vector<std::string> lines;
            std::string line;
            std::vector<std::string> stack;
            bool modified = false;

            while (std::getline(in, line)) {
                std::string trimmed = line;
                size_t s = trimmed.find_first_not_of(" \t");
                std::string t = (s != std::string::npos) ? trimmed.substr(s) : "";

                size_t c_pos = t.find('#');
                std::string no_comment = (c_pos != std::string::npos) ? t.substr(0, c_pos) : t;
                while (!no_comment.empty() && (no_comment.back() == ' ' || no_comment.back() == '\t')) no_comment.pop_back();

                if (!no_comment.empty() && no_comment.back() == '{') {
                    std::string sec = no_comment.substr(0, no_comment.size() - 1);
                    while (!sec.empty() && (sec.back() == ' ' || sec.back() == '\t')) sec.pop_back();
                    stack.push_back(sec);
                    lines.push_back(line);
                    continue;
                }
                if (!no_comment.empty() && no_comment.front() == '}') {
                    if (!stack.empty()) stack.pop_back();
                    lines.push_back(line);
                    continue;
                }

                size_t eq = no_comment.find('=');
                if (eq != std::string::npos) {
                    std::string k = no_comment.substr(0, eq);
                    while (!k.empty() && (k.back() == ' ' || k.back() == '\t')) k.pop_back();
                    while (!k.empty() && (k.front() == ' ' || k.front() == '\t')) k.erase(0, 1);


                    std::string top = stack.empty() ? "" : stack.back();
                    if (top == "gaps") {
                        if (k == "inner") {
                            lines.push_back("        inner = " + std::to_string(cfg.inner_gap));
                            modified = true;
                            continue;
                        } else if (k == "outer") {
                            lines.push_back("        outer = " + std::to_string(cfg.outer_gap));
                            modified = true;
                            continue;
                        }
                    } else if (top == "border") {
                        if (k == "width") {
                            lines.push_back("        width = " + std::to_string(cfg.border_width));
                            modified = true;
                            continue;
                        } else if (k == "radius") {
                            lines.push_back("        radius = " + std::to_string(cfg.corner_radius));
                            modified = true;
                            continue;
                        } else if (k == "active") {
                            lines.push_back("        active = " + cfg.active_border_value);
                            modified = true;
                            continue;
                        } else if (k == "inactive") {
                            lines.push_back("        inactive = " + cfg.inactive_border_value);
                            modified = true;
                            continue;
                        } else if (k == "shading") {
                            lines.push_back(std::string("        shading = ") + (cfg.border_shading ? "true" : "false"));
                            modified = true;
                            continue;
                        } else if (k == "shading_strength") {
                            lines.push_back("        shading_strength = " + std::to_string(cfg.border_shading_strength));
                            modified = true;
                            continue;
                        }
                    } else if (top == "general") {
                        if (k == "smart_gaps") {
                            lines.push_back(std::string("    smart_gaps = ") + (cfg.smart_gaps ? "true" : "false"));
                            modified = true;
                            continue;
                        } else if (k == "resize_on_border") {
                            lines.push_back(std::string("    resize_on_border = ") + (cfg.resize_on_border ? "true" : "false"));
                            modified = true;
                            continue;
                        } else if (k == "border_grab_area") {
                            lines.push_back("    border_grab_area = " + std::to_string(cfg.border_grab_area));
                            modified = true;
                            continue;
                        }
                    }
                }

                lines.push_back(line);
            }
            in.close();

            if (modified) {
                std::ofstream out(miquland_conf);
                if (out.is_open()) {
                    for (const auto& l : lines) out << l << "\n";
                    out.flush();
                }
            }
        }
    }
}

void MillerView::render_gaps_border_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    if (!item || !item->gaps_border_cfg) return;
    auto& cfg = *item->gaps_border_cfg;
    load_gaps_border_config(cfg);

    // --- 1. VISUAL INTERACTIVE PREVIEW CARD ---
    auto preview_container = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto header_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto p_title = TextViewBuilder::create()
        ->text("Interactive Layout Simulation")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    header_row->add_view(p_title);
    preview_container->add_view(header_row);

    // Miniature Desktop Stage (Aspect representation)
    int stage_outer_pad = std::clamp(static_cast<int>(std::round(cfg.outer_gap * 0.6f)), 4, 30);
    int stage_inner_gap = std::clamp(static_cast<int>(std::round(cfg.inner_gap * 0.6f)), 2, 26);
    int stage_border_w = std::clamp(cfg.border_width, 0, 7);
    int stage_radius = std::clamp(static_cast<int>(std::round(cfg.corner_radius * 0.6f)), 0, 18);

    Color active_color = parse_border_display_color(cfg.active_border_value, Color(1.0f, 0.8f, 0.0f));
    Color inactive_color = parse_border_display_color(cfg.inactive_border_value, config->colors.outline.with_alpha(0.45f));

    auto stage_frame = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(stage_outer_pad, stage_outer_pad)
        ->spacing(stage_inner_gap)
        ->backgroundColor(config->colors.on_surface.with_alpha(0.04f))
        ->stroke(1, config->colors.outline.with_alpha(0.18f))
        ->cornerRadius(get_inner_radius(config))
        ->build();

    // 1.1 Left Window (Active / Focused)
    auto active_win = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(10, 8)
        ->spacing(6)
        ->backgroundColor(config->colors.surface)
        ->stroke(stage_border_w, active_color)
        ->cornerRadius(stage_radius)
        ->build();

    auto act_tb = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(5)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto dot_close = LinearLayoutBuilder::create()
        ->backgroundColor(Color(0.95f, 0.35f, 0.35f))
        ->cornerRadius(4)
        ->build();
    act_tb->add_view(dot_close, LayoutParams(7, 7));

    auto dot_min = LinearLayoutBuilder::create()
        ->backgroundColor(Color(0.95f, 0.75f, 0.25f))
        ->cornerRadius(4)
        ->build();
    act_tb->add_view(dot_min, LayoutParams(7, 7));

    auto dot_max = LinearLayoutBuilder::create()
        ->backgroundColor(Color(0.35f, 0.85f, 0.45f))
        ->cornerRadius(4)
        ->build();
    act_tb->add_view(dot_max, LayoutParams(7, 7));

    auto act_title = TextViewBuilder::create()
        ->text("Active Window")
        ->caption(true)
        ->bold(true)
        ->textColor(config->colors.on_surface)
        ->build();
    act_tb->add_view(act_title);
    active_win->add_view(act_tb);

    auto act_line1 = LinearLayoutBuilder::create()
        ->backgroundColor(config->colors.on_surface.with_alpha(0.12f))
        ->cornerRadius(3)
        ->build();
    active_win->add_view(act_line1, LayoutParams(static_cast<int>(LayoutDimension::MatchParent), 8));

    auto act_line2 = LinearLayoutBuilder::create()
        ->backgroundColor(active_color.with_alpha(0.35f))
        ->cornerRadius(3)
        ->build();
    active_win->add_view(act_line2, LayoutParams(110, 8));

    auto act_line3 = LinearLayoutBuilder::create()
        ->backgroundColor(config->colors.on_surface.with_alpha(0.08f))
        ->cornerRadius(3)
        ->build();
    active_win->add_view(act_line3, LayoutParams(static_cast<int>(LayoutDimension::MatchParent), 8));

    // 1.2 Right Window (Inactive / Unfocused)
    auto inact_win = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(10, 8)
        ->spacing(6)
        ->backgroundColor(config->colors.surface.with_alpha(0.85f))
        ->stroke(stage_border_w, inactive_color)
        ->cornerRadius(stage_radius)
        ->build();

    auto inact_tb = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(5)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto inact_dot = LinearLayoutBuilder::create()
        ->backgroundColor(config->colors.outline.with_alpha(0.4f))
        ->cornerRadius(4)
        ->build();
    inact_tb->add_view(inact_dot, LayoutParams(7, 7));

    auto inact_title = TextViewBuilder::create()
        ->text("Inactive Window")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    inact_tb->add_view(inact_title);
    inact_win->add_view(inact_tb);

    auto inact_line1 = LinearLayoutBuilder::create()
        ->backgroundColor(config->colors.on_surface.with_alpha(0.08f))
        ->cornerRadius(3)
        ->build();
    inact_win->add_view(inact_line1, LayoutParams(static_cast<int>(LayoutDimension::MatchParent), 8));

    auto inact_line2 = LinearLayoutBuilder::create()
        ->backgroundColor(config->colors.on_surface.with_alpha(0.06f))
        ->cornerRadius(3)
        ->build();
    inact_win->add_view(inact_line2, LayoutParams(95, 8));

    auto inact_line3 = LinearLayoutBuilder::create()
        ->backgroundColor(config->colors.on_surface.with_alpha(0.08f))
        ->cornerRadius(3)
        ->build();
    inact_win->add_view(inact_line3, LayoutParams(static_cast<int>(LayoutDimension::MatchParent), 8));

    LayoutParams win_lp(static_cast<int>(LayoutDimension::MatchParent), 115, 1.0f);
    stage_frame->add_view(active_win, win_lp);
    stage_frame->add_view(inact_win, win_lp);

    preview_container->add_view(stage_frame, LayoutParams(static_cast<int>(LayoutDimension::MatchParent), static_cast<int>(LayoutDimension::WrapContent)));

    // Footer info metrics
    std::string info_text = "Inner: " + std::to_string(cfg.inner_gap) + " px • Outer: " +
                            std::to_string(cfg.outer_gap) + " px • Border: " +
                            std::to_string(cfg.border_width) + " px • Radius: " +
                            std::to_string(cfg.corner_radius) + " px";
    if (cfg.smart_gaps) info_text += " • Smart Gaps: On";

    auto p_info = TextViewBuilder::create()
        ->text(info_text)
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    preview_container->add_view(p_info);

    add_preview_card(preview_container);

    // --- 2. CONFIGURATION FORM CONTROLS ---
    auto main_card = create_form_card(config);

    // 2.1 Inner Gap
    add_form_slider(
        main_card, config,
        "Inner window gap",
        "Spacing between adjacent tiled windows (in pixels)",
        static_cast<float>(cfg.inner_gap) / 48.0f,
        std::to_string(cfg.inner_gap) + " px",
        [this, item](float val) -> std::string {
            if (!item || !item->gaps_border_cfg) return "";
            auto& cfg = *item->gaps_border_cfg;
            int px = static_cast<int>(std::round(val * 48.0f));
            cfg.inner_gap = px;
            save_gaps_border_config(cfg);
            deferred_rebuild_preview(60);
            return std::to_string(px) + " px";
        }
    );

    // 2.2 Outer Gap
    add_form_slider(
        main_card, config,
        "Outer screen edge gap",
        "Margin between windows and monitor screen edges (in pixels)",
        static_cast<float>(cfg.outer_gap) / 64.0f,
        std::to_string(cfg.outer_gap) + " px",
        [this, item](float val) -> std::string {
            if (!item || !item->gaps_border_cfg) return "";
            auto& cfg = *item->gaps_border_cfg;
            int px = static_cast<int>(std::round(val * 64.0f));
            cfg.outer_gap = px;
            save_gaps_border_config(cfg);
            deferred_rebuild_preview(60);
            return std::to_string(px) + " px";
        }
    );

    // 2.3 Smart Gaps
    add_form_toggle(
        main_card, config,
        "Smart gaps",
        "Automatically collapse gaps and borders when only one window is tiled on screen",
        cfg.smart_gaps,
        [this, item](bool val) {
            if (!item || !item->gaps_border_cfg) return;
            auto& cfg = *item->gaps_border_cfg;
            cfg.smart_gaps = val;
            save_gaps_border_config(cfg);
            deferred_rebuild_preview(10);
        }
    );

    // 2.4 Border Width
    add_form_slider(
        main_card, config,
        "Window border width",
        "Border stroke thickness in pixels (0 disables window borders)",
        static_cast<float>(cfg.border_width) / 12.0f,
        cfg.border_width == 0 ? "Disabled (0 px)" : (std::to_string(cfg.border_width) + " px"),
        [this, item](float val) -> std::string {
            if (!item || !item->gaps_border_cfg) return "";
            auto& cfg = *item->gaps_border_cfg;
            int px = static_cast<int>(std::round(val * 12.0f));
            cfg.border_width = px;
            save_gaps_border_config(cfg);
            deferred_rebuild_preview(60);
            return px == 0 ? "Disabled (0 px)" : (std::to_string(px) + " px");
        }
    );

    // 2.5 Corner Radius
    add_form_slider(
        main_card, config,
        "Window corner radius",
        "Rounding curvature applied to window borders and backgrounds (in pixels)",
        static_cast<float>(cfg.corner_radius) / 36.0f,
        std::to_string(cfg.corner_radius) + " px",
        [this, item](float val) -> std::string {
            if (!item || !item->gaps_border_cfg) return "";
            auto& cfg = *item->gaps_border_cfg;
            int px = static_cast<int>(std::round(val * 36.0f));
            cfg.corner_radius = px;
            save_gaps_border_config(cfg);
            deferred_rebuild_preview(60);
            return std::to_string(px) + " px";
        }
    );

    // 2.6 Active Border Color
    int cur_act_idx = 0;
    for (size_t i = 0; i < cfg.active_border_presets.size(); ++i) {
        if (cfg.active_border_presets[i] == cfg.active_border_preset) {
            cur_act_idx = static_cast<int>(i);
            break;
        }
    }
    add_form_spinner(
        main_card, config,
        "Active window border color",
        "Border styling or linear gradient for the focused active window",
        cfg.active_border_presets,
        cur_act_idx,
        [this, item](int idx, const std::string& val) {
            (void)idx;
            if (!item || !item->gaps_border_cfg) return;
            auto& cfg = *item->gaps_border_cfg;
            cfg.active_border_preset = val;
            if (val.find("Yellow-Red") != std::string::npos) cfg.active_border_value = "#ffff00 #ff0000 45deg";
            else if (val.find("Cyan-Blue") != std::string::npos) cfg.active_border_value = "#00ffcc #0066ff 45deg";
            else if (val.find("Purple-Blue") != std::string::npos) cfg.active_border_value = "#ff007f #7928ca #0066ff 135deg";
            else if (val.find("Neon") != std::string::npos) cfg.active_border_value = "#00ff88 #00ccff 90deg";
            else if (val.find("Monochrome") != std::string::npos) cfg.active_border_value = "#ffffff";
            else if (val.find("Sky Blue") != std::string::npos) cfg.active_border_value = "#3584e4";
            else if (val.find("Coral") != std::string::npos) cfg.active_border_value = "#ff6b6b";
            save_gaps_border_config(cfg);
            deferred_rebuild_preview(10);
        }
    );

    // 2.7 Inactive Border Color
    int cur_inact_idx = 0;
    for (size_t i = 0; i < cfg.inactive_border_presets.size(); ++i) {
        if (cfg.inactive_border_presets[i] == cfg.inactive_border_preset) {
            cur_inact_idx = static_cast<int>(i);
            break;
        }
    }
    add_form_spinner(
        main_card, config,
        "Inactive window border color",
        "Border styling for unfocused background windows",
        cfg.inactive_border_presets,
        cur_inact_idx,
        [this, item](int idx, const std::string& val) {
            (void)idx;
            if (!item || !item->gaps_border_cfg) return;
            auto& cfg = *item->gaps_border_cfg;
            cfg.inactive_border_preset = val;
            if (val.find("Dark Slate") != std::string::npos) cfg.inactive_border_value = "#334455 #222d38 45deg";
            else if (val.find("Soft Slate") != std::string::npos) cfg.inactive_border_value = "#99c2ff";
            else if (val.find("Graphite") != std::string::npos) cfg.inactive_border_value = "#2d3748";
            else if (val.find("Muted Amber") != std::string::npos) cfg.inactive_border_value = "#aa9900 #aa3333 45deg";
            else if (val.find("Subtle") != std::string::npos) cfg.inactive_border_value = "#ffffff33";
            else if (val.find("Transparent") != std::string::npos) cfg.inactive_border_value = "#00000000";
            save_gaps_border_config(cfg);
            deferred_rebuild_preview(10);
        }
    );

    // 2.8 Border Specular Shading
    add_form_toggle(
        main_card, config,
        "Border specular shading",
        "Subtle 3D lighting and specular bevel depth across the border stroke",
        cfg.border_shading,
        [this, item](bool val) {
            if (!item || !item->gaps_border_cfg) return;
            auto& cfg = *item->gaps_border_cfg;
            cfg.border_shading = val;
            save_gaps_border_config(cfg);
            deferred_rebuild_preview(10);
        }
    );

    if (cfg.border_shading) {
        add_form_slider(
            main_card, config,
            "Shading intensity",
            "Specular bevel strength across the window frame",
            cfg.border_shading_strength,
            std::to_string(static_cast<int>(cfg.border_shading_strength * 100)) + "%",
            [item](float val) -> std::string {
                if (!item || !item->gaps_border_cfg) return "";
                auto& cfg = *item->gaps_border_cfg;
                cfg.border_shading_strength = val;
                save_gaps_border_config(cfg);
                return std::to_string(static_cast<int>(val * 100)) + "%";
            }
        );
    }

    // 2.9 Direct Border Drag Resizing
    add_form_toggle(
        main_card, config,
        "Direct border drag resizing",
        "Resize windows by dragging directly on window borders without modifier key",
        cfg.resize_on_border,
        [this, item](bool val) {
            if (!item || !item->gaps_border_cfg) return;
            auto& cfg = *item->gaps_border_cfg;
            cfg.resize_on_border = val;
            save_gaps_border_config(cfg);
            deferred_rebuild_preview(10);
        }
    );

    if (cfg.resize_on_border) {
        float grab_norm = static_cast<float>(cfg.border_grab_area - 1) / 23.0f;
        add_form_slider(
            main_card, config,
            "Border grab area",
            "Hit-test grab margin in pixels along window borders for resizing",
            grab_norm,
            std::to_string(cfg.border_grab_area) + " px",
            [item](float val) -> std::string {
                if (!item || !item->gaps_border_cfg) return "";
                auto& cfg = *item->gaps_border_cfg;
                int px = 1 + static_cast<int>(std::round(val * 23.0f));
                cfg.border_grab_area = px;
                save_gaps_border_config(cfg);
                return std::to_string(px) + " px";
            }
        );
    }

    add_preview_card(main_card);
}


static void load_login_screen_config(LoginScreenConfig& cfg) {
    if (cfg.is_loaded) return;
    cfg.is_loaded = true;

    cfg.available_sessions.clear();
    std::vector<std::string> session_dirs = {"/usr/share/wayland-sessions", "/usr/share/xsessions"};
    for (const auto& sdir : session_dirs) {
        if (fs::exists(sdir)) {
            for (const auto& entry : fs::directory_iterator(sdir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".desktop") {
                    std::string sname = entry.path().stem().string();
                    if (std::find(cfg.available_sessions.begin(), cfg.available_sessions.end(), sname) == cfg.available_sessions.end()) {
                        cfg.available_sessions.push_back(sname);
                    }
                }
            }
        }
    }
    if (cfg.available_sessions.empty()) {
        cfg.available_sessions = {"miquland", "hyprland"};
    }

    std::string conf_path;
    if (fs::exists("/etc/miqudm/miqudm.conf")) {
        conf_path = "/etc/miqudm/miqudm.conf";
    } else if (fs::exists("/usr/share/miqudm/miqudm.conf")) {
        conf_path = "/usr/share/miqudm/miqudm.conf";
    }

    if (conf_path.empty()) return;

    std::ifstream file(conf_path);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) continue;
        size_t last = line.find_last_not_of(" \t\r\n");
        line = line.substr(first, (last - first + 1));
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        if (line.front() == '[' && line.back() == ']') continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        auto trim_str = [](std::string s) {
            size_t f = s.find_first_not_of(" \t\r\n");
            if (f == std::string::npos) return std::string();
            size_t l = s.find_last_not_of(" \t\r\n");
            return s.substr(f, (l - f + 1));
        };
        key = trim_str(key);
        val = trim_str(val);
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);

        auto parse_bool = [](const std::string& v) {
            std::string lower = v;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            return (lower == "true" || lower == "1" || lower == "yes" || lower == "on");
        };

        if (key == "wallpaper") {
            cfg.wallpaper = val;
        } else if (key == "clock_format_24h") {
            cfg.clock_format_24h = parse_bool(val);
        } else if (key == "show_power_buttons") {
            cfg.show_power_buttons = parse_bool(val);
        } else if (key == "show_sessions") {
            cfg.show_sessions = parse_bool(val);
        } else if (key == "allow_user_switching") {
            cfg.allow_user_switching = parse_bool(val);
        } else if (key == "card_radius") {
            try { cfg.card_radius = std::clamp(std::stoi(val), 0, 48); } catch (...) {}
        } else if (key == "avatar_size") {
            try { cfg.avatar_size = std::clamp(std::stoi(val), 32, 160); } catch (...) {}
        } else if (key == "default_session") {
            cfg.default_session = val;
        } else if (key == "remember_last_user") {
            cfg.remember_last_user = parse_bool(val);
        } else if (key == "numlock") {
            cfg.numlock = parse_bool(val);
        }
    }
}

void MillerView::render_login_screen_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    if (!item || !item->login_screen_cfg) return;
    auto& cfg = *item->login_screen_cfg;
    load_login_screen_config(cfg);

    // --- 1. Top Preview Card: Mini Mockup of Login Screen (320x180 px) ---
    auto preview_container = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(12, 12)
        ->spacing(8)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto stage_frame = FrameLayoutBuilder::create()
        ->cornerRadius(get_inner_radius(config))
        ->stroke(1, config->colors.outline.with_alpha(0.35f))
        ->backgroundColor(config->colors.surface_variant.with_alpha(0.5f))
        ->build();

    std::string exp_wp = expand_wallpaper_path(cfg.wallpaper);
    if (!exp_wp.empty() && fs::exists(exp_wp)) {
        auto wp_view = ImageViewBuilder::create()
            ->source(exp_wp)
            ->fitMode(FitMode::Cover)
            ->cornerRadius(get_inner_radius(config))
            ->build();
        LayoutParams bg_lp(320, 180);
        stage_frame->add_view(wp_view, bg_lp);
    }

    int scaled_card_radius = std::clamp(cfg.card_radius * 12 / 20, 4, 16);
    int scaled_avatar_size = std::clamp(cfg.avatar_size * 36 / 84, 24, 48);

    auto mini_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->gravity(Gravity::Center)
        ->padding(12, 8)
        ->spacing(4)
        ->backgroundColor(config->colors.surface.with_alpha(0.92f))
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(scaled_card_radius)
        ->build();

    std::string clock_str = cfg.clock_format_24h ? "14:30" : "02:30 PM";
    auto clock_tv = TextViewBuilder::create()
        ->text(clock_str)
        ->textSize(static_cast<int>(config->metrics.font_size * 1.15f))
        ->bold(true)
        ->textColor(config->colors.on_surface)
        ->center(true)
        ->build();
    mini_card->add_view(clock_tv);

    auto avatar_iv = ImageViewBuilder::create()
        ->source("avatar-default")
        ->targetSize(scaled_avatar_size)
        ->fitMode(FitMode::Contain)
        ->alpha(0.9f)
        ->build();
    LayoutParams av_lp(scaled_avatar_size, scaled_avatar_size);
    av_lp.gravity = Gravity::CenterHorizontal;
    mini_card->add_view(avatar_iv, av_lp);

    auto user_tv = TextViewBuilder::create()
        ->text("User")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant)
        ->center(true)
        ->build();
    mini_card->add_view(user_tv);

    auto pass_chip = TextViewBuilder::create()
        ->text("••••••••")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.4f))
        ->center(true)
        ->build();
    mini_card->add_view(pass_chip);

    LayoutParams mini_card_lp(150, static_cast<int>(LayoutDimension::WrapContent));
    mini_card_lp.gravity = Gravity::Center;
    stage_frame->add_view(mini_card, mini_card_lp);

    auto bottom_indicator_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->gravity(Gravity::CenterVertical)
        ->padding(10, 4)
        ->backgroundColor(Color::transparent())
        ->build();

    if (cfg.show_power_buttons) {
        auto power_tv = TextViewBuilder::create()
            ->text("⏻  ↻  ☾")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.85f))
            ->build();
        bottom_indicator_row->add_view(power_tv);
    }

    auto spacer = LinearLayoutBuilder::create()->build();
    LayoutParams sp_lp(static_cast<int>(LayoutDimension::MatchParent), 1, 1.0f);
    bottom_indicator_row->add_view(spacer, sp_lp);

    if (cfg.show_sessions) {
        auto sess_tv = TextViewBuilder::create()
            ->text(cfg.default_session.empty() ? "miquland" : cfg.default_session)
            ->caption(true)
            ->textColor(config->colors.primary)
            ->build();
        bottom_indicator_row->add_view(sess_tv);
    }

    LayoutParams bir_lp(static_cast<int>(LayoutDimension::MatchParent), static_cast<int>(LayoutDimension::WrapContent));
    bir_lp.gravity = Gravity::Bottom | Gravity::CenterHorizontal;
    stage_frame->add_view(bottom_indicator_row, bir_lp);

    LayoutParams stage_lp(320, 180);
    stage_lp.gravity = Gravity::CenterHorizontal;
    preview_container->add_view(stage_frame, stage_lp);

    std::string wp_display = cfg.wallpaper.empty()
        ? "Background: Solid tone"
        : "Background: " + fs::path(cfg.wallpaper).filename().string();
    auto wp_label = TextViewBuilder::create()
        ->text(wp_display)
        ->caption(true)
        ->textColor(config->colors.on_surface_variant)
        ->center(true)
        ->build();
    LayoutParams wp_lp(static_cast<int>(LayoutDimension::MatchParent), static_cast<int>(LayoutDimension::WrapContent));
    wp_lp.gravity = Gravity::CenterHorizontal;
    preview_container->add_view(wp_label, wp_lp);

    add_preview_card(preview_container);

    // --- 2. Form Card 1: Appearance & Visual Metrics ---
    auto visual_card = create_form_card(config);

    auto wp_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->gravity(Gravity::CenterVertical)
        ->padding(12, 6)
        ->backgroundColor(Color::transparent())
        ->build();

    auto wp_title = TextViewBuilder::create()
        ->text("Custom Wallpaper")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    LayoutParams wpt_lp(static_cast<int>(LayoutDimension::MatchParent), static_cast<int>(LayoutDimension::WrapContent), 1.0f);
    wp_row->add_view(wp_title, wpt_lp);

    auto on_browse_wp = [this, item]() {
        std::thread([this, item]() {
            std::string cmd;
            if (system("which zenity >/dev/null 2>&1") == 0) {
                cmd = "zenity --file-selection --title=\"Select Login Wallpaper\" "
                      "--file-filter=\"Images | *.png *.jpg *.jpeg *.webp *.svg *.bmp *.avif *.jxl\" 2>/dev/null";
            } else if (system("which kdialog >/dev/null 2>&1") == 0) {
                cmd = "kdialog --getopenfilename : \"Images (*.png *.jpg *.jpeg *.webp *.svg *.bmp *.avif *.jxl)\" 2>/dev/null";
            }
            if (!cmd.empty()) {
                FILE* fp = popen(cmd.c_str(), "r");
                if (fp) {
                    char buf[1024];
                    std::string picked = "";
                    if (fgets(buf, sizeof(buf), fp)) {
                        picked = buf;
                        while (!picked.empty() && (picked.back() == '\n' || picked.back() == '\r')) {
                            picked.pop_back();
                        }
                    }
                    pclose(fp);
                    if (!picked.empty() && fs::exists(picked)) {
                        item->login_screen_cfg->wallpaper = picked;
                        deferred_rebuild_preview(10);
                    }
                }
            }
        }).detach();
    };

    auto browse_btn = ButtonBuilder::create()
        ->text("Browse")
        ->padding(12, 4)
        ->cornerRadius(get_btn_radius(config))
        ->onClick(on_browse_wp)
        ->build();
    LayoutParams b_lp(static_cast<int>(LayoutDimension::WrapContent), 32);
    wp_row->add_view(browse_btn, b_lp);

    if (!cfg.wallpaper.empty()) {
        auto on_clear_wp = [this, item]() {
            item->login_screen_cfg->wallpaper = "";
            deferred_rebuild_preview(10);
        };
        auto clear_btn = ButtonBuilder::create()
            ->text("Clear")
            ->padding(10, 4)
            ->cornerRadius(get_btn_radius(config))
            ->onClick(on_clear_wp)
            ->build();
        LayoutParams c_lp(static_cast<int>(LayoutDimension::WrapContent), 32);
        wp_row->add_view(clear_btn, c_lp);
    }
    visual_card->add_view(wp_row);

    add_form_toggle(
        visual_card, config,
        "24-Hour Clock",
        "",
        cfg.clock_format_24h,
        [this, item](bool val) {
            item->login_screen_cfg->clock_format_24h = val;
            deferred_rebuild_preview(10);
        }
    );

    add_form_slider(
        visual_card, config,
        "Card Corner Radius",
        "",
        static_cast<float>(cfg.card_radius - 8) / 24.0f,
        std::to_string(cfg.card_radius) + " px",
        [this, item](float norm) -> std::string {
            int r = 8 + static_cast<int>(std::round(norm * 24.0f));
            item->login_screen_cfg->card_radius = r;
            deferred_rebuild_preview(50);
            return std::to_string(r) + " px";
        }
    );

    add_form_slider(
        visual_card, config,
        "Avatar Size",
        "",
        static_cast<float>(cfg.avatar_size - 48) / 72.0f,
        std::to_string(cfg.avatar_size) + " px",
        [this, item](float norm) -> std::string {
            int s = 48 + static_cast<int>(std::round(norm * 72.0f));
            item->login_screen_cfg->avatar_size = s;
            deferred_rebuild_preview(50);
            return std::to_string(s) + " px";
        }
    );

    add_preview_card(visual_card);

    // --- 3. Form Card 2: Session & Behavior ---
    auto session_card = create_form_card(config);

    int cur_sess_idx = 0;
    for (size_t i = 0; i < cfg.available_sessions.size(); ++i) {
        if (cfg.available_sessions[i] == cfg.default_session) {
            cur_sess_idx = static_cast<int>(i);
            break;
        }
    }
    add_form_spinner(
        session_card, config,
        "Default Session",
        "",
        cfg.available_sessions,
        cur_sess_idx,
        [this, item](int idx, const std::string& val) {
            (void)idx;
            item->login_screen_cfg->default_session = val;
            deferred_rebuild_preview(10);
        }
    );

    add_form_toggle(
        session_card, config,
        "Show Power Actions",
        "",
        cfg.show_power_buttons,
        [this, item](bool val) {
            item->login_screen_cfg->show_power_buttons = val;
            deferred_rebuild_preview(10);
        }
    );

    add_form_toggle(
        session_card, config,
        "Show Session Selector",
        "",
        cfg.show_sessions,
        [this, item](bool val) {
            item->login_screen_cfg->show_sessions = val;
            deferred_rebuild_preview(10);
        }
    );

    add_form_toggle(
        session_card, config,
        "Allow User Switching",
        "",
        cfg.allow_user_switching,
        [item](bool val) {
            item->login_screen_cfg->allow_user_switching = val;
        }
    );

    add_form_toggle(
        session_card, config,
        "Remember Last User",
        "",
        cfg.remember_last_user,
        [item](bool val) {
            item->login_screen_cfg->remember_last_user = val;
        }
    );

    add_form_toggle(
        session_card, config,
        "Enable NumLock on Startup",
        "",
        cfg.numlock,
        [item](bool val) {
            item->login_screen_cfg->numlock = val;
        }
    );

    add_preview_card(session_card);
}

} // namespace miqu
