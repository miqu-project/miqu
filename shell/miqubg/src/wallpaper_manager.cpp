#include "wallpaper_manager.hpp"
#include <iostream>
#include <set>
#include <vector>
#include <algorithm>
#include <thread>
#include <chrono>
#include <filesystem>
#include <climits>
#include <unistd.h>

namespace miqubg {

std::string WallpaperManager::resolve_default_wallpaper() {
    namespace fs = std::filesystem;
    std::error_code ec;

    // 1. Resolve directory of running executable
    std::string exe_dir;
    char exe_buf[PATH_MAX];
    ssize_t len = ::readlink("/proc/self/exe", exe_buf, sizeof(exe_buf) - 1);
    if (len > 0) {
        exe_buf[len] = '\0';
        exe_dir = fs::path(exe_buf).parent_path().string();
    }

    std::vector<std::string> candidates;

    // Installed system asset paths
#ifdef MIQUBG_ASSET_DIR
    candidates.push_back(std::string(MIQUBG_ASSET_DIR) + "/wallpaper.png");
#endif
    candidates.push_back("/usr/share/miqubg/wallpaper.png");
    candidates.push_back("/usr/local/share/miqubg/wallpaper.png");

    // Relative to executable location
    if (!exe_dir.empty()) {
        candidates.push_back(exe_dir + "/assets/wallpaper.png");
        candidates.push_back(exe_dir + "/../assets/wallpaper.png");
        candidates.push_back(exe_dir + "/../share/miqubg/wallpaper.png");
        candidates.push_back(exe_dir + "/../../shell/miqubg/assets/wallpaper.png");
    }

    // Relative to working directory
    candidates.push_back("assets/wallpaper.png");
    candidates.push_back("../assets/wallpaper.png");
    candidates.push_back("shell/miqubg/assets/wallpaper.png");

    // Distribution / desktop fallback paths
    candidates.push_back("/usr/share/backgrounds/biway/wallpaper.png");
    candidates.push_back("/usr/share/backgrounds/biway/lightwallpaper.png");
    candidates.push_back("/usr/share/backgrounds/biway/darkwallpaper.jpg");
    candidates.push_back("/usr/share/backgrounds/wallpaper.png");

    for (const auto& path : candidates) {
        if (!path.empty() && fs::exists(path, ec) && fs::is_regular_file(path, ec)) {
            return path;
        }
    }

    return "";
}

miqu::FitMode WallpaperManager::parse_mode(const std::string& mode_str) {
    std::string s = mode_str;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (s == "fit" || s == "contain") return miqu::FitMode::Contain;
    if (s == "stretch" || s == "fill") return miqu::FitMode::Fill;
    if (s == "center") return miqu::FitMode::Center;
    if (s == "tile") return miqu::FitMode::Tile;
    return miqu::FitMode::Cover;
}


void WallpaperManager::init(miqu::AppEngine* engine) {
    m_engine = engine;

    miqu::OutputManager::get()->on_outputs_changed([this]() {
        sync_outputs();
    });

    sync_outputs();
}

OutputConfig WallpaperManager::get_config_for_output(const std::string& name) const {
    OutputConfig cfg = m_default_config;
    auto it = m_output_configs.find(name);
    if (it != m_output_configs.end()) {
        cfg = it->second;
    }
    if (cfg.image_path.empty()) {
        cfg.image_path = resolve_default_wallpaper();
    }
    return cfg;
}

OutputConfig WallpaperManager::get_current_config(const std::string& output_name) const {
    if (output_name.empty() || output_name == "*") {
        OutputConfig cfg = m_default_config;
        if (cfg.image_path.empty()) {
            cfg.image_path = resolve_default_wallpaper();
        }
        return cfg;
    }
    return get_config_for_output(output_name);
}

static std::shared_ptr<miqu::Window> create_wallpaper_window(const miqu::OutputInfo& out, const OutputConfig& cfg) {
    std::string img_path = cfg.image_path;
    if (img_path.empty()) {
        img_path = WallpaperManager::resolve_default_wallpaper();
    }

    auto imageView = miqu::ImageViewBuilder::create()
        ->source(img_path)
        ->fitMode(cfg.mode)
        ->backgroundColor(cfg.bg_color)
        ->build();

    return miqu::WindowBuilder::create()
        ->role(miqu::WindowRole::LayerBackground)
        ->output(out.wl_output)
        ->layerNamespace("wallpaper")
        ->keyboardInteractive(false)
        ->exclusiveZone(-1)
        ->contentView(imageView)
        ->build();
}

void WallpaperManager::update_wallpaper(const std::string& output_name, OutputConfig config) {
    if (output_name.empty() || output_name == "*") {
        m_default_config = config;
        m_output_configs.clear();
    } else {
        m_output_configs[output_name] = config;
    }

    auto outputs = miqu::OutputManager::get()->get_outputs();
    for (const auto& out : outputs) {
        if (!output_name.empty() && output_name != "*" && out.name != output_name) {
            continue;
        }

        OutputConfig cfg = get_config_for_output(out.name);
        std::cout << "[miqubg] Swapping wallpaper for output '"
                  << (out.name.empty() ? "(unknown)" : out.name) << "' [id=" << out.id << "]"
                  << " image: " << cfg.image_path << std::endl;

        auto new_window = create_wallpaper_window(out, cfg);
        if (!new_window) {
            std::cerr << "[miqubg] Failed to create new wallpaper surface for output " << out.id << std::endl;
            continue;
        }

        auto it_old = m_windows.find(out.id);
        std::shared_ptr<miqu::Window> old_window = (it_old != m_windows.end()) ? it_old->second : nullptr;

        m_windows[out.id] = new_window;

        if (old_window && m_engine) {
            // miquland's compositor engine smoothly animates the new surface into place.
            // After 380ms, cleanly tear down the old background window on the main thread.
            std::thread([engine = m_engine, old_window]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(380));
                engine->post([old_window]() {
                    if (old_window) {
                        old_window->close();
                    }
                });
            }).detach();
        }
    }
}

void WallpaperManager::sync_outputs() {
    if (!m_engine) return;

    auto outputs = miqu::OutputManager::get()->get_outputs();
    std::set<uint32_t> current_output_ids;

    for (const auto& out : outputs) {
        current_output_ids.insert(out.id);

        if (m_windows.find(out.id) == m_windows.end()) {
            OutputConfig cfg = get_config_for_output(out.name);

            std::cout << "[miqubg] Creating wallpaper surface for output '"
                      << (out.name.empty() ? "(unknown)" : out.name) << "' [id=" << out.id << "]"
                      << (cfg.image_path.empty() ? " (solid color)" : (" image: " + cfg.image_path))
                      << std::endl;

            auto window = create_wallpaper_window(out, cfg);
            if (window) {
                m_windows[out.id] = window;
            } else {
                std::cerr << "[miqubg] Failed to create layer window for output " << out.id << std::endl;
            }
        }
    }

    // Clean up surfaces for removed outputs
    for (auto it = m_windows.begin(); it != m_windows.end();) {
        if (current_output_ids.find(it->first) == current_output_ids.end()) {
            std::cout << "[miqubg] Output disconnected [id=" << it->first << "], removing wallpaper surface." << std::endl;
            if (it->second) {
                it->second->close();
            }
            it = m_windows.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace miqubg
