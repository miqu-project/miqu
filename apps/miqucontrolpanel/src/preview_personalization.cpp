#include "miller_view.hpp"
#include "preview_common.hpp"
#include <miqutoolkit/view/popup_window.hpp>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <thread>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

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

static bool send_miqubg_wallpaper_ipc(const std::string& image_path, const std::string& fit_mode) {
    const char* wayland_display = std::getenv("WAYLAND_DISPLAY");
    std::string display_name = (wayland_display && *wayland_display) ? wayland_display : "wayland-0";

    const char* runtime_dir = std::getenv("XDG_RUNTIME_DIR");
    std::string sock_path = (runtime_dir && *runtime_dir)
        ? std::string(runtime_dir) + "/miqubg-" + display_name + ".sock"
        : "/tmp/miqubg-" + std::to_string(getuid()) + "-" + display_name + ".sock";

    std::string mode_str = "cover";
    std::string lower = fit_mode;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower == "fit" || lower == "contain") mode_str = "contain";
    else if (lower == "stretch" || lower == "fill") mode_str = "fill";
    else if (lower == "center") mode_str = "center";
    else if (lower == "tile") mode_str = "tile";
    else mode_str = "cover";

    std::string cmd = "SET * " + mode_str + " #000000 " + image_path + "\n";

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return false;

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, sock_path.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        // Fallback: daemon not yet listening on socket, spawn miqubg directly
        std::string spawn = "miqubg -i \"" + image_path + "\" -m " + mode_str + " &";
        int res = system(spawn.c_str());
        return (res == 0);
    }

    ssize_t sent = write(fd, cmd.c_str(), cmd.size());
    if (sent < 0) {
        close(fd);
        return false;
    }

    char buf[256];
    std::memset(buf, 0, sizeof(buf));
    read(fd, buf, sizeof(buf) - 1);
    close(fd);
    return true;
}

struct ColorPreset {
    std::string name;
    Color primary;
    Color secondary;
    Color tertiary;
};

static const std::vector<ColorPreset> s_preset_schemes = {
    {"Sapphire",   Color::from_hex("#3584e4"), Color::from_hex("#78aeed"), Color::from_hex("#1c3d5a")},
    {"Emerald",    Color::from_hex("#2ec27e"), Color::from_hex("#8ff0a4"), Color::from_hex("#1b4332")},
    {"Sunset",     Color::from_hex("#e66100"), Color::from_hex("#f6a65e"), Color::from_hex("#5c2600")},
    {"Lavender",   Color::from_hex("#9141ac"), Color::from_hex("#dc8add"), Color::from_hex("#3d1a47")},
    {"Arctic",     Color::from_hex("#00a896"), Color::from_hex("#02c39a"), Color::from_hex("#05383b")},
    {"Rose",       Color::from_hex("#e01b24"), Color::from_hex("#f88379"), Color::from_hex("#5c060a")},
    {"Caramel",    Color::from_hex("#c678dd"), Color::from_hex("#e5c07b"), Color::from_hex("#2c2530")},
    {"Monochrome", Color::from_hex("#7f8c8d"), Color::from_hex("#bdc3c7"), Color::from_hex("#2c3e50")}
};

static int s_selected_scheme_idx = 0;

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

    // 2. Bottom Row: Browse Button, Fit Mode Spinner, Apply Button (Centered)
    auto bottom_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->gravity(Gravity::Center)
        ->spacing(10)
        ->build();

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
            bool ok = send_miqubg_wallpaper_ipc(expanded, item->wallpaper_cfg->fit_mode);
            if (ok) {
                item->last_action_message = "Wallpaper applied via miqubg";
            } else {
                item->last_action_message = "Failed to connect to miqubg daemon";
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
    // 2. Style Section Card
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

    auto style_icon = TextViewBuilder::create()
        ->text("󰔎")
        ->textSize(config->metrics.font_size + 2)
        ->textColor(config->colors.primary)
        ->build();
    style_header->add_view(style_icon);

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

    auto scheme_views = std::make_shared<std::vector<std::shared_ptr<ColorSchemePreviewView>>>();

    for (size_t i = 0; i < s_preset_schemes.size(); ++i) {
        const auto& s = s_preset_schemes[i];
        auto sv = std::make_shared<ColorSchemePreviewView>(
            s.primary, s.secondary, s.tertiary, s.name, static_cast<int>(i) == s_selected_scheme_idx
        );
        scheme_views->push_back(sv);

        sv->set_on_click_listener([this, item, i, scheme_views]() {
            s_selected_scheme_idx = static_cast<int>(i);
            for (size_t j = 0; j < scheme_views->size(); ++j) {
                (*scheme_views)[j]->set_selected(static_cast<int>(j) == s_selected_scheme_idx);
            }
            item->last_action_message = "Preset selected: " + s_preset_schemes[i].name;
            rebuild_preview_column();
            notify_state_changed();
        });

        if (i < 4) {
            row1->add_view(sv, LayoutParams(70, 54));
        } else {
            row2->add_view(sv, LayoutParams(70, 54));
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
}

void MillerView::render_keybindings_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    if (!item) return;
    auto main_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 16)
        ->spacing(10)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    for (size_t i = 0; i < item->keybindings.size(); ++i) {
        const auto& kb = item->keybindings[i];

        if (i > 0) {
            auto divider = LinearLayoutBuilder::create()
                ->backgroundColor(config->colors.outline.with_alpha(0.10f))
                ->build();
            LayoutParams dlp;
            dlp.width = static_cast<int>(LayoutDimension::MatchParent);
            dlp.height = 1;
            main_card->add_view(divider, dlp);
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
            ->text(kb.action)
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

        main_card->add_view(row);
    }

    add_preview_card(main_card);
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

    auto defs_icon = TextViewBuilder::create()
        ->text("󰋼")
        ->textSize(config->metrics.font_size + 2)
        ->textColor(config->colors.primary)
        ->build();
    LayoutParams deficon_lp;
    deficon_lp.width = 24;
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

    if (!item->telemetry.empty()) {
        add_preview_card(create_telemetry_card(item->telemetry, config, item->telemetry_title.empty() ? "Inactivity & security specifications" : item->telemetry_title));
    }
}



} // namespace miqu
