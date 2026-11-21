#include "config.hpp"
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/core/fs_utils.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <filesystem>
#include <climits>
#include <unistd.h>

namespace miqubg {

namespace fs = std::filesystem;

static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n\"'");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n\"'");
    return str.substr(first, (last - first + 1));
}

Config& Config::get() {
    static Config instance;
    return instance;
}

Config::Config() {
    m_default_config.mode = miqu::FitMode::Cover;
    m_default_config.bg_color = miqu::Color::rgb(0.0f, 0.0f, 0.0f);
    m_default_config.image_path = WallpaperManager::resolve_default_wallpaper();
}

std::string Config::get_user_config_path() {
    return miqu::FsUtils::get_user_config_file("miqubg", "miqubg.conf");
}

std::string Config::get_system_config_path() {
    return miqu::FsUtils::get_system_config_file("miqubg", "miqubg.conf");
}

std::string Config::expand_path(const std::string& path) {
    return miqu::FsUtils::expand_user_path(path);
}

std::string Config::mode_to_string(miqu::FitMode mode) {
    switch (mode) {
        case miqu::FitMode::Contain: return "contain";
        case miqu::FitMode::Fill: return "stretch";
        case miqu::FitMode::Center: return "center";
        case miqu::FitMode::Tile: return "tile";
        case miqu::FitMode::Cover:
        default:
            return "cover";
    }
}

miqu::FitMode Config::string_to_mode(const std::string& str) {
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (s == "fit" || s == "contain") return miqu::FitMode::Contain;
    if (s == "stretch" || s == "fill") return miqu::FitMode::Fill;
    if (s == "center") return miqu::FitMode::Center;
    if (s == "tile") return miqu::FitMode::Tile;
    return miqu::FitMode::Cover;
}

std::string Config::color_to_hex(const miqu::Color& c) {
    int r = static_cast<int>(std::clamp(c.r * 255.0f, 0.0f, 255.0f));
    int g = static_cast<int>(std::clamp(c.g * 255.0f, 0.0f, 255.0f));
    int b = static_cast<int>(std::clamp(c.b * 255.0f, 0.0f, 255.0f));
    std::ostringstream oss;
    oss << "#" << std::hex << std::setfill('0')
        << std::setw(2) << r
        << std::setw(2) << g
        << std::setw(2) << b;
    return oss.str();
}

miqu::Color Config::hex_to_color(const std::string& hex, const miqu::Color& fallback) {
    return miqu::Color::from_hex(hex, fallback);
}

OutputConfig Config::get_config_for_output(const std::string& output) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (output.empty() || output == "*") {
        return m_default_config;
    }
    auto it = m_output_configs.find(output);
    if (it != m_output_configs.end()) {
        return it->second;
    }
    return m_default_config;
}

bool Config::consume_internal_write_flag() {
    auto now = std::chrono::steady_clock::now();
    if (m_internal_write_flag && (now - m_internal_write_time < std::chrono::milliseconds(1000))) {
        m_internal_write_flag = false;
        return true;
    }
    m_internal_write_flag = false;
    return false;
}

std::string Config::init_user_config() {
    std::string res = miqu::Config::init_user_config("miqubg", "miqubg.conf");
    if (!res.empty()) {
        return res;
    }

    // Fallback if system baseline file is not yet installed in system prefix
    std::string user_conf = get_user_config_path();
    if (user_conf.empty()) return "";

    std::error_code ec;
    fs::create_directories(fs::path(user_conf).parent_path(), ec);

    // Write baseline template
    std::ofstream out(user_conf);
    if (out.is_open()) {
        out << "# ==============================================================================\n"
            << "# miqubg Configuration File\n"
            << "# Independent Wayland Wallpaper Daemon for Miquland\n"
            << "# ==============================================================================\n"
            << "\n"
            << "[general]\n"
            << "# Default wallpaper applied across all monitors\n"
            << "# Syntax: wallpaper = [monitor], <path>, [fit_mode], [bg_color]\n"
            << "wallpaper = , /usr/share/miqubg/wallpaper.png, cover, #000000\n"
            << "\n"
            << "# Per-monitor overrides:\n"
            << "# wallpaper = HDMI-A-1, /usr/share/miqubg/wallpaper.png, cover, #000000\n"
            << "# wallpaper = eDP-1, /usr/share/miqubg/wallpaper.png, fit, #1e1e2e\n";
        return user_conf;
    }

    return "";
}

bool Config::load(const std::string& custom_path) {
    std::string target_path = custom_path;
    if (target_path.empty()) {
        std::string user_conf = get_user_config_path();
        if (!user_conf.empty() && fs::exists(user_conf)) {
            target_path = user_conf;
        } else {
            std::string sys_conf = get_system_config_path();
            if (!sys_conf.empty() && fs::exists(sys_conf)) {
                target_path = sys_conf;
            } else {
                // Check relative asset locations
                std::vector<std::string> candidates = {
                    "assets/miqubg.conf",
                    "../assets/miqubg.conf",
                    "shell/miqubg/assets/miqubg.conf"
                };
                for (const auto& c : candidates) {
                    if (fs::exists(c)) {
                        target_path = c;
                        break;
                    }
                }
            }
        }
    }

    if (target_path.empty() || !fs::exists(target_path)) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config_path = target_path;
        m_loaded_files.clear();
        m_output_configs.clear();
        m_default_config.mode = miqu::FitMode::Cover;
        m_default_config.bg_color = miqu::Color::rgb(0.0f, 0.0f, 0.0f);
        m_default_config.image_path = WallpaperManager::resolve_default_wallpaper();
    }

    // Register with miqutoolkit Config so setup_config_watcher() tracks loaded files
    miqu::Config::get()->load_from_file(target_path);

    load_file_internal(target_path, 0);
    return true;
}

void Config::load_file_internal(const std::string& raw_path, int depth) {
    if (depth > 10) return;

    std::string expanded = expand_path(raw_path);
    if (!fs::exists(expanded)) return;

    std::string canonical;
    try {
        canonical = fs::canonical(expanded).string();
    } catch (...) {
        canonical = expanded;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (std::find(m_loaded_files.begin(), m_loaded_files.end(), canonical) == m_loaded_files.end()) {
            m_loaded_files.push_back(canonical);
        }
    }

    std::ifstream file(canonical);
    if (!file.is_open()) return;

    std::string current_section = "general";
    std::string line;

    while (std::getline(file, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') continue;

        // Section header [name]
        if (trimmed.front() == '[' && trimmed.back() == ']') {
            current_section = trim(trimmed.substr(1, trimmed.size() - 2));
            std::transform(current_section.begin(), current_section.end(), current_section.begin(), ::tolower);
            if (current_section.rfind("output:", 0) == 0) {
                current_section = current_section.substr(7);
            } else if (current_section.rfind("monitor:", 0) == 0) {
                current_section = current_section.substr(8);
            }
            continue;
        }

        size_t eq = trimmed.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(trimmed.substr(0, eq));
        std::string val = trim(trimmed.substr(eq + 1));
        std::string key_lower = key;
        std::transform(key_lower.begin(), key_lower.end(), key_lower.begin(), ::tolower);

        // Include / Source directive
        if (key_lower == "source" || key_lower == "include") {
            load_file_internal(val, depth + 1);
            continue;
        }

        // Hyprland / unified wallpaper line:
        // wallpaper = [monitor], <path>, [fit_mode], [bg_color]
        // monitor = <monitor>, <path>, [fit_mode], [bg_color]
        if (key_lower == "wallpaper" || key_lower == "monitor") {
            // Check if value is comma-separated
            std::vector<std::string> tokens;
            std::istringstream ss(val);
            std::string tok;
            while (std::getline(ss, tok, ',')) {
                tokens.push_back(trim(tok));
            }

            std::string target_output = "";
            std::string img_path = "";
            miqu::FitMode mode = miqu::FitMode::Cover;
            miqu::Color col = miqu::Color::rgb(0.0f, 0.0f, 0.0f);

            if (tokens.size() == 1) {
                // Single token: wallpaper path for current_section
                img_path = expand_path(tokens[0]);
                if (current_section != "general") {
                    target_output = current_section;
                }
            } else if (tokens.size() >= 2) {
                bool is_path = (tokens[0].rfind('/', 0) == 0 || tokens[0].rfind('~', 0) == 0 ||
                                miqu::FsUtils::has_image_extension(tokens[0]));

                if (is_path) {
                    img_path = expand_path(tokens[0]);
                    mode = string_to_mode(tokens[1]);
                    if (tokens.size() >= 3) {
                        col = hex_to_color(tokens[2], col);
                    }
                    if (current_section != "general") {
                        target_output = current_section;
                    }
                } else {
                    target_output = tokens[0];
                    img_path = expand_path(tokens[1]);
                    if (tokens.size() >= 3) {
                        mode = string_to_mode(tokens[2]);
                    }
                    if (tokens.size() >= 4) {
                        col = hex_to_color(tokens[3], col);
                    }
                }
            }

            std::lock_guard<std::mutex> lock(m_mutex);
            OutputConfig cfg;
            cfg.image_path = img_path;
            cfg.mode = mode;
            cfg.bg_color = col;

            if (target_output.empty() || target_output == "*") {
                m_default_config = cfg;
            } else {
                m_output_configs[target_output] = cfg;
            }
            continue;
        }

        // Section property setters: mode, color, bg_color
        std::lock_guard<std::mutex> lock(m_mutex);
        if (key_lower == "mode" || key_lower == "fit_mode") {
            miqu::FitMode m = string_to_mode(val);
            if (current_section == "general") {
                m_default_config.mode = m;
            } else {
                m_output_configs[current_section].mode = m;
            }
        } else if (key_lower == "color" || key_lower == "bg_color") {
            miqu::Color c = hex_to_color(val, miqu::Color::rgb(0.0f, 0.0f, 0.0f));
            if (current_section == "general") {
                m_default_config.bg_color = c;
            } else {
                m_output_configs[current_section].bg_color = c;
            }
        } else if (key_lower == "clear") {
            miqu::Color c = hex_to_color(val, miqu::Color::rgb(0.0f, 0.0f, 0.0f));
            if (current_section == "general") {
                m_default_config.image_path = "";
                m_default_config.bg_color = c;
            } else {
                m_output_configs[current_section].image_path = "";
                m_output_configs[current_section].bg_color = c;
            }
        }
    }
}

bool Config::save(const std::string& custom_path) {
    std::string target_file = custom_path;

    if (target_file.empty()) {
        std::string user_conf = get_user_config_path();
        target_file = user_conf;
    }

    if (target_file.empty()) return false;

    // Never write into system prefix /usr/share/miqubg
    if (target_file.rfind("/usr/share", 0) == 0 || target_file.rfind("/usr/local/share", 0) == 0) {
        target_file = get_user_config_path();
    }

    std::error_code ec;
    fs::create_directories(fs::path(target_file).parent_path(), ec);

    // Mark internal write to suppress self inotify reload feedback
    m_internal_write_time = std::chrono::steady_clock::now();
    m_internal_write_flag = true;

    std::lock_guard<std::mutex> lock(m_mutex);

    std::string default_line = "wallpaper = , " + m_default_config.image_path + ", "
                               + mode_to_string(m_default_config.mode) + ", "
                               + color_to_hex(m_default_config.bg_color);

    std::vector<std::string> lines;
    bool had_existing_file = fs::exists(target_file);

    if (had_existing_file) {
        std::ifstream in(target_file);
        if (in.is_open()) {
            std::string line;
            bool replaced_default = false;
            std::map<std::string, bool> replaced_outputs;

            while (std::getline(in, line)) {
                std::string t = trim(line);
                std::string t_lower = t;
                std::transform(t_lower.begin(), t_lower.end(), t_lower.begin(), ::tolower);

                if (t_lower.rfind("wallpaper", 0) == 0 || t_lower.rfind("monitor", 0) == 0) {
                    size_t eq = t.find('=');
                    if (eq != std::string::npos) {
                        std::string val = trim(t.substr(eq + 1));
                        // Check if default wallpaper line
                        if (val.rfind(',', 0) == 0 || val.rfind('*', 0) == 0 ||
                            val.rfind('/', 0) == 0 || val.rfind('~', 0) == 0) {
                            if (!replaced_default) {
                                lines.push_back(default_line);
                                replaced_default = true;
                            }
                            continue;
                        }

                        // Per-monitor line: check if monitor matches one in active m_output_configs
                        size_t comma = val.find(',');
                        std::string mon = trim(val.substr(0, comma));
                        auto it = m_output_configs.find(mon);
                        if (it != m_output_configs.end()) {
                            std::string mon_line = "wallpaper = " + mon + ", " + it->second.image_path + ", "
                                                   + mode_to_string(it->second.mode) + ", "
                                                   + color_to_hex(it->second.bg_color);
                            lines.push_back(mon_line);
                            replaced_outputs[mon] = true;
                            continue;
                        } else {
                            // Output not in active overrides, omit stale line
                            continue;
                        }
                    }
                }
                lines.push_back(line);
            }

            // If default was not found in existing lines, insert after [general]
            if (!replaced_default) {
                auto gen_it = std::find_if(lines.begin(), lines.end(), [](const std::string& l) {
                    return trim(l) == "[general]";
                });
                if (gen_it != lines.end()) {
                    lines.insert(gen_it + 1, default_line);
                } else {
                    lines.push_back("\n[general]\n" + default_line);
                }
            }

            // Append any new per-monitor overrides that were not already in the file
            for (const auto& [mon, cfg] : m_output_configs) {
                if (!replaced_outputs[mon]) {
                    lines.push_back("wallpaper = " + mon + ", " + cfg.image_path + ", "
                                   + mode_to_string(cfg.mode) + ", "
                                   + color_to_hex(cfg.bg_color));
                }
            }
        }
    }

    if (!had_existing_file || lines.empty()) {
        lines.clear();
        lines.push_back("# ==============================================================================");
        lines.push_back("# miqubg Configuration File");
        lines.push_back("# Independent Wayland Wallpaper Daemon for Miquland");
        lines.push_back("# ==============================================================================");
        lines.push_back("");
        lines.push_back("[general]");
        lines.push_back("# Default wallpaper applied across all monitors");
        lines.push_back("# Syntax: wallpaper = [monitor], <path>, [fit_mode], [bg_color]");
        lines.push_back(default_line);
        lines.push_back("");
        lines.push_back("# ==============================================================================");
        lines.push_back("# Per-Monitor Overrides");
        lines.push_back("# ==============================================================================");
        if (m_output_configs.empty()) {
            lines.push_back("# wallpaper = HDMI-A-1, /usr/share/miqubg/wallpaper.png, cover, #000000");
            lines.push_back("# wallpaper = eDP-1, /usr/share/miqubg/wallpaper.png, fit, #1e1e2e");
        } else {
            for (const auto& [mon, cfg] : m_output_configs) {
                lines.push_back("wallpaper = " + mon + ", " + cfg.image_path + ", "
                               + mode_to_string(cfg.mode) + ", "
                               + color_to_hex(cfg.bg_color));
            }
        }
    }

    std::ofstream out(target_file);
    if (!out.is_open()) return false;
    for (const auto& l : lines) {
        out << l << "\n";
    }
    out.flush();

    m_config_path = target_file;
    return true;
}

} // namespace miqubg
