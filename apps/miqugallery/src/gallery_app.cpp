#include "gallery_app.hpp"
#include <xkbcommon/xkbcommon-keysyms.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace miqu {

constexpr int MATCH_PARENT = static_cast<int>(LayoutDimension::MatchParent);
constexpr int WRAP_CONTENT = static_cast<int>(LayoutDimension::WrapContent);

class ZoomableImageView : public ImageView {
public:
    ZoomableImageView() = default;

    void set_on_zoom_changed(std::function<void(double)> cb) {
        m_on_zoom_changed = std::move(cb);
    }

    void set_intrinsic_size(int w, int h) {
        m_intrinsic_w = w;
        m_intrinsic_h = h;
    }

    bool on_mouse_move(int lx, int ly, const Rect& bounds) override {
        m_last_lx = lx;
        m_last_ly = ly;
        if (m_dragging) {
            double nx = m_drag_start_pan_x + (lx - m_drag_start_x);
            double ny = m_drag_start_pan_y + (ly - m_drag_start_y);
            apply_clamped_pan(nx, ny, bounds);
            return true;
        }
        return false;
    }

    bool on_mouse_button(int lx, int ly, MouseButton button, bool pressed, const Rect& bounds) override {
        if (!is_visible() || !is_enabled()) return false;
        m_last_lx = lx;
        m_last_ly = ly;

        if (button == MouseButton::Left) {
            if (pressed) {
                if (!bounds.contains(Point(lx, ly))) return false;
                auto now = std::chrono::steady_clock::now();
                auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_click_time).count();
                int dx = lx - m_last_click_x;
                int dy = ly - m_last_click_y;
                m_last_click_time = now;
                m_last_click_x = lx;
                m_last_click_y = ly;

                if (dt > 0 && dt < 320 && (dx * dx + dy * dy) <= 64) {
                    m_dragging = false;
                    if (get_zoom() > 1.05) {
                        zoom_to_fit();
                    } else {
                        zoom_at(2.5, lx, ly, bounds);
                    }
                    return true;
                }

                if (get_zoom() > 1.001) {
                    m_dragging = true;
                    m_drag_start_x = lx;
                    m_drag_start_y = ly;
                    m_drag_start_pan_x = get_pan_x();
                    m_drag_start_pan_y = get_pan_y();
                    return true;
                }
            } else if (m_dragging) {
                m_dragging = false;
                return true;
            }
        }
        return ImageView::on_mouse_button(lx, ly, button, pressed, bounds);
    }

    bool on_scroll_2d(double dx, double dy, uint32_t axis_source = 0) override {
        if (!is_visible() || !is_enabled()) return false;
        const Rect& bounds = get_bounds();

        bool ctrl_held = (get_window() && get_window()->has_ctrl());

        // WL_POINTER_AXIS_SOURCE_FINGER == 1: two-finger touchpad pan (unless Ctrl is held to zoom)
        if (axis_source == 1 && !ctrl_held) {
            if (get_zoom() <= 1.001) {
                // At fit level: two-finger scroll also zooms (centered on view)
                if (std::abs(dy) < 1e-4) return false;
                double factor = std::pow(1.12, -dy / 10.0);
                double anchor_x = bounds.x + bounds.width / 2.0;
                double anchor_y = bounds.y + bounds.height / 2.0;
                zoom_at(get_zoom() * factor, anchor_x, anchor_y, bounds);
                return true;
            }
            apply_clamped_pan(get_pan_x() - dx, get_pan_y() - dy, bounds);
            return true;
        }

        // Mouse wheel / Ctrl+touchpad: zoom centered on cursor
        if (std::abs(dy) < 1e-4) return false;
        double factor = std::pow(1.12, -dy / 10.0);
        double anchor_x = (m_last_lx >= 0) ? m_last_lx : (bounds.x + bounds.width / 2.0);
        double anchor_y = (m_last_ly >= 0) ? m_last_ly : (bounds.y + bounds.height / 2.0);
        zoom_at(get_zoom() * factor, anchor_x, anchor_y, bounds);
        return true;
    }

    bool on_pinch_begin(int lx, int ly, uint32_t /*fingers*/) override {
        if (!is_visible() || !is_enabled()) return false;
        m_pinch_active = true;
        m_pinch_start_zoom = get_zoom();
        m_pinch_start_pan_x = get_pan_x();
        m_pinch_start_pan_y = get_pan_y();
        m_pinch_anchor_x = lx;
        m_pinch_anchor_y = ly;
        m_pinch_accum_dx = 0.0;
        m_pinch_accum_dy = 0.0;
        return true;
    }

    bool on_pinch_update(int /*lx*/, int /*ly*/, double scale, double dx, double dy) override {
        if (!m_pinch_active) return false;
        m_pinch_accum_dx += dx;
        m_pinch_accum_dy += dy;

        const Rect& bounds = get_bounds();
        double target_zoom = std::clamp(m_pinch_start_zoom * scale, 1.0, max_zoom(bounds));
        double ratio = (m_pinch_start_zoom > 1e-6) ? (target_zoom / m_pinch_start_zoom) : 1.0;

        double cx = 0.0, cy = 0.0;
        content_center(bounds, cx, cy);
        double ox = m_pinch_anchor_x - cx;
        double oy = m_pinch_anchor_y - cy;

        double new_pan_x = ox - (ox - m_pinch_start_pan_x) * ratio + m_pinch_accum_dx;
        double new_pan_y = oy - (oy - m_pinch_start_pan_y) * ratio + m_pinch_accum_dy;

        set_zoom_and_pan(target_zoom, new_pan_x, new_pan_y, bounds);
        return true;
    }

    bool on_pinch_end(bool cancelled) override {
        if (!m_pinch_active) return false;
        m_pinch_active = false;
        if (cancelled) {
            set_zoom_and_pan(m_pinch_start_zoom, m_pinch_start_pan_x, m_pinch_start_pan_y, get_bounds());
        }
        return true;
    }

    void zoom_by(double factor) {
        const Rect& bounds = get_bounds();
        double cx = 0.0, cy = 0.0;
        content_center(bounds, cx, cy);
        zoom_at(get_zoom() * factor, cx, cy, bounds);
    }

    void zoom_to_fit() {
        set_zoom_and_pan(1.0, 0.0, 0.0, get_bounds());
    }

    void zoom_to_native() {
        const Rect& bounds = get_bounds();
        double base = base_scale(bounds);
        if (base > 1e-6) {
            double cx = 0.0, cy = 0.0;
            content_center(bounds, cx, cy);
            zoom_at(1.0 / base, cx, cy, bounds);
        }
    }

    void pan_by(double dx, double dy) {
        if (get_zoom() <= 1.001) return;
        apply_clamped_pan(get_pan_x() + dx, get_pan_y() + dy, get_bounds());
    }

private:
    void content_rect(const Rect& bounds, double& x, double& y, double& w, double& h) const {
        const Padding& p = get_padding();
        x = bounds.x + p.left;
        y = bounds.y + p.top;
        w = std::max(1, bounds.width - p.left - p.right);
        h = std::max(1, bounds.height - p.top - p.bottom);
    }

    void content_center(const Rect& bounds, double& cx, double& cy) const {
        double x = 0.0, y = 0.0, w = 1.0, h = 1.0;
        content_rect(bounds, x, y, w, h);
        cx = x + w / 2.0;
        cy = y + h / 2.0;
    }

    void image_dims(int& sw, int& sh) const {
        sw = m_intrinsic_w > 0 ? m_intrinsic_w : get_source_width();
        sh = m_intrinsic_h > 0 ? m_intrinsic_h : get_source_height();
    }

    double base_scale(const Rect& bounds) const {
        int sw = 0, sh = 0;
        image_dims(sw, sh);
        if (sw <= 0 || sh <= 0) return 1.0;
        double x = 0.0, y = 0.0, w = 1.0, h = 1.0;
        content_rect(bounds, x, y, w, h);
        return std::min(w / sw, h / sh);
    }

    double max_zoom(const Rect& bounds) const {
        double base = base_scale(bounds);
        return std::max(16.0, 8.0 / std::max(base, 1e-4));
    }

    void clamp_pan(double zoom, double& px, double& py, const Rect& bounds) const {
        if (zoom <= 1.001) {
            px = 0.0;
            py = 0.0;
            return;
        }
        int sw = 0, sh = 0;
        image_dims(sw, sh);
        if (sw <= 0 || sh <= 0) return;

        double x = 0.0, y = 0.0, w = 1.0, h = 1.0;
        content_rect(bounds, x, y, w, h);
        double base = std::min(w / sw, h / sh);
        double scaled_w = sw * base * zoom;
        double scaled_h = sh * base * zoom;
        double max_x = std::max(0.0, (scaled_w - w) / 2.0);
        double max_y = std::max(0.0, (scaled_h - h) / 2.0);
        px = std::clamp(px, -max_x, max_x);
        py = std::clamp(py, -max_y, max_y);
    }

    void set_zoom_and_pan(double zoom, double px, double py, const Rect& bounds) {
        double clamped_zoom = std::clamp(zoom, 1.0, max_zoom(bounds));
        clamp_pan(clamped_zoom, px, py, bounds);
        bool changed = std::abs(get_zoom() - clamped_zoom) > 1e-4;
        set_zoom(clamped_zoom);
        set_pan(px, py);
        if (changed && m_on_zoom_changed) {
            m_on_zoom_changed(clamped_zoom);
        }
    }

    void apply_clamped_pan(double px, double py, const Rect& bounds) {
        double z = get_zoom();
        clamp_pan(z, px, py, bounds);
        set_pan(px, py);
    }

    void zoom_at(double new_zoom, double anchor_x, double anchor_y, const Rect& bounds) {
        double old_zoom = get_zoom();
        double clamped_zoom = std::clamp(new_zoom, 1.0, max_zoom(bounds));
        double ratio = (old_zoom > 1e-6) ? (clamped_zoom / old_zoom) : 1.0;

        double cx = 0.0, cy = 0.0;
        content_center(bounds, cx, cy);
        double ox = anchor_x - cx;
        double oy = anchor_y - cy;

        double new_pan_x = ox - (ox - get_pan_x()) * ratio;
        double new_pan_y = oy - (oy - get_pan_y()) * ratio;
        set_zoom_and_pan(clamped_zoom, new_pan_x, new_pan_y, bounds);
    }

    int m_intrinsic_w = 0;
    int m_intrinsic_h = 0;
    int m_last_lx = -1;
    int m_last_ly = -1;

    bool m_dragging = false;
    int m_drag_start_x = 0;
    int m_drag_start_y = 0;
    double m_drag_start_pan_x = 0.0;
    double m_drag_start_pan_y = 0.0;

    std::chrono::steady_clock::time_point m_last_click_time{};
    int m_last_click_x = 0;
    int m_last_click_y = 0;

    bool m_pinch_active = false;
    double m_pinch_start_zoom = 1.0;
    double m_pinch_start_pan_x = 0.0;
    double m_pinch_start_pan_y = 0.0;
    double m_pinch_anchor_x = 0.0;
    double m_pinch_anchor_y = 0.0;
    double m_pinch_accum_dx = 0.0;
    double m_pinch_accum_dy = 0.0;

    std::function<void(double)> m_on_zoom_changed;
};

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
            if (!m_image_view) return false;
            switch (ev.keysym) {
                case XKB_KEY_plus:
                case XKB_KEY_equal:
                case XKB_KEY_KP_Add:
                    m_image_view->zoom_by(1.25);
                    return true;
                case XKB_KEY_minus:
                case XKB_KEY_KP_Subtract:
                    m_image_view->zoom_by(1.0 / 1.25);
                    return true;
                case XKB_KEY_0:
                case XKB_KEY_KP_0:
                    m_image_view->zoom_to_fit();
                    return true;
                case XKB_KEY_1:
                case XKB_KEY_KP_1:
                    m_image_view->zoom_to_native();
                    return true;
                case XKB_KEY_Left:
                    m_image_view->pan_by(48.0, 0.0);
                    return true;
                case XKB_KEY_Right:
                    m_image_view->pan_by(-48.0, 0.0);
                    return true;
                case XKB_KEY_Up:
                    m_image_view->pan_by(0.0, 48.0);
                    return true;
                case XKB_KEY_Down:
                    m_image_view->pan_by(0.0, -48.0);
                    return true;
                default:
                    break;
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
    m_image_view.reset();
    m_hud_card.reset();
    m_meta_view.reset();
    m_base_meta_line.clear();

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

    // 1. Full-quality ZoomableImageView filling the canvas
    m_image_view = std::make_shared<ZoomableImageView>();
    m_image_view->set_image_resource(m_image_path.string());
    m_image_view->set_fit_mode(FitMode::Contain);
    m_image_view->set_quality_mode(ImageQuality::FullOriginal);
    m_image_view->set_corner_radius(card_rad);
    m_image_view->set_padding(4, 4);
    m_image_view->set_on_zoom_changed([this](double) {
        update_hud_text();
    });

    // 2. Metadata HUD pill (dimensions • size • modified • zoom)
    int img_w = 0, img_h = 0;
    std::string meta_line;
    if (gdk_pixbuf_get_file_info(m_image_path.c_str(), &img_w, &img_h) != nullptr && img_w > 0 && img_h > 0) {
        m_image_view->set_intrinsic_size(img_w, img_h);
        meta_line = std::to_string(img_w) + " × " + std::to_string(img_h);
    }

    m_canvas_frame->add_view(m_image_view, LayoutParams(MATCH_PARENT, MATCH_PARENT));

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

    m_base_meta_line = meta_line;

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

        m_meta_view = TextViewBuilder::create()
            ->text(meta_line)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.9f))
            ->center(true)
            ->build();
        m_hud_card->add_view(m_meta_view, LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity::Center));
        m_hud_card->set_visibility(m_hud_visible ? Visibility::Visible : Visibility::Gone);

        m_canvas_frame->add_view(m_hud_card, LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity::CenterHorizontal | Gravity::Bottom));
    }
}

void GalleryApp::update_hud_text() {
    if (!m_meta_view) return;
    std::string text = m_base_meta_line;
    if (m_image_view && m_image_view->get_zoom() > 1.005) {
        int pct = static_cast<int>(std::round(m_image_view->get_zoom() * 100.0));
        if (!text.empty()) text += "  •  ";
        text += std::to_string(pct) + "%";
    }
    m_meta_view->set_text(text);
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
