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
    std::lock_guard<std::mutex> lock(m_config_mutex);
    OutputConfig cfg = m_default_config;
    auto it = m_output_configs.find(name);
    if (it != m_output_configs.end()) {
        cfg = it->second;
    }
    return cfg;
}

OutputConfig WallpaperManager::get_current_config(const std::string& output_name) const {
    if (output_name.empty() || output_name == "*") {
        std::lock_guard<std::mutex> lock(m_config_mutex);
        return m_default_config;
    }
    return get_config_for_output(output_name);
}

std::map<std::string, OutputConfig> WallpaperManager::get_all_active_configs() const {
    std::map<std::string, OutputConfig> result;
    auto outputs = miqu::OutputManager::get()->get_outputs();
    for (const auto& out : outputs) {
        std::string name = out.name.empty() ? ("output-" + std::to_string(out.id)) : out.name;
        result[name] = get_config_for_output(out.name);
    }
    if (result.empty()) {
        result["*"] = get_current_config("*");
    }
    return result;
}

static std::shared_ptr<miqu::Window> create_wallpaper_window(const miqu::OutputInfo& out, const OutputConfig& cfg) {
    auto imgBuilder = miqu::ImageViewBuilder::create()
        ->fitMode(cfg.mode)
        ->backgroundColor(cfg.bg_color);

    if (!cfg.image_path.empty()) {
        imgBuilder->source(cfg.image_path);
    }
    auto imageView = imgBuilder->build();

    return miqu::WindowBuilder::create()
        ->role(miqu::WindowRole::LayerBackground)
        ->output(out.wl_output)
        ->layerNamespace("wallpaper")
        ->keyboardInteractive(false)
        ->exclusiveZone(-1)
        ->contentView(imageView)
        ->build();
}

WallpaperManager::~WallpaperManager() {
    for (auto& pair : m_windows) {
        if (pair.second) {
            pair.second->close();
        }
    }
    m_windows.clear();
}

void WallpaperManager::update_wallpaper(const std::string& output_name, OutputConfig config) {
    {
        std::lock_guard<std::mutex> lock(m_config_mutex);
        if (output_name.empty() || output_name == "*") {
            m_default_config = config;
            m_output_configs.clear();
        } else {
            m_output_configs[output_name] = config;
        }
    }

    auto outputs = miqu::OutputManager::get()->get_outputs();
    for (const auto& out : outputs) {
        if (!output_name.empty() && output_name != "*" && out.name != output_name) {
            continue;
        }

        OutputConfig cfg = get_config_for_output(out.name);
        std::cout << "[miqubg] Updating wallpaper for output '"
                  << (out.name.empty() ? "(unknown)" : out.name) << "' [id=" << out.id << "]"
                  << (cfg.image_path.empty() ? " (solid color)" : (" image: " + cfg.image_path))
                  << std::endl;

        auto it = m_windows.find(out.id);
        if (it != m_windows.end() && it->second) {
            // Direct in-place update on the existing persistent surface
            auto content_view = it->second->get_content_view();
            auto img_view = std::dynamic_pointer_cast<miqu::ImageView>(content_view);
            if (img_view) {
                img_view->set_fit_mode(cfg.mode);
                img_view->set_background_color(cfg.bg_color);
                img_view->set_image_resource(cfg.image_path);
                img_view->invalidate_surface_cache();
                it->second->schedule_redraw();
                continue;
            }
        }

        // Output does not have a surface yet; create it once
        auto new_window = create_wallpaper_window(out, cfg);
        if (new_window) {
            m_windows[out.id] = new_window;
        } else {
            std::cerr << "[miqubg] Failed to create layer window for output " << out.id << std::endl;
        }
    }
}

void WallpaperManager::sync_outputs() {
    if (!m_engine) return;

    auto outputs = miqu::OutputManager::get()->get_outputs();
    std::set<uint32_t> current_output_ids;

    for (const auto& out : outputs) {
        current_output_ids.insert(out.id);

        auto it = m_windows.find(out.id);
        if (it == m_windows.end() || !it->second) {
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
        } else {
            // Refresh existing output in place
            OutputConfig cfg = get_config_for_output(out.name);
            auto content_view = it->second->get_content_view();
            auto img_view = std::dynamic_pointer_cast<miqu::ImageView>(content_view);
            if (img_view) {
                img_view->set_fit_mode(cfg.mode);
                img_view->set_background_color(cfg.bg_color);
                img_view->set_image_resource(cfg.image_path);
                img_view->invalidate_surface_cache();
                it->second->schedule_redraw();
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
