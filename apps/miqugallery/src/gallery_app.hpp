#pragma once

#include <miqutoolkit/miqutoolkit.hpp>
#include <filesystem>
#include <memory>
#include <string>

namespace miqu {

class ZoomableImageView;

class GalleryApp {
public:
    explicit GalleryApp(std::shared_ptr<AppEngine> engine, const std::string& image_path = "");
    ~GalleryApp() = default;

    bool init();
    void sync_theme();

    std::shared_ptr<Window> get_window() const { return m_window; }

private:
    void setup_ui();
    void load_image();
    void toggle_hud();
    void update_hud_text();

    std::shared_ptr<AppEngine> m_engine;
    std::filesystem::path m_image_path;
    std::shared_ptr<Window> m_window;

    std::shared_ptr<LinearLayout> m_root_layout;
    std::shared_ptr<Toolbar> m_toolbar;
    std::shared_ptr<FrameLayout> m_canvas_frame;
    std::shared_ptr<ZoomableImageView> m_image_view;
    std::shared_ptr<LinearLayout> m_hud_card;
    std::shared_ptr<TextView> m_meta_view;
    std::string m_base_meta_line;
    bool m_hud_visible = true;
};

} // namespace miqu
