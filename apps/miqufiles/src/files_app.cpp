#include "files_app.hpp"
#include "preview_common.hpp"
#include <xkbcommon/xkbcommon-keysyms.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

namespace miqu {

constexpr int MATCH_PARENT = static_cast<int>(LayoutDimension::MatchParent);
constexpr int WRAP_CONTENT = static_cast<int>(LayoutDimension::WrapContent);

static bool read_config_show_hidden() {
    std::string user_conf = FsUtils::get_user_config_file("miqufiles");
    std::string target_conf = fs::exists(user_conf) ? user_conf : FsUtils::get_system_config_file("miqufiles");
    if (!fs::exists(target_conf)) return false;

    std::ifstream file(target_conf);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        size_t hash_pos = line.find('#');
        if (hash_pos != std::string::npos) line = line.substr(0, hash_pos);
        size_t semi_pos = line.find(';');
        if (semi_pos != std::string::npos) line = line.substr(0, semi_pos);

        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = line.substr(0, eq_pos);
        std::string val = line.substr(eq_pos + 1);

        auto trim = [](std::string s) {
            size_t p1 = s.find_first_not_of(" \t\r\n\"'");
            if (p1 == std::string::npos) return std::string();
            size_t p2 = s.find_last_not_of(" \t\r\n\"'");
            return s.substr(p1, p2 - p1 + 1);
        };

        key = trim(key);
        val = trim(val);
        if (key == "show_hidden") {
            std::transform(val.begin(), val.end(), val.begin(), [](unsigned char c) { return std::tolower(c); });
            return (val == "true" || val == "1" || val == "yes");
        }
    }
    return false;
}

FilesApp::FilesApp(std::shared_ptr<AppEngine> engine, const std::string& start_path)
    : m_engine(std::move(engine)), m_start_path(start_path) {}

bool FilesApp::init() {
    if (!m_engine) return false;

    setup_ui();

    m_window = WindowBuilder::create()
        ->title("Files")
        ->appId("miqufiles")
        ->role(WindowRole::Toplevel)
        ->preferredSize(1120, 680)
        ->closeOnEscape(false)
        ->keyboardInteractive(true)
        ->contentView(m_root_layout)
        ->onClose([this]() {
            if (m_engine) m_engine->quit();
        })
        ->onShortcut([this](const KeyPressEvent& ev) -> bool {
            if (!ev.pressed) return false;
            if (m_search_view && m_search_view->is_focused()) {
                if (ev.keysym == XKB_KEY_Escape) {
                    m_search_view->set_query("");
                    m_search_view->set_focused(false);
                    if (m_window) m_window->clear_focus();
                    if (m_explorer_view) m_explorer_view->set_search_query("");
                    return true;
                }
                return false;
            }
            if (ev.has_ctrl() && (ev.keysym == XKB_KEY_f || ev.keysym == XKB_KEY_F)) {
                if (m_search_view) {
                    m_search_view->set_focused(true);
                    if (m_window) m_window->set_focused_view(m_search_view.get());
                }
                return true;
            }
            handle_key_press(ev);
            return true;
        })
        ->onKey([this](const KeyPressEvent& ev) -> bool {
            if (!ev.pressed) return false;
            if (m_search_view && m_search_view->is_focused()) {
                if (ev.keysym == XKB_KEY_Escape) {
                    m_search_view->set_query("");
                    m_search_view->set_focused(false);
                    if (m_window) m_window->clear_focus();
                    if (m_explorer_view) m_explorer_view->set_search_query("");
                    return true;
                }
                if (m_search_view->on_key(ev)) {
                    return true;
                }
            }
            handle_key_press(ev);
            return true;
        })
        ->build();

    if (!m_window) return false;
    m_explorer_view->set_explorer_window(m_window);
    update_header();
    return true;
}



class PreviewToggleIconView : public View {
public:
    explicit PreviewToggleIconView(std::function<bool()> is_active)
        : m_is_active(std::move(is_active)) {}

    void draw(cairo_t* cr, const Rect& bounds) override {
        if (!cr || bounds.width <= 0 || bounds.height <= 0) return;
        auto config = Config::get();
        if (!config) return;

        bool active = m_is_active ? m_is_active() : true;
        Color col = active ? config->colors.on_surface : config->colors.on_surface_variant.with_alpha(0.55f);

        double icon_w = 15.0;
        double icon_h = 13.0;
        double x = std::round(bounds.x + (bounds.width - icon_w) / 2.0);
        double y = std::round(bounds.y + (bounds.height - icon_h) / 2.0);
        double r = 2.0;
        double divider_x = x + icon_w - 4.5;

        cairo_save(cr);
        cairo_set_source_rgba(cr, col.r, col.g, col.b, col.a);
        cairo_set_line_width(cr, 1.35);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

        // Outer rounded rect
        cairo_new_sub_path(cr);
        cairo_arc(cr, x + icon_w - r, y + r, r, -M_PI / 2, 0);
        cairo_arc(cr, x + icon_w - r, y + icon_h - r, r, 0, M_PI / 2);
        cairo_arc(cr, x + r, y + icon_h - r, r, M_PI / 2, M_PI);
        cairo_arc(cr, x + r, y + r, r, M_PI, 3 * M_PI / 2);
        cairo_close_path(cr);
        cairo_stroke(cr);

        // Vertical divider for right sidebar pane
        cairo_move_to(cr, divider_x, y);
        cairo_line_to(cr, divider_x, y + icon_h);
        cairo_stroke(cr);

        // Subtle fill for preview section when active
        if (active) {
            cairo_set_source_rgba(cr, col.r, col.g, col.b, 0.22);
            cairo_rectangle(cr, divider_x, y, x + icon_w - divider_x, icon_h);
            cairo_fill(cr);
        }

        cairo_restore(cr);
    }

    Size measure_size() const override { return Size(28, 28); }
    Size measure_size(int) const override { return Size(28, 28); }

private:
    std::function<bool()> m_is_active;
};

class PowerIconView : public View {
public:
    void draw(cairo_t* cr, const Rect& bounds) override {
        if (!cr || bounds.width <= 0 || bounds.height <= 0) return;
        auto config = Config::get();
        if (!config) return;

        Color col = config->colors.on_surface_variant;
        double cx = bounds.x + bounds.width / 2.0;
        double cy = bounds.y + bounds.height / 2.0 + 0.5;
        double radius = 5.5;

        cairo_save(cr);
        cairo_set_source_rgba(cr, col.r, col.g, col.b, col.a);
        cairo_set_line_width(cr, 1.4);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

        // Circle arc leaving the top ~70 degrees open
        cairo_arc(cr, cx, cy, radius, -M_PI / 2.0 + 0.65, 3 * M_PI / 2.0 - 0.65);
        cairo_stroke(cr);

        // Vertical line through the top
        cairo_move_to(cr, cx, cy - radius - 1.5);
        cairo_line_to(cr, cx, cy - 0.5);
        cairo_stroke(cr);

        cairo_restore(cr);
    }

    Size measure_size() const override { return Size(28, 28); }
    Size measure_size(int) const override { return Size(28, 28); }
};

void FilesApp::setup_ui() {
    auto config = Config::get();
    if (!config) return;

    m_root_layout = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(0)
        ->backgroundColor(Color::transparent())
        ->build();

    constexpr int kToolbarItemH = 28;

    // 1. Top Bar (Horizontal header: Logo + Nav + Breadcrumbs + Spacer + Search + View Mode + Hidden + Preview + Power)
    m_top_bar = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(12, 6, 12, 6)
        ->spacing(8)
        ->gravity(Gravity::CenterVertical)
        ->backgroundColor(Color::transparent())
        ->build();
    m_root_layout->add_view(m_top_bar, LayoutParams(MATCH_PARENT, WRAP_CONTENT));

    // Corner File App Logo Box (Adapts to system icon theme)
    m_app_icon = ImageViewBuilder::create()
        ->source("system-file-manager")
        ->targetSize(18)
        ->fitMode(FitMode::Contain)
        ->build();

    m_app_logo_box = CardViewBuilder::create()
        ->style(CardStyle::Filled)
        ->backgroundColor(config->colors.surface_variant.with_alpha(0.35f))
        ->elevation(0)
        ->cornerRadius(get_btn_radius(config))
        ->tooltip("Files")
        ->addView(m_app_icon)
        ->build();
    m_app_icon->set_layout_params(LayoutParams(MATCH_PARENT, MATCH_PARENT, Gravity::Center));
    m_app_logo_box->set_focusable(false);
    m_top_bar->add_view(m_app_logo_box, LayoutParams(kToolbarItemH, kToolbarItemH, Gravity::CenterVertical));

    auto make_pill_btn = [&](std::shared_ptr<CardView>& card_out, std::shared_ptr<TextView>& lbl_out,
                             const std::string& text, const std::string& tooltip, const std::string& shortcut,
                             std::function<void()> on_click, bool add_to_bar = true) {
        lbl_out = TextViewBuilder::create()
            ->text(text)
            ->textSize(config->metrics.font_size)
            ->textAlignment(TextAlignment::Center)
            ->textColor(config->colors.on_surface_variant)
            ->build();

        auto content = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->padding(add_to_bar ? 8 : 0, add_to_bar ? 3 : 0)
            ->gravity(Gravity::Center)
            ->addView(lbl_out)
            ->build();

        card_out = CardViewBuilder::create()
            ->style(CardStyle::Filled)
            ->backgroundColor(config->colors.surface_variant.with_alpha(0.35f))
            ->elevation(0)
            ->cornerRadius(get_btn_radius(config))
            ->tooltip(tooltip)
            ->tooltipShortcut(shortcut)
            ->addView(content)
            ->build();
        card_out->set_focusable(false);
        card_out->set_on_click_listener(std::move(on_click));

        LayoutParams lp(WRAP_CONTENT, kToolbarItemH, Gravity::CenterVertical);
        card_out->set_layout_params(lp);
        if (add_to_bar) {
            m_top_bar->add_view(card_out);
        }
    };

    // Group 1: Navigation capsule (Back, Forward, Up)
    auto nav_group = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(0)
        ->gravity(Gravity::Center)
        ->build();

    auto make_nav_btn = [&](std::shared_ptr<CardView>& card_out, std::shared_ptr<TextView>& lbl_out,
                            const std::string& text, int text_size, const std::string& tooltip,
                            const std::string& shortcut, std::function<void()> on_click) {
        lbl_out = TextViewBuilder::create()
            ->text(text)
            ->textSize(text_size)
            ->textAlignment(TextAlignment::Center)
            ->textColor(config->colors.on_surface_variant)
            ->build();

        auto content = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->gravity(Gravity::Center)
            ->addView(lbl_out)
            ->build();

        card_out = CardViewBuilder::create()
            ->style(CardStyle::Filled)
            ->backgroundColor(Color::transparent())
            ->elevation(0)
            ->cornerRadius(get_btn_radius(config))
            ->tooltip(tooltip)
            ->tooltipShortcut(shortcut)
            ->addView(content)
            ->build();
        card_out->set_focusable(false);
        card_out->set_on_click_listener(std::move(on_click));
        card_out->set_layout_params(LayoutParams(26, kToolbarItemH, Gravity::CenterVertical));
        nav_group->add_view(card_out);
    };

    make_nav_btn(m_back_btn, m_back_label, "‹", config->metrics.font_size + 5, "Back", "Alt+Left", [this]() {
        if (m_explorer_view && m_explorer_view->check_and_update_click_debounce()) {
            m_explorer_view->navigate_back();
        }
    });

    make_nav_btn(m_forward_btn, m_forward_label, "›", config->metrics.font_size + 5, "Forward", "Alt+Right", [this]() {
        if (m_explorer_view && m_explorer_view->check_and_update_click_debounce()) {
            m_explorer_view->navigate_forward();
        }
    });

    make_nav_btn(m_up_btn, m_up_label, "↑", config->metrics.font_size + 3, "Up", "Backspace", [this]() {
        if (m_explorer_view && m_explorer_view->check_and_update_click_debounce()) {
            m_explorer_view->navigate_up();
        }
    });

    auto nav_card = CardViewBuilder::create()
        ->style(CardStyle::Filled)
        ->backgroundColor(config->colors.surface_variant.with_alpha(0.25f))
        ->elevation(0)
        ->cornerRadius(get_btn_radius(config))
        ->padding(2, 0)
        ->addView(nav_group)
        ->build();
    m_top_bar->add_view(nav_card, LayoutParams(WRAP_CONTENT, kToolbarItemH, Gravity::CenterVertical));

    // Group 2: Breadcrumb capsule
    m_breadcrumb_container = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(4)
        ->padding(4, 0)
        ->gravity(Gravity::CenterVertical)
        ->build();
    auto bc_card = CardViewBuilder::create()
        ->style(CardStyle::Filled)
        ->backgroundColor(config->colors.surface_variant.with_alpha(0.25f))
        ->elevation(0)
        ->cornerRadius(get_btn_radius(config))
        ->addView(m_breadcrumb_container)
        ->build();
    m_top_bar->add_view(bc_card, LayoutParams(WRAP_CONTENT, kToolbarItemH, Gravity::CenterVertical));

    // Flexible Spacer between left and right groups
    auto spacer = LinearLayoutBuilder::create()->build();
    m_top_bar->add_view(spacer, LayoutParams(0, kToolbarItemH, 1.0f));

    // Search (in top bar)
    m_search_view = SearchViewBuilder::create()
        ->hint("Search")
        ->focused(false)
        ->focusBorder(false)
        ->padding(10, 2)
        ->elevation(0)
        ->backgroundColor(config->colors.surface_variant.with_alpha(0.35f))
        ->stroke(0, Color::transparent())
        ->onQueryTextListener(
            [this](const std::string& query) {
                if (m_explorer_view) m_explorer_view->set_search_query(query);
            },
            [this](const std::string&) {
                if (m_search_view) m_search_view->set_focused(false);
                if (m_window) m_window->clear_focus();
                if (m_explorer_view) m_explorer_view->navigate_enter();
            }
        )
        ->build();
    m_search_view->set_focused(false);
    m_search_view->set_layout_params(LayoutParams(200, kToolbarItemH, Gravity::CenterVertical));
    m_top_bar->add_view(m_search_view);

    // View Mode Cycle Button: Details -> Grid -> List (in top bar)
    make_pill_btn(m_view_mode_btn, m_view_mode_lbl, "Details", "Switch View Mode", "Ctrl+1 / 2 / 3", [this]() {
        if (!m_explorer_view) return;
        ExplorerViewMode cur = m_explorer_view->get_view_mode();
        ExplorerViewMode next = ExplorerViewMode::Grid;
        if (cur == ExplorerViewMode::Details) next = ExplorerViewMode::Grid;
        else if (cur == ExplorerViewMode::Grid) next = ExplorerViewMode::List;
        else if (cur == ExplorerViewMode::List) next = ExplorerViewMode::Details;
        m_explorer_view->set_view_mode(next);
    });

    // Hidden Toggle (in top bar)
    m_hidden_icon = ImageViewBuilder::create()
        ->source("show-hidden")
        ->targetSize(15)
        ->fitMode(FitMode::Contain)
        ->alpha(0.6f)
        ->build();

    m_hidden_label = TextViewBuilder::create()
        ->text("Hidden")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface_variant)
        ->build();

    auto hidden_content = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(8, 3)
        ->spacing(5)
        ->gravity(Gravity::Center)
        ->addView(m_hidden_icon, LayoutParams(15, 15, Gravity::CenterVertical))
        ->addView(m_hidden_label)
        ->build();

    m_hidden_btn = CardViewBuilder::create()
        ->style(CardStyle::Filled)
        ->backgroundColor(config->colors.surface_variant.with_alpha(0.35f))
        ->elevation(0)
        ->cornerRadius(get_btn_radius(config))
        ->tooltip("Show Hidden Files")
        ->tooltipShortcut("Ctrl+H")
        ->addView(hidden_content)
        ->build();
    m_hidden_btn->set_focusable(false);
    m_hidden_btn->set_on_click_listener([this]() { toggle_show_hidden(); });
    m_hidden_btn->set_layout_params(LayoutParams(WRAP_CONTENT, kToolbarItemH, Gravity::CenterVertical));
    m_top_bar->add_view(m_hidden_btn);

    // Preview Pane Toggle Button (in top bar)
    auto preview_icon = std::make_shared<PreviewToggleIconView>([this]() {
        return m_explorer_view && m_explorer_view->is_preview_visible();
    });

    m_preview_btn = CardViewBuilder::create()
        ->style(CardStyle::Filled)
        ->backgroundColor(config->colors.surface_variant.with_alpha(0.35f))
        ->elevation(0)
        ->cornerRadius(get_btn_radius(config))
        ->tooltip("Toggle Preview Pane")
        ->tooltipShortcut("Alt+P")
        ->addView(preview_icon)
        ->build();
    preview_icon->set_layout_params(LayoutParams(MATCH_PARENT, MATCH_PARENT, Gravity::Center));
    m_preview_btn->set_focusable(false);
    m_preview_btn->set_on_click_listener([this]() { toggle_preview_pane(); });
    m_preview_btn->set_layout_params(LayoutParams(28, kToolbarItemH, Gravity::CenterVertical));
    m_top_bar->add_view(m_preview_btn);

    // Power / Close Button (in top bar)
    auto close_icon = std::make_shared<PowerIconView>();
    m_close_btn = CardViewBuilder::create()
        ->style(CardStyle::Filled)
        ->backgroundColor(config->colors.surface_variant.with_alpha(0.35f))
        ->elevation(0)
        ->cornerRadius(get_btn_radius(config))
        ->tooltip("Quit Files")
        ->tooltipShortcut("Ctrl+Q")
        ->addView(close_icon)
        ->build();
    close_icon->set_layout_params(LayoutParams(MATCH_PARENT, MATCH_PARENT, Gravity::Center));
    m_close_btn->set_focusable(false);
    m_close_btn->set_on_click_listener([this]() {
        if (auto* eng = AppEngine::instance()) {
            eng->quit(0);
        } else if (m_window) {
            m_window->request_close();
        }
    });
    m_close_btn->set_layout_params(LayoutParams(28, kToolbarItemH, Gravity::CenterVertical));
    m_top_bar->add_view(m_close_btn);

    auto top_div = DividerViewBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->thickness(1)
        ->color(config->colors.outline.with_alpha(0.25f))
        ->build();
    m_root_layout->add_view(top_div, LayoutParams(MATCH_PARENT, 1));

    // 2. Explorer View
    bool initial_hidden = read_config_show_hidden();
    m_explorer_view = std::make_shared<ExplorerView>(m_start_path, initial_hidden);
    m_explorer_view->set_on_state_changed([this]() { update_header(); });
    m_root_layout->add_view(m_explorer_view, LayoutParams(MATCH_PARENT, MATCH_PARENT, 1.0f));
}

void FilesApp::update_header() {
    auto config = Config::get();
    if (!config || !m_explorer_view) return;

    if (m_window) {
        m_window->set_title(m_explorer_view->get_current_dir_title().empty() ? "Files"
                                                                             : m_explorer_view->get_current_dir_title() + " — Files");
    }

    // Rebuild clickable breadcrumbs from m_explorer_view->get_current_dir()
    if (m_breadcrumb_container) {
        m_breadcrumb_container->clear_views();
        fs::path cur = m_explorer_view->get_current_dir();
        if (!cur.empty()) {
            std::vector<fs::path> segments;
            fs::path acc;
            for (const auto& part : cur) {
                acc /= part;
                segments.push_back(acc);
            }

            // Show up to last 4 segments if deep
            size_t start_i = segments.size() > 5 ? segments.size() - 5 : 0;
            for (size_t i = start_i; i < segments.size(); ++i) {
                if (i > start_i) {
                    auto sep = TextViewBuilder::create()
                        ->text("›")
                        ->caption(true)
                        ->textColor(config->colors.on_surface_variant.with_alpha(0.45f))
                        ->build();
                    m_breadcrumb_container->add_view(sep, LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity::CenterVertical));
                }

                fs::path seg_path = segments[i];
                std::string label_str = seg_path.filename().string();
                if (label_str.empty()) label_str = "/";
                bool is_last = (i + 1 == segments.size());

                auto lbl = TextViewBuilder::create()
                    ->text(label_str)
                    ->caption(true)
                    ->bold(is_last)
                    ->textColor(is_last ? config->colors.on_surface : config->colors.on_surface_variant)
                    ->build();

                auto btn = CardViewBuilder::create()
                    ->style(CardStyle::Filled)
                    ->backgroundColor(is_last ? config->colors.surface_variant.with_alpha(0.55f) : Color::transparent())
                    ->elevation(0)
                    ->cornerRadius(get_chip_radius(config))
                    ->padding(8, 2)
                    ->addView(lbl)
                    ->build();
                btn->set_focusable(false);
                btn->set_on_click_listener([this, seg_path]() {
                    if (m_explorer_view) {
                        if (m_search_view && m_explorer_view->is_searching()) {
                            m_search_view->set_query("");
                            m_explorer_view->set_search_query("");
                        }
                        m_explorer_view->navigate_to_dir(seg_path, true);
                    }
                });
                m_breadcrumb_container->add_view(btn, LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity::CenterVertical));
            }

            // Copy Path button inside breadcrumb container
            auto copy_lbl = TextViewBuilder::create()
                ->text("⧉")
                ->textSize(config->metrics.font_size + 1)
                ->textAlignment(TextAlignment::Center)
                ->textColor(config->colors.on_surface_variant.with_alpha(0.6f))
                ->build();

            auto copy_btn = CardViewBuilder::create()
                ->style(CardStyle::Filled)
                ->backgroundColor(Color::transparent())
                ->elevation(0)
                ->cornerRadius(get_chip_radius(config))
                ->tooltip("Copy Path")
                ->tooltipShortcut("Ctrl+Shift+C")
                ->padding(5, 2)
                ->addView(copy_lbl)
                ->build();
            copy_btn->set_focusable(false);
            copy_btn->set_on_click_listener([cur]() {
                ExplorerView::copy_text_to_clipboard(cur.string());
            });
            m_breadcrumb_container->add_view(copy_btn, LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity::CenterVertical));
        }
    }

    update_nav_buttons_ui();
    update_view_mode_buttons_ui();
    update_hidden_btn_ui();
    update_preview_btn_ui();
}

void FilesApp::update_nav_buttons_ui() {
    auto config = Config::get();
    if (!config || !m_explorer_view) return;

    auto style_nav = [&](const std::shared_ptr<CardView>& btn, const std::shared_ptr<TextView>& lbl, bool enabled) {
        if (!btn || !lbl) return;
        btn->set_card_background_color(Color::transparent());
        lbl->set_text_color(enabled ? config->colors.on_surface : config->colors.on_surface_variant.with_alpha(0.35f));
    };

    style_nav(m_back_btn, m_back_label, m_explorer_view->can_navigate_back());
    style_nav(m_forward_btn, m_forward_label, m_explorer_view->can_navigate_forward());
    style_nav(m_up_btn, m_up_label, m_explorer_view->can_navigate_up());
}

void FilesApp::update_view_mode_buttons_ui() {
    auto config = Config::get();
    if (!config || !m_explorer_view || !m_view_mode_btn || !m_view_mode_lbl) return;

    ExplorerViewMode mode = m_explorer_view->get_view_mode();
    const char* label = "Grid";
    if (mode == ExplorerViewMode::List) label = "List";
    else if (mode == ExplorerViewMode::Details) label = "Details";

    m_view_mode_lbl->set_text(label);
    m_view_mode_btn->set_card_background_color(config->colors.surface_variant.with_alpha(0.35f));
    m_view_mode_lbl->set_text_color(config->colors.on_surface_variant);
}

void FilesApp::update_hidden_btn_ui() {
    auto config = Config::get();
    if (!config || !m_explorer_view) return;
    bool active = m_explorer_view->get_show_hidden();

    if (m_hidden_btn) {
        m_hidden_btn->set_card_background_color(active ? config->colors.primary_container
                                                       : config->colors.surface_variant.with_alpha(0.35f));
    }
    if (m_hidden_icon) m_hidden_icon->set_alpha(active ? 1.0f : 0.6f);
    if (m_hidden_label) {
        m_hidden_label->set_text_color(active ? config->colors.primary : config->colors.on_surface_variant);
    }
}

void FilesApp::update_preview_btn_ui() {
    auto config = Config::get();
    if (!config || !m_explorer_view || !m_preview_btn) return;
    bool active = m_explorer_view->is_preview_visible();

    m_preview_btn->set_card_background_color(active ? config->colors.primary_container.with_alpha(0.45f)
                                                    : config->colors.surface_variant.with_alpha(0.35f));
    m_preview_btn->request_redraw();
}

void FilesApp::toggle_show_hidden() {
    if (!m_explorer_view) return;
    m_explorer_view->set_show_hidden(!m_explorer_view->get_show_hidden());
    update_hidden_btn_ui();
}

void FilesApp::toggle_preview_pane() {
    if (!m_explorer_view) return;
    m_explorer_view->toggle_preview();
    update_preview_btn_ui();
}

void FilesApp::sync_theme() {
    auto config = Config::get();
    if (m_app_icon) {
        m_app_icon->invalidate_surface_cache();
        m_app_icon->request_redraw();
    }
    if (m_app_logo_box && config) {
        m_app_logo_box->set_card_background_color(config->colors.surface_variant.with_alpha(0.35f));
        m_app_logo_box->set_corner_radius(get_btn_radius(config));
    }
    if (m_explorer_view) m_explorer_view->sync_theme();
    update_header();
}

void FilesApp::handle_key_press(const KeyPressEvent& event) {
    if (!event.pressed || !m_explorer_view) return;

    if (m_explorer_view->handle_key(event)) {
        update_header();
        return;
    }

    if (event.has_ctrl() && event.has_shift() && (event.keysym == XKB_KEY_c || event.keysym == XKB_KEY_C)) {
        if (m_explorer_view) {
            ExplorerView::copy_text_to_clipboard(m_explorer_view->get_current_dir().string());
        }
        return;
    }

    if (event.has_ctrl() && (event.keysym == XKB_KEY_h || event.keysym == XKB_KEY_H)) {
        toggle_show_hidden();
        return;
    }

    if ((event.has_alt() || event.has_ctrl()) && (event.keysym == XKB_KEY_p || event.keysym == XKB_KEY_P)) {
        toggle_preview_pane();
        return;
    }

    if (event.has_ctrl() && (event.keysym == XKB_KEY_q || event.keysym == XKB_KEY_Q)) {
        if (auto* eng = AppEngine::instance()) {
            eng->quit(0);
        } else if (m_window) {
            m_window->request_close();
        }
        return;
    }
}

} // namespace miqu
