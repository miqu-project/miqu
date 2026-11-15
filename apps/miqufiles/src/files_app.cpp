#include "files_app.hpp"
#include <xkbcommon/xkbcommon-keysyms.h>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace miqu {

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
        ->preferredSize(1020, 660)
        ->closeOnEscape(false)
        ->keyboardInteractive(true)
        ->contentView(m_root_layout)
        ->onClose([this]() {
            if (m_engine) m_engine->quit();
        })
        ->onKey([this](const KeyPressEvent& ev) {
            handle_key_press(ev);
        })
        ->build();

    if (!m_window) {
        std::cerr << "[miqufiles] Failed to build main window.\n";
        return false;
    }

    m_engine->register_window(m_window);
    sync_theme();
    update_header();
    update_hidden_btn_ui();

    return true;
}

void FilesApp::setup_ui() {
    auto config = Config::get();

    m_root_layout = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(0)
        ->backgroundColor(Color::transparent())
        ->build();

    // 1. Top Toolbar (Centered Title "Files", close button on right)
    m_toolbar = ToolbarBuilder::create()
        ->title("Files")
        ->iconResource("system-file-manager")
        ->titleAlignment(TitleAlignment::Center)
        ->showDivider(false)
        ->onClose([this]() {
            if (auto* eng = AppEngine::instance()) {
                eng->quit(0);
            }
        })
        ->closeTooltip("Quit Files")
        ->build();

    LayoutParams toolbar_lp;
    toolbar_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    toolbar_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    m_root_layout->add_view(m_toolbar, toolbar_lp);

    // 2. Ribbon between Toolbar and Main Content (Back button + Clickable Breadcrumbs + Search)
    m_ribbon = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(16, 0)
        ->spacing(12)
        ->backgroundColor(Color::transparent())
        ->build();

    m_back_label = TextViewBuilder::create()
        ->text("‹ Back")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.primary)
        ->build();

    auto back_content = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(10, 3)
        ->spacing(4)
        ->addView(m_back_label)
        ->build();

    m_back_btn = CardViewBuilder::create()
        ->style(CardStyle::Filled)
        ->backgroundColor(config->colors.surface_variant.with_alpha(0.35f))
        ->elevation(0)
        ->tooltip("Back")
        ->tooltipShortcut("Left / Backspace")
        ->addView(back_content)
        ->build();

    m_back_btn->set_on_click_listener([this]() {
        if (m_miller_view && m_miller_view->check_and_update_click_debounce() && m_miller_view->can_navigate_back()) {
            if (m_miller_view->is_searching()) {
                if (m_search_view) {
                    m_search_view->set_query("");
                    m_search_view->set_focused(false);
                }
                if (m_window) m_window->clear_focus();
            }
            m_miller_view->navigate_back();
        }
    });

    LayoutParams back_lp;
    back_lp.width = static_cast<int>(LayoutDimension::WrapContent);
    back_lp.height = 26;
    back_lp.gravity = Gravity::CenterVertical;
    m_ribbon->add_view(m_back_btn, back_lp);

    m_breadcrumb_container = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(2)
        ->gravity(Gravity::CenterVertical)
        ->build();

    LayoutParams breadcrumb_lp;
    breadcrumb_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    breadcrumb_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    breadcrumb_lp.weight = 1.0f;
    breadcrumb_lp.gravity = Gravity::CenterVertical;
    m_ribbon->add_view(m_breadcrumb_container, breadcrumb_lp);

    // Search bar integrated into the ribbon header
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
                if (m_miller_view) {
                    m_miller_view->set_search_query(query);
                }
            },
            [this](const std::string&) {
                if (m_search_view) {
                    m_search_view->set_focused(false);
                }
                if (m_window) {
                    m_window->clear_focus();
                }
                if (m_miller_view) {
                    m_miller_view->navigate_enter();
                }
            }
        )
        ->build();
    m_search_view->set_focused(false);

    LayoutParams search_lp;
    search_lp.width = 240;
    search_lp.height = 28;
    search_lp.gravity = Gravity::CenterVertical;
    m_ribbon->add_view(m_search_view, search_lp);

    m_hidden_icon = ImageViewBuilder::create()
        ->source("show-hidden")
        ->targetSize(16)
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
        ->spacing(6)
        ->gravity(Gravity::Center)
        ->addView(m_hidden_icon, LayoutParams(16, 16, Gravity::CenterVertical))
        ->addView(m_hidden_label)
        ->build();

    m_hidden_btn = CardViewBuilder::create()
        ->style(CardStyle::Filled)
        ->backgroundColor(config->colors.surface_variant.with_alpha(0.35f))
        ->elevation(0)
        ->addView(hidden_content)
        ->build();
    m_hidden_btn->set_focusable(false);
    m_hidden_btn->set_tooltip("Show Hidden Files");
    m_hidden_btn->set_tooltip_shortcut("Ctrl+H");

    m_hidden_btn->set_on_click_listener([this]() {
        toggle_show_hidden();
    });

    LayoutParams hidden_lp;
    hidden_lp.width = static_cast<int>(LayoutDimension::WrapContent);
    hidden_lp.height = 26;
    hidden_lp.gravity = Gravity::CenterVertical;
    m_ribbon->add_view(m_hidden_btn, hidden_lp);

    LayoutParams ribbon_lp;
    ribbon_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    ribbon_lp.height = 36;
    m_root_layout->add_view(m_ribbon, ribbon_lp);

    // 3. Middle Body: MillerView
    bool initial_show_hidden = read_config_show_hidden();
    m_miller_view = std::make_shared<MillerView>();
    m_miller_view->init(m_start_path, initial_show_hidden);
    m_miller_view->set_on_state_changed([this]() {
        update_header();
    });

    LayoutParams miller_lp;
    miller_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    miller_lp.height = static_cast<int>(LayoutDimension::MatchParent);
    miller_lp.weight = 1.0f;
    m_root_layout->add_view(m_miller_view, miller_lp);
}

void FilesApp::sync_theme() {
    auto config = Config::get();
    m_root_layout->set_background_color(Color::transparent());

    if (m_ribbon) {
        m_ribbon->set_background_color(Color::transparent());
    }
    if (m_back_label) {
        m_back_label->set_text_color(config->colors.primary);
        m_back_label->set_text_size(config->metrics.font_size);
    }
    if (m_back_btn) {
        m_back_btn->set_card_background_color(config->colors.surface_variant.with_alpha(0.35f));
    }
    if (m_search_view) {
        m_search_view->set_background_color(config->colors.surface_variant.with_alpha(0.35f));
        m_search_view->set_stroke(0, Color::transparent());
    }
    if (m_hidden_btn) {
        update_hidden_btn_ui();
    }

    if (m_miller_view) {
        m_miller_view->sync_theme();
    }

    update_header();
    if (m_window) {
        m_window->schedule_redraw();
    }
}

void FilesApp::update_header() {
    if (!m_miller_view) return;

    if (m_breadcrumb_container) {
        m_breadcrumb_container->clear_views();
        auto config = Config::get();

        auto add_separator = [this, config]() {
            auto sep = TextViewBuilder::create()
                ->text("›")
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.on_surface_variant.with_alpha(0.45f))
                ->build();
            LayoutParams sep_lp;
            sep_lp.width = static_cast<int>(LayoutDimension::WrapContent);
            sep_lp.height = static_cast<int>(LayoutDimension::WrapContent);
            sep_lp.gravity = Gravity::CenterVertical;
            m_breadcrumb_container->add_view(sep, sep_lp);
        };

        auto add_crumb = [this, config](const std::string& label, std::function<void()> on_click, bool is_current) {
            LayoutParams lp;
            lp.width = static_cast<int>(LayoutDimension::WrapContent);
            lp.height = static_cast<int>(LayoutDimension::WrapContent);
            lp.gravity = Gravity::CenterVertical;

            if (is_current) {
                auto crumb_view = TextViewBuilder::create()
                    ->text(label)
                    ->textSize(config->metrics.font_size)
                    ->textColor(config->colors.on_surface)
                    ->build();
                m_breadcrumb_container->add_view(crumb_view, lp);
            } else {
                auto crumb_btn = ButtonBuilder::create()
                    ->text(label)
                    ->flat(true)
                    ->padding(4, 2)
                    ->cornerRadius(5)
                    ->textSize(config->metrics.font_size)
                    ->textColor(config->colors.on_surface_variant)
                    ->build();
                crumb_btn->set_focusable(false);
                if (on_click) {
                    crumb_btn->set_on_click_listener(std::move(on_click));
                }
                m_breadcrumb_container->add_view(crumb_btn, lp);
            }
        };

        if (m_miller_view->is_searching()) {
            add_crumb("/", [this]() {
                if (m_search_view) {
                    m_search_view->set_query("");
                    m_search_view->set_focused(false);
                }
                if (m_miller_view) {
                    m_miller_view->navigate_to_depth(0);
                }
            }, false);

            add_separator();

            std::string search_label = "Search: \"" + m_miller_view->get_search_query() + "\"";
            auto current = m_miller_view->get_current_selected_item();
            if (current) {
                add_crumb(search_label, [this]() {
                    if (m_search_view) {
                        m_search_view->set_focused(true);
                    }
                }, false);
                add_separator();
                add_crumb(current->title, nullptr, true);
            } else {
                add_crumb(search_label, nullptr, true);
            }
        } else {
            const auto& path = m_miller_view->get_path();
            auto current = m_miller_view->get_current_selected_item();

            if (path.empty()) {
                if (!current) {
                    add_crumb("/", nullptr, true);
                } else {
                    add_crumb("/", [this]() {
                        if (m_miller_view) {
                            m_miller_view->navigate_to_depth(0);
                        }
                    }, false);
                    add_separator();
                    add_crumb(current->title, nullptr, true);
                }
            } else {
                add_crumb("/", [this]() {
                    if (m_miller_view) {
                        m_miller_view->navigate_to_depth(0);
                    }
                }, false);

                for (size_t i = 0; i < path.size(); ++i) {
                    add_separator();
                    size_t target_depth = i + 1;
                    bool is_last = (i == path.size() - 1) && (!current);
                    add_crumb(path[i]->title, is_last ? nullptr : std::function<void()>([this, target_depth]() {
                        if (m_miller_view) {
                            m_miller_view->navigate_to_depth(target_depth);
                        }
                    }), is_last);
                }

                if (current) {
                    add_separator();
                    if (m_miller_view->is_preview_expanded()) {
                        add_crumb(current->title, [this]() {
                            if (m_miller_view) {
                                m_miller_view->set_preview_expanded(false);
                            }
                        }, false);
                    } else {
                        add_crumb(current->title, nullptr, true);
                    }
                }
            }
        }
    }

    if (m_back_btn) {
        bool can_back = m_miller_view->can_navigate_back();
        m_back_btn->set_visibility(can_back ? Visibility::Visible : Visibility::Gone);
    }

    if (m_search_view && !m_miller_view->is_searching() && !m_search_view->get_query().empty()) {
        m_search_view->set_query("");
    }

    if (m_window) {
        m_window->schedule_redraw();
    }
}

void FilesApp::update_hidden_btn_ui() {
    if (!m_miller_view || !m_hidden_btn || !m_hidden_label) return;
    auto config = Config::get();
    bool is_hidden_shown = m_miller_view->is_show_hidden();

    if (is_hidden_shown) {
        m_hidden_label->set_text_color(config->colors.primary);
        if (m_hidden_icon) {
            m_hidden_icon->set_image_resource("view-reveal");
            m_hidden_icon->set_alpha(1.0f);
        }
        m_hidden_btn->set_card_background_color(config->colors.primary.with_alpha(0.20f));
        m_hidden_btn->set_tooltip("Hide Hidden Files");
    } else {
        m_hidden_label->set_text_color(config->colors.on_surface_variant);
        if (m_hidden_icon) {
            m_hidden_icon->set_image_resource("show-hidden");
            m_hidden_icon->set_alpha(0.6f);
        }
        m_hidden_btn->set_card_background_color(config->colors.surface_variant.with_alpha(0.35f));
        m_hidden_btn->set_tooltip("Show Hidden Files");
    }
    m_hidden_btn->set_tooltip_shortcut("Ctrl+H");
}

void FilesApp::toggle_show_hidden() {
    if (!m_miller_view) return;
    if (!m_miller_view->check_and_update_click_debounce()) return;

    m_miller_view->toggle_show_hidden();
    update_hidden_btn_ui();
    update_header();
    if (m_window) {
        m_window->schedule_redraw();
    }
}

void FilesApp::handle_key_press(const KeyPressEvent& event) {
    if (!event.pressed || !m_miller_view) return;

    // 1. Global: Ctrl+F to focus search bar from anywhere
    if (event.has_ctrl() && (event.keysym == XKB_KEY_f || event.keysym == XKB_KEY_F)) {
        if (m_search_view) {
            m_search_view->set_focused(true);
            if (m_window) m_window->schedule_redraw();
            return;
        }
    }

    // Global: Ctrl+H to toggle show/hide hidden files
    if (event.has_ctrl() && (event.keysym == XKB_KEY_h || event.keysym == XKB_KEY_H)) {
        toggle_show_hidden();
        return;
    }

    // F11: Toggle full-width / standard 3-column view on leaf files
    if (event.keysym == XKB_KEY_F11) {
        auto item = m_miller_view->get_current_selected_item();
        if (item && !item->is_category) {
            m_miller_view->set_preview_expanded(!m_miller_view->is_preview_expanded());
            return;
        }
    }

    // 2. Tab / Shift+Tab: Strictly reserved for cycling controls inside preview forms
    if (event.keysym == XKB_KEY_Tab || event.keysym == XKB_KEY_ISO_Left_Tab) {
        if (m_miller_view && m_miller_view->is_preview_focused()) {
            if (event.has_shift() || event.keysym == XKB_KEY_ISO_Left_Tab) {
                m_miller_view->focus_prev_preview_control();
            } else {
                m_miller_view->focus_next_preview_control();
            }
            if (m_window) m_window->schedule_redraw();
        }
        return;
    }

    // 3. State 1: Search Mode
    bool is_search_active = m_search_view && (m_search_view->is_focused() || m_search_view->has_focus() ||
                            (m_search_view->get_edit_text() && m_search_view->get_edit_text()->has_focus()));
    if (is_search_active) {
        if (event.keysym == XKB_KEY_Escape) {
            m_search_view->set_focused(false);
            m_search_view->set_query("");
            if (m_miller_view->is_searching()) {
                m_miller_view->set_search_query("");
            }
            if (m_window) m_window->clear_focus();
            return;
        }
        if (event.keysym == XKB_KEY_Up) {
            m_miller_view->navigate_up();
            return;
        }
        if (event.keysym == XKB_KEY_Down) {
            m_miller_view->navigate_down();
            return;
        }
        if (event.keysym == XKB_KEY_Return || event.keysym == XKB_KEY_KP_Enter) {
            if (m_search_view) {
                m_search_view->set_focused(false);
            }
            if (m_window) {
                m_window->clear_focus();
            }
            m_miller_view->navigate_enter();
            return;
        }
        return;
    }

    // 4. State 2: Detail Mode (Right Pillar Active)
    if (m_miller_view->is_preview_focused()) {
        // In full-width expanded leaf: Backspace or Escape collapses to 3-pillar mode
        if (m_miller_view->is_preview_expanded()) {
            if (event.keysym == XKB_KEY_BackSpace || event.keysym == XKB_KEY_Escape) {
                m_miller_view->set_preview_expanded(false);
                return;
            }
        } else {
            // In standard 3-pillar mode: Left, Backspace, or Escape exits to middle pillar
            if (event.keysym == XKB_KEY_Left || event.keysym == XKB_KEY_h ||
                event.keysym == XKB_KEY_BackSpace || event.keysym == XKB_KEY_Escape) {
                if (m_window) m_window->clear_focus();
                m_miller_view->set_preview_focused(false);
                return;
            }
        }

        // If right pillar is active but no control has focus yet (e.g. empty space clicked)
        if (!m_window || !m_window->get_focused_view() || !m_window->get_focused_view()->has_focus()) {
            if (event.keysym == XKB_KEY_Down || event.keysym == XKB_KEY_Up ||
                event.keysym == XKB_KEY_j || event.keysym == XKB_KEY_k ||
                event.keysym == XKB_KEY_Return || event.keysym == XKB_KEY_KP_Enter) {
                m_miller_view->focus_first_preview_control();
                return;
            }
        }
        return;
    }

    // 5. State 3: List Mode (Middle Pillar Active)
    if (m_miller_view->handle_list_key(event)) {
        update_header();
        return;
    }

    switch (event.keysym) {
        case XKB_KEY_k:
        case XKB_KEY_Up:
            m_miller_view->navigate_up();
            break;

        case XKB_KEY_j:
        case XKB_KEY_Down:
            m_miller_view->navigate_down();
            break;

        case XKB_KEY_l:
        case XKB_KEY_Right:
        case XKB_KEY_Return:
        case XKB_KEY_KP_Enter:
            m_miller_view->navigate_enter();
            break;

        case XKB_KEY_h:
        case XKB_KEY_Left:
        case XKB_KEY_BackSpace:
            m_miller_view->navigate_back();
            break;

        case XKB_KEY_Escape:
            if (!m_miller_view->navigate_back()) {
                if (m_engine) m_engine->quit();
            }
            break;

        case XKB_KEY_period:
            toggle_show_hidden();
            break;

        default:
            break;
    }
}

} // namespace miqu
