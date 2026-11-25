#include "gallery_app.hpp"
#include <xkbcommon/xkbcommon-keysyms.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace miqu {

constexpr int MATCH_PARENT = static_cast<int>(LayoutDimension::MatchParent);
constexpr int WRAP_CONTENT = static_cast<int>(LayoutDimension::WrapContent);

static std::string format_bytes(uintmax_t bytes) {
    static const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double val = static_cast<double>(bytes);
    int idx = 0;
    while (val >= 1024.0 && idx < 4) {
        val /= 1024.0;
        idx++;
    }
    std::ostringstream oss;
    if (idx == 0) oss << bytes << " B";
    else oss << std::fixed << std::setprecision(1) << val << " " << units[idx];
    return oss.str();
}

static std::string format_mtime(const fs::path& p) {
    std::error_code ec;
    auto ftime = fs::last_write_time(p, ec);
    if (ec) return "";
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
    std::tm tm_val{};
    localtime_r(&tt, &tm_val);
    char buf[64];
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm_val) == 0) return "";
    return buf;
}

GalleryApp::GalleryApp(std::shared_ptr<AppEngine> engine, const std::string& image_path)
    : m_engine(std::move(engine)) {
    if (!image_path.empty()) {
        std::error_code ec;
        fs::path p(image_path);
        fs::path can = fs::weakly_canonical(p, ec);
        m_image_path = ec ? p : can;
    }
}

bool GalleryApp::init() {
    if (!m_engine) return false;

    setup_ui();
    load_image();

    std::string win_title = "Gallery";
    if (!m_image_path.empty() && !m_image_path.filename().empty()) {
        win_title = m_image_path.filename().string();
    }

    m_window = WindowBuilder::create()
        ->title(win_title)
        ->appId("miqugallery")
        ->role(WindowRole::Toplevel)
        ->preferredSize(960, 680)
        ->closeOnEscape(false)
        ->keyboardInteractive(true)
        ->contentView(m_root_layout)
        ->onClose([this]() {
            if (m_engine) m_engine->quit();
        })
        ->onKey([this](const KeyPressEvent& ev) -> bool {
            if (!ev.pressed) return false;
            if (ev.keysym == XKB_KEY_i || ev.keysym == XKB_KEY_I) {
                toggle_hud();
                return true;
            }
            return false;
        })
        ->build();

    return m_window != nullptr;
}

void GalleryApp::setup_ui() {
    auto config = Config::get();
    if (!config) return;

    m_root_layout = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->backgroundColor(Color::transparent())
        ->build();

    std::string title = "Gallery";
    if (!m_image_path.empty() && !m_image_path.filename().empty()) {
        title = m_image_path.filename().string();
    }

    m_toolbar = ToolbarBuilder::create()
        ->title(title)
        ->iconResource("image-x-generic")
        ->titleAlignment(TitleAlignment::Center)
        ->showDivider(false)
        ->onClose([this]() {
            if (m_engine) m_engine->quit(0);
        })
        ->closeTooltip("Close Gallery")
        ->build();

    m_root_layout->add_view(m_toolbar, LayoutParams(MATCH_PARENT, WRAP_CONTENT));

    m_canvas_frame = FrameLayoutBuilder::create()
        ->padding(12, 12)
        ->backgroundColor(Color::transparent())
        ->build();

    m_root_layout->add_view(m_canvas_frame, LayoutParams(MATCH_PARENT, MATCH_PARENT, 1.0f));
}

void GalleryApp::load_image() {
    auto config = Config::get();
    if (!config || !m_canvas_frame) return;

    m_canvas_frame->clear_views();
    m_hud_card.reset();

    int card_rad = config->metrics.corner_radius;

    std::error_code ec;
    if (m_image_path.empty() || !fs::exists(m_image_path, ec) || fs::is_directory(m_image_path, ec)) {
        auto empty_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->gravity(Gravity::Center)
            ->spacing(8)
            ->padding(24, 24)
            ->build();

        auto icon = ImageViewBuilder::create()
            ->source("image-x-generic")
            ->targetSize(56)
            ->fitMode(FitMode::Contain)
            ->alpha(0.5f)
            ->build();
        empty_box->add_view(icon, LayoutParams(56, 56, Gravity::CenterHorizontal));

        auto msg = TextViewBuilder::create()
            ->text(m_image_path.empty() ? "No Image Provided" : "Image Not Found")
            ->bold(true)
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->center(true)
            ->build();
        empty_box->add_view(msg);

        auto hint = TextViewBuilder::create()
            ->text("Open an image file with miqugallery to view it at full quality")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant)
            ->center(true)
            ->build();
        empty_box->add_view(hint);

        m_canvas_frame->add_view(empty_box, LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity::Center));
        return;
    }

    // 1. Full-quality ImageView filling the canvas
    auto img = ImageViewBuilder::create()
        ->imageResource(m_image_path.string())
        ->fitMode(FitMode::Contain)
        ->qualityMode(ImageQuality::FullOriginal)
        ->cornerRadius(card_rad)
        ->padding(4, 4)
        ->build();
    m_canvas_frame->add_view(img, LayoutParams(MATCH_PARENT, MATCH_PARENT));

    // 2. Metadata HUD pill (dimensions • size • modified)
    int img_w = 0, img_h = 0;
    std::string meta_line;
    if (gdk_pixbuf_get_file_info(m_image_path.c_str(), &img_w, &img_h) != nullptr && img_w > 0 && img_h > 0) {
        meta_line = std::to_string(img_w) + " × " + std::to_string(img_h);
    }

    uintmax_t sz = fs::file_size(m_image_path, ec);
    if (!ec) {
        if (!meta_line.empty()) meta_line += "  •  ";
        meta_line += format_bytes(sz);
    }

    std::string mtime = format_mtime(m_image_path);
    if (!mtime.empty()) {
        if (!meta_line.empty()) meta_line += "  •  ";
        meta_line += mtime;
    }

    if (!meta_line.empty()) {
        m_hud_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->padding(16, 8)
            ->gravity(Gravity::Center)
            ->backgroundColor(config->colors.surface.with_alpha(0.85f))
            ->stroke(1, config->colors.outline.with_alpha(0.35f))
            ->cornerRadius(card_rad)
            ->margin(0, 0, 0, 16)
            ->build();

        auto meta_view = TextViewBuilder::create()
            ->text(meta_line)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.9f))
            ->center(true)
            ->build();
        m_hud_card->add_view(meta_view, LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity::Center));
        m_hud_card->set_visibility(m_hud_visible ? Visibility::Visible : Visibility::Gone);

        m_canvas_frame->add_view(m_hud_card, LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity::CenterHorizontal | Gravity::Bottom));
    }
}

void GalleryApp::toggle_hud() {
    m_hud_visible = !m_hud_visible;
    if (m_hud_card) {
        m_hud_card->set_visibility(m_hud_visible ? Visibility::Visible : Visibility::Gone);
    }
}

void GalleryApp::sync_theme() {
    load_image();
    if (m_canvas_frame) m_canvas_frame->request_redraw();
}

} // namespace miqu
