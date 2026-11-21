#include "plugin_backend.hpp"
#include <miquland/plugin.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <unordered_set>
#include <dlfcn.h>

namespace fs = std::filesystem;

namespace miqu {

static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

static std::string format_display_name(const std::string& name) {
    if (name.empty()) return "Unknown Plugin";
    if (name == "miquoverview" || name == "libmiquoverview.so" || name == "libmiquoverview") {
        return "Miqu Overview";
    }
    std::string s = name;
    if (s.rfind("lib", 0) == 0) s = s.substr(3);
    if (s.size() > 3 && s.substr(s.size() - 3) == ".so") s = s.substr(0, s.size() - 3);
    if (!s.empty()) s[0] = std::toupper(s[0]);
    return s;
}

std::vector<PluginItemInfo> PluginBackend::get_available_plugins() {
    std::vector<PluginItemInfo> plugins;
    std::unordered_set<std::string> seen_paths;

    std::vector<std::string> search_dirs = {
        "/usr/lib/miquland/plugins"
    };

    const char* home = std::getenv("HOME");
    if (home) {
        search_dirs.push_back(std::string(home) + "/.local/lib/miquland/plugins");
    }

    // 1. Passive / Read-only check for active plugins in configuration
    std::unordered_set<std::string> active_plugins;
    fs::path active_conf_path;
    if (home) {
        fs::path p_conf = fs::path(home) / ".config" / "miquland" / "plugins.conf";
        if (fs::exists(p_conf)) {
            active_conf_path = p_conf;
        } else {
            fs::path m_conf = fs::path(home) / ".config" / "miquland" / "miquland.conf";
            if (fs::exists(m_conf)) active_conf_path = m_conf;
        }
    }
    if (active_conf_path.empty() && fs::exists("/usr/share/miquland/plugins.conf")) {
        active_conf_path = "/usr/share/miquland/plugins.conf";
    }

    if (!active_conf_path.empty()) {
        std::ifstream f(active_conf_path);
        std::string line;
        while (std::getline(f, line)) {
            std::string trimmed = trim(line);
            if (trimmed.empty() || trimmed.front() == '#' || trimmed.rfind("//", 0) == 0) {
                continue;
            }
            if (trimmed.rfind("load", 0) == 0) {
                size_t eq = trimmed.find('=');
                if (eq != std::string::npos) {
                    std::string val = trim(trimmed.substr(eq + 1));
                    if (!val.empty() && val.front() == '"' && val.back() == '"') {
                        val = val.substr(1, val.size() - 2);
                    }
                    active_plugins.insert(val);
                    fs::path vp(val);
                    active_plugins.insert(vp.filename().string());
                }
            }
        }
    }

    // 2. Scan directories for shared library plugins (.so)
    for (const auto& dir : search_dirs) {
        std::error_code ec;
        if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) continue;

        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (ec) break;
            if (!entry.is_regular_file()) continue;

            std::string ext = entry.path().extension().string();
            if (ext != ".so") continue;

            std::string full_path = entry.path().string();
            if (seen_paths.count(full_path)) continue;
            seen_paths.insert(full_path);

            std::string filename = entry.path().filename().string();

            PluginItemInfo info;
            info.path = full_path;
            info.name = filename;
            info.display_name = format_display_name(filename);
            info.version = "1.0.0";
            info.author = "Miqu Desktop Team";
            info.description = "Miqu Desktop dynamic compositor plugin";
            info.abi_version = miquland::MIQU_PLUGIN_ABI_VERSION;

            // Safe introspection via miqu_plugin_get_info if exported
            void* handle = dlopen(full_path.c_str(), RTLD_LAZY | RTLD_LOCAL);
            if (handle) {
                auto get_info_fn = reinterpret_cast<miquland::PluginGetInfoFunc>(dlsym(handle, "miqu_plugin_get_info"));
                if (get_info_fn) {
                    auto* pinfo = get_info_fn();
                    if (pinfo) {
                        if (pinfo->name) info.name = pinfo->name;
                        if (pinfo->author) info.author = pinfo->author;
                        if (pinfo->description) info.description = pinfo->description;
                        if (pinfo->version) info.version = pinfo->version;
                        info.abi_version = pinfo->abi_version;
                        info.display_name = format_display_name(info.name);
                    }
                }
                dlclose(handle);
            }

            // Check if active
            info.is_active = (active_plugins.count(full_path) > 0 || active_plugins.count(filename) > 0);

            plugins.push_back(info);
        }
    }

    return plugins;
}

std::vector<std::shared_ptr<SettingItem>> PluginBackend::get_plugin_items() {
    std::vector<std::shared_ptr<SettingItem>> items;
    auto plugins = get_available_plugins();

    for (const auto& p : plugins) {
        std::string id = "plugin_" + p.name;
        std::string sub = p.is_active ? "Active" : "Inactive";
        auto item = SettingItem::create_leaf(id, p.display_name, sub, "preferences-plugin");
        item->is_plugin = true;
        item->plugin_info = p;
        items.push_back(item);
    }

    if (items.empty()) {
        auto empty_item = SettingItem::create_leaf("plugin_none", "No Plugins Installed", "Place .so plugins in /usr/lib/miquland/plugins", "dialog-information");
        empty_item->is_plugin = true;
        empty_item->plugin_info.name = "none";
        empty_item->plugin_info.display_name = "No Plugins Installed";
        empty_item->plugin_info.description = "No dynamic shared library plugins were found in /usr/lib/miquland/plugins or ~/.local/lib/miquland/plugins.";
        items.push_back(empty_item);
    }

    return items;
}

void PluginBackend::ensure_user_config_on_tinker() {
    const char* home = std::getenv("HOME");
    if (!home) return;

    fs::path miqu_dir = fs::path(home) / ".config" / "miquland";
    std::error_code ec;

    if (!fs::exists(miqu_dir, ec) || !fs::exists(miqu_dir / "plugins.conf", ec)) {
        std::system("miquland --init-config >/dev/null 2>&1");
    }

    if (!fs::exists(miqu_dir, ec)) {
        fs::create_directories(miqu_dir, ec);
    }

    // Ensure ~/.config/miquland/miquland.conf sources plugins.conf
    fs::path main_conf = miqu_dir / "miquland.conf";
    if (fs::exists(main_conf, ec)) {
        std::ifstream mf(main_conf);
        std::string line;
        bool has_source = false;
        while (std::getline(mf, line)) {
            std::string t = trim(line);
            if (t.find("plugins.conf") != std::string::npos && t.find("source") != std::string::npos && t.front() != '#') {
                has_source = true;
                break;
            }
        }
        mf.close();

        if (!has_source) {
            std::ofstream mappend(main_conf, std::ios::app);
            if (mappend.is_open()) {
                mappend << "\n# Sourced modular plugins configuration:\nsource = ~/.config/miquland/plugins.conf\n";
            }
        }
    }
}

bool PluginBackend::set_plugin_active(const std::string& plugin_path, bool active) {
    if (plugin_path.empty()) return false;

    // 1. Tinker check: Ensure user configuration files exist before modifying
    ensure_user_config_on_tinker();

    const char* home = std::getenv("HOME");
    if (!home) return false;

    fs::path conf_path = fs::path(home) / ".config" / "miquland" / "plugins.conf";
    fs::path plugin_p(plugin_path);
    std::string filename = plugin_p.filename().string();

    std::vector<std::string> lines;
    bool found = false;
    bool in_plugins_section = false;
    bool has_plugins_section = false;

    if (fs::exists(conf_path)) {
        std::ifstream in(conf_path);
        std::string line;
        while (std::getline(in, line)) {
            std::string t = trim(line);
            if (t.rfind("plugins", 0) == 0 && t.find('{') != std::string::npos) {
                in_plugins_section = true;
                has_plugins_section = true;
            } else if (in_plugins_section && t == "}") {
                in_plugins_section = false;
            }

            // Check if this line refers to the target plugin
            if ((line.find(plugin_path) != std::string::npos || line.find(filename) != std::string::npos) &&
                line.find("load") != std::string::npos) {
                found = true;
                if (active) {
                    // Activate: uncomment load = plugin_path
                    lines.push_back("    load = " + plugin_path);
                } else {
                    // Deactivate: comment out
                    lines.push_back("    # load = " + plugin_path);
                }
            } else {
                lines.push_back(line);
            }
        }
        in.close();
    }

    if (!found) {
        if (!has_plugins_section) {
            lines.clear();
            lines.push_back("# ==============================================================================");
            lines.push_back("# plugins.conf — miquland dynamic plugins");
            lines.push_back("# ==============================================================================");
            lines.push_back("plugins {");
            if (active) {
                lines.push_back("    load = " + plugin_path);
            } else {
                lines.push_back("    # load = " + plugin_path);
            }
            lines.push_back("}");
        } else {
            // Insert inside plugins { ... } section before closing }
            std::vector<std::string> updated;
            bool inserted = false;
            for (const auto& l : lines) {
                std::string t = trim(l);
                if (!inserted && t == "}") {
                    if (active) {
                        updated.push_back("    load = " + plugin_path);
                    } else {
                        updated.push_back("    # load = " + plugin_path);
                    }
                    inserted = true;
                }
                updated.push_back(l);
            }
            lines = std::move(updated);
        }
    }

    // Write out updated configuration
    std::ofstream out(conf_path);
    if (!out.is_open()) return false;
    for (const auto& l : lines) {
        out << l << "\n";
    }
    out.flush();

    return true;
}

} // namespace miqu
