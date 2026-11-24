#include "miller_view.hpp"
#include "preview_common.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace miqu {

constexpr int MATCH_PARENT = static_cast<int>(LayoutDimension::MatchParent);
constexpr int WRAP_CONTENT = static_cast<int>(LayoutDimension::WrapContent);

MillerView::MillerView() : LinearLayout(Orientation::Horizontal) {}

bool MillerView::check_and_update_click_debounce() {
    auto now = std::chrono::steady_clock::now();
    uint64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    constexpr uint64_t DEBOUNCE_THRESHOLD_MS = 250;
    if (m_last_click_time_ms > 0 && (now_ms - m_last_click_time_ms) < DEBOUNCE_THRESHOLD_MS) {
        return false;
    }
    m_last_click_time_ms = now_ms;
    return true;
}

void MillerView::refresh_model() {
    m_root_tree = FilesModel::build_default_tree(m_start_path, m_show_hidden);
    if (m_preview_layout) {
        rebuild_columns();
        notify_state_changed();
    }
}

void MillerView::set_show_hidden(bool show) {
    if (m_show_hidden == show) return;
    m_show_hidden = show;

    // 1. Remember what item was selected (by id / path)
    std::string selected_id;
    auto cur_item = get_current_selected_item();
    if (cur_item) {
        selected_id = cur_item->id;
    }

    // 2. Re-scan root tree
    m_root_tree = FilesModel::build_default_tree(m_start_path, m_show_hidden);

    // 3. For each folder in m_path, reload its children with m_show_hidden
    std::vector<std::shared_ptr<FileItem>>* parent_level = &m_root_tree;

    for (size_t depth = 0; depth < m_path.size(); ++depth) {
        auto& path_item = m_path[depth];

        // Find path_item in parent_level by id
        int new_idx = -1;
        for (size_t i = 0; i < parent_level->size(); ++i) {
            if ((*parent_level)[i]->id == path_item->id) {
                new_idx = static_cast<int>(i);
                path_item = (*parent_level)[i]; // update pointer to new instance
                break;
            }
        }

        if (depth < m_history_indices.size()) {
            if (new_idx >= 0) {
                m_history_indices[depth] = new_idx;
            }
        }

        path_item->reload_children(m_show_hidden);
        parent_level = &(path_item->children);
    }

    // 4. Update current selection index in current list
    const auto* current_pool = m_path.empty() ? &m_root_tree : &(m_path.back()->children);
    int new_current_idx = 0;
    if (!selected_id.empty()) {
        for (size_t i = 0; i < current_pool->size(); ++i) {
            if ((*current_pool)[i]->id == selected_id) {
                new_current_idx = static_cast<int>(i);
                break;
            }
        }
    }
    if (!current_pool->empty()) {
        m_current_index = std::clamp(new_current_idx, 0, static_cast<int>(current_pool->size()) - 1);
    } else {
        m_current_index = 0;
    }

    // 5. If searching, re-run search against updated pool
    if (!m_search_query.empty()) {
        std::string q = m_search_query;
        m_search_query.clear(); // force set_search_query to rebuild
        set_search_query(q);
    } else {
        rebuild_columns();
        notify_state_changed();
    }
}

void MillerView::toggle_show_hidden() {
    set_show_hidden(!m_show_hidden);
}

void MillerView::execute_sibling_swap(const std::shared_ptr<FileItem>& item, size_t index) {
    if (!item) return;

    if (item->is_category) {
        if (!m_path.empty()) {
            if (m_path.back() == item) {
                return;
            }
            m_path.back() = item;
        } else {
            m_path.push_back(item);
        }
        if (!m_history_indices.empty()) {
            m_history_indices.back() = static_cast<int>(index);
        } else {
            m_history_indices.push_back(static_cast<int>(index));
        }
        if (m_preview_expanded) {
            set_preview_expanded(false);
        }
        m_current_index = 0;
        m_preview_focused = false;
        if (m_window) {
            m_window->clear_focus();
        }
        rebuild_columns();
        notify_state_changed();
    } else {
        execute_shift_up(index);
    }
}

void MillerView::execute_shift_up(size_t index) {
    if (!m_path.empty()) {
        m_path.pop_back();
        if (!m_history_indices.empty()) {
            m_history_indices.pop_back();
        }
    }
    if (m_preview_expanded) {
        set_preview_expanded(false);
    }
    m_current_index = static_cast<int>(index);
    m_preview_focused = false;
    if (m_window) {
        m_window->clear_focus();
    }
    rebuild_columns();
    notify_state_changed();
}

void MillerView::handle_parent_item_click(size_t idx) {
    if (!check_and_update_click_debounce()) return;

    if (m_path.size() >= 2) {
        m_path[m_path.size() - 2]->ensure_children();
    }
    const auto* parent_siblings = (!m_search_query.empty() || m_path.size() <= 1)
        ? &m_root_tree
        : &(m_path[m_path.size() - 2]->children);
    if (idx >= parent_siblings->size()) return;
    const auto& item = (*parent_siblings)[idx];

    execute_sibling_swap(item, idx);
}

void MillerView::init(const std::string& start_path, bool show_hidden) {
    m_start_path = start_path;
    m_show_hidden = show_hidden;
    m_root_tree = FilesModel::build_default_tree(m_start_path, m_show_hidden);

    set_orientation(Orientation::Horizontal);
    set_padding(12, 4, 12, 12);
    set_divider_spacing(10);
    set_background_color(Color::transparent());

    auto config = Config::get();
    int inner_rad = get_inner_radius(config);

    // 1. Column 1: Parent Pillar (width 280)
    m_parent_pillar = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(4)
        ->padding(10, 10)
        ->backgroundColor(Color::transparent())
        ->build();

    m_parent_hero = create_parent_hero_box(config);
    m_parent_pillar->add_view(m_parent_hero, LayoutParams(MATCH_PARENT, MATCH_PARENT, 1.0f));

    m_parent_list = ListViewBuilder::create()
        ->itemHeight(40)
        ->spacing(4)
        ->itemCornerRadius(inner_rad)
        ->scrollbar(false)
        ->build();
    m_parent_list->set_focusable(false);
    m_parent_list->set_draw_focus_ring(false);
    m_parent_list->set_keyboard_navigation_enabled(false);
    m_parent_list->set_on_item_click_listener([this](size_t idx, std::shared_ptr<View>) {
        handle_parent_item_click(idx);
    });
    m_parent_pillar->add_view(m_parent_list, LayoutParams(MATCH_PARENT, MATCH_PARENT, 1.0f));
    add_view(m_parent_pillar, LayoutParams(280, MATCH_PARENT));

    // 2. Column 2: Current (Center) Pillar (width 300)
    m_current_pillar = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(4)
        ->padding(10, 10)
        ->backgroundColor(Color::transparent())
        ->build();

    m_empty_search_box = create_empty_search_box(config);
    m_empty_search_box->set_visibility(Visibility::Gone);
    m_current_pillar->add_view(m_empty_search_box, LayoutParams(MATCH_PARENT, MATCH_PARENT, 1.0f));

    m_current_list = ListViewBuilder::create()
        ->itemHeight(40)
        ->spacing(4)
        ->itemCornerRadius(inner_rad)
        ->scrollbar(false)
        ->selectionMode(SelectionMode::Multiple)
        ->build();
    m_current_list->set_focusable(false);
    m_current_list->set_draw_focus_ring(false);
    m_current_list->set_on_selection_changed([this](const std::unordered_set<int>&) {
        int sel = m_current_list->get_selected_index();
        if (sel >= 0) {
            m_current_index = sel;
            rebuild_preview_column();
            notify_state_changed();
        }
    });
    m_current_list->set_on_item_click_listener([this](size_t idx, std::shared_ptr<View>) {
        if (!check_and_update_click_debounce()) return;
        int clicked_idx = static_cast<int>(idx);
        if (clicked_idx != m_current_index) {
            if (m_preview_expanded) {
                set_preview_expanded(false);
            }
            m_current_index = clicked_idx;
            m_preview_focused = false;
            if (m_window) {
                m_window->clear_focus();
            }
            rebuild_preview_column();
            notify_state_changed();
        } else {
            bool has_modifier = m_window && (m_window->has_shift() || m_window->has_ctrl());
            if (!has_modifier) {
                auto item = get_current_selected_item();
                if (item && item->is_category) {
                    navigate_enter();
                }
            }
        }
    });
    m_current_pillar->add_view(m_current_list, LayoutParams(MATCH_PARENT, MATCH_PARENT, 1.0f));
    add_view(m_current_pillar, LayoutParams(300, MATCH_PARENT));

    // 3. Column 3: Preview (Right) Pillar Container (weight 1.0f)
    m_preview_pillar = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(4)
        ->backgroundColor(Color::transparent())
        ->build();

    // 3a. Header Ribbon for Interactive Pillar
    m_preview_ribbon = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->gravity(Gravity::CenterVertical)
        ->padding(14, 2, 14, 2)
        ->spacing(8)
        ->backgroundColor(Color::transparent())
        ->build();

    m_preview_toggle_icon = TextViewBuilder::create()
        ->text("|‹")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.primary)
        ->build();

    auto icon_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(8, 2)
        ->gravity(Gravity::Center)
        ->addView(m_preview_toggle_icon)
        ->build();

    m_preview_toggle_btn = CardViewBuilder::create()
        ->style(CardStyle::Filled)
        ->backgroundColor(config->colors.surface_variant.with_alpha(0.35f))
        ->elevation(0)
        ->tooltip("Expand full width")
        ->addView(icon_box)
        ->build();
    m_preview_toggle_btn->set_on_click_listener([this]() {
        if (!check_and_update_click_debounce()) return;
        set_preview_expanded(!m_preview_expanded);
    });
    m_preview_toggle_btn->set_focusable(true);

    m_preview_title = TextViewBuilder::create()
        ->text("")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface_variant)
        ->ellipsize(true)
        ->build();
    m_preview_ribbon->add_view(m_preview_title, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));

    m_preview_actions = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->gravity(Gravity::CenterVertical)
        ->spacing(6)
        ->backgroundColor(Color::transparent())
        ->build();
    m_preview_ribbon->add_view(m_preview_actions, LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity::CenterVertical));

    m_preview_ribbon->add_view(m_preview_toggle_btn, LayoutParams(WRAP_CONTENT, 26, Gravity::CenterVertical));
    m_preview_pillar->add_view(m_preview_ribbon, LayoutParams(MATCH_PARENT, 32));

    // 3b. Category Overview ListView
    m_category_list = ListViewBuilder::create()
        ->itemHeight(40)
        ->spacing(4)
        ->itemCornerRadius(inner_rad)
        ->scrollbar(false)
        ->build();
    m_category_list->set_focusable(false);
    m_category_list->set_draw_focus_ring(false);
    m_category_list->set_selectable(false);
    m_category_list->set_keyboard_navigation_enabled(false);
    m_category_list->set_margin(10, 0, 10, 0);
    m_category_list->set_on_item_click_listener([this](size_t idx, std::shared_ptr<View>) {
        if (!check_and_update_click_debounce()) return;
        auto item = get_current_selected_item();
        if (!item || !item->is_category) return;

        item->ensure_children();
        if (idx >= item->children.size()) return;

        if (!m_search_query.empty()) {
            m_search_query.clear();
            m_search_results.clear();
        }

        m_path.push_back(item);
        m_history_indices.push_back(m_current_index);
        m_current_index = static_cast<int>(idx);
        m_preview_focused = false;
        if (m_window) {
            m_window->clear_focus();
        }
        rebuild_columns();
        notify_state_changed();
    });
    m_preview_pillar->add_view(m_category_list, LayoutParams(MATCH_PARENT, MATCH_PARENT, 1.0f));

    // 3c. Scrollable Interactive Cards Container
    m_preview_layout = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(10)
        ->padding(14, 4, 14, 14)
        ->backgroundColor(Color::transparent())
        ->build();

    m_preview_scroll = ScrollViewBuilder::create()
        ->contentView(m_preview_layout)
        ->scrollbar(true)
        ->build();

    m_preview_pillar->add_view(m_preview_scroll, LayoutParams(MATCH_PARENT, MATCH_PARENT, 1.0f));

    m_preview_frame = FrameLayoutBuilder::create()
        ->backgroundColor(Color::transparent())
        ->build();
    m_preview_frame->set_visibility(Visibility::Gone);
    m_preview_pillar->add_view(m_preview_frame, LayoutParams(MATCH_PARENT, MATCH_PARENT, 1.0f));

    add_view(m_preview_pillar, LayoutParams(MATCH_PARENT, MATCH_PARENT, 1.0f));

    // Startup drill: open whichever folder it was launched from (m_start_path or current directory)
    fs::path target_path;
    std::error_code ec;
    if (!m_start_path.empty()) {
        target_path = fs::canonical(m_start_path, ec);
        if (ec || !fs::exists(target_path, ec)) {
            target_path = fs::absolute(m_start_path, ec);
        }
    } else {
        target_path = fs::current_path(ec);
    }

    if (ec || !fs::exists(target_path, ec)) {
        const char* home = std::getenv("HOME");
        target_path = home ? fs::path(home) : fs::path("/");
    }

    // If target_path is a file, target its parent directory and record file name for selection
    std::string select_filename;
    if (fs::is_regular_file(target_path, ec)) {
        select_filename = target_path.filename().string();
        target_path = target_path.parent_path();
    }

    // Decompose target_path into segments relative to "/"
    // e.g. "/home/anisur/Downloads" -> ["home", "anisur", "Downloads"]
    std::vector<std::string> segments;
    for (fs::path p = target_path; p != p.root_path() && !p.empty(); p = p.parent_path()) {
        std::string fname = p.filename().string();
        if (!fname.empty() && fname != "/") {
            segments.push_back(fname);
        }
    }
    std::reverse(segments.begin(), segments.end());
    for (const auto& seg : segments) {
        if (!seg.empty() && seg[0] == '.') {
            m_show_hidden = true;
            break;
        }
    }
    if (!select_filename.empty() && select_filename[0] == '.') {
        m_show_hidden = true;
    }
    if (m_show_hidden) {
        m_root_tree = FilesModel::build_default_tree(m_start_path, m_show_hidden);
    }

    m_path.clear();
    m_history_indices.clear();
    m_current_index = 0;

    std::vector<std::shared_ptr<FileItem>>* current_level = &m_root_tree;

    for (const auto& seg : segments) {
        std::shared_ptr<FileItem> match_item = nullptr;
        int match_idx = -1;

        for (size_t i = 0; i < current_level->size(); ++i) {
            if ((*current_level)[i]->title == seg) {
                match_item = (*current_level)[i];
                match_idx = static_cast<int>(i);
                break;
            }
        }

        if (!match_item) {
            break;
        }

        match_item->ensure_children();
        m_path.push_back(match_item);
        m_history_indices.push_back(match_idx);
        current_level = &(match_item->children);
    }

    if (!select_filename.empty()) {
        for (size_t i = 0; i < current_level->size(); ++i) {
            if ((*current_level)[i]->title == select_filename) {
                m_current_index = static_cast<int>(i);
                break;
            }
        }
    } else {
        m_current_index = 0;
    }

    sync_theme();
    rebuild_columns();
}

void MillerView::sync_theme() {
    auto config = Config::get();
    if (config && m_preview_toggle_btn) {
        m_preview_toggle_btn->set_card_background_color(config->colors.surface_variant.with_alpha(0.35f));
    }
    m_displayed_parent_items.clear();
    m_displayed_current_items.clear();
    m_displayed_category_items.clear();
    update_preview_ribbon();
    update_pillar_styles();
    rebuild_parent_column();
    rebuild_current_column();
    rebuild_preview_column();
    request_redraw();
}

void MillerView::update_pillar_styles() {
    auto config = Config::get();
    if (!config) return;

    int radius = get_card_radius(config);
    Color active_border = config->colors.primary.with_alpha(0.55f);

    // 1. Column 1: Parent Pillar (Always transparent, zero border)
    if (m_parent_pillar) {
        m_parent_pillar->set_corner_radius(radius);
        m_parent_pillar->set_stroke(0, Color::transparent());
        m_parent_pillar->set_background_color(Color::transparent());
    }

    // 2. Column 2: Center Pillar (Subtle border ONLY when actively focused, transparent background)
    if (m_current_pillar) {
        m_current_pillar->set_corner_radius(radius);
        m_current_pillar->set_stroke(m_preview_focused ? 0 : 1, m_preview_focused ? Color::transparent() : active_border);
        m_current_pillar->set_background_color(Color::transparent());
    }

    // 3. Column 3: Right (Preview) Pillar Container (Subtle border ONLY when actively focused in 3-pillar mode)
    if (m_preview_pillar) {
        m_preview_pillar->set_corner_radius(radius);
        bool show_border = m_preview_focused && !m_preview_expanded;
        m_preview_pillar->set_stroke(show_border ? 1 : 0, show_border ? active_border : Color::transparent());
        m_preview_pillar->set_background_color(Color::transparent());
    }
}

void MillerView::update_preview_ribbon() {
    if (!m_preview_ribbon) return;

    auto config = Config::get();

    if (is_multi_selected()) {
        m_preview_ribbon->set_visibility(Visibility::Visible);
        if (m_preview_title) {
            m_preview_title->set_text(std::to_string(get_selected_count()) + " items selected");
            if (config) {
                m_preview_title->set_text_color(config->colors.primary);
            }
        }
        if (m_preview_actions) {
            m_preview_actions->clear_views();
            m_preview_actions->set_visibility(Visibility::Gone);
        }
        if (m_preview_toggle_btn) {
            m_preview_toggle_btn->set_visibility(Visibility::Gone);
        }
        return;
    }

    if (m_preview_toggle_btn) {
        m_preview_toggle_btn->set_visibility(Visibility::Visible);
        m_preview_toggle_btn->set_tooltip(m_preview_expanded ? "Collapse to 3 pillars" : "Expand full width");
        m_preview_toggle_btn->set_tooltip_shortcut(m_preview_expanded ? "Esc" : "");
    }

    auto item = get_current_selected_item();

    // Only show ribbon for leaf items
    if (!item || item->is_category) {
        m_preview_ribbon->set_visibility(Visibility::Gone);
        if (m_preview_actions) {
            m_preview_actions->clear_views();
            m_preview_actions->set_visibility(Visibility::Gone);
        }
        return;
    }

    m_preview_ribbon->set_visibility(Visibility::Visible);

    if (m_preview_title) {
        m_preview_title->set_text(item ? item->title : "");
        if (config) {
            m_preview_title->set_text_color(config->colors.on_surface_variant);
        }
    }

    if (m_preview_toggle_icon) {
        m_preview_toggle_icon->set_text(m_preview_expanded ? "|›" : "|‹");
        if (config) {
            m_preview_toggle_icon->set_text_color(config->colors.primary);
        }
    }

    if (m_preview_actions) {
        m_preview_actions->clear_views();
        m_preview_actions->set_visibility(Visibility::Visible);

        // 0. In-place Edit / Save button for text files
        bool is_text = item && !item->is_dir &&
                       (item->is_text_file || (item->category == FileCategory::CodeText && item->size_bytes <= 1024 * 1024 * 2));
        if (is_text) {
            item->is_text_file = true;
            auto edit_btn = ButtonBuilder::create()
                ->icon(item->is_editing ? "document-save" : "document-edit")
                ->text(item->is_editing ? "Save" : "Edit")
                ->flat(!item->is_editing)
                ->padding(8, 2)
                ->cornerRadius(get_btn_radius(config))
                ->textSize(static_cast<int>(config->metrics.font_size * 0.9f))
                ->tooltip(item->is_editing ? "Save changes to file (Ctrl+S)" : "Edit file in-place")
                ->build();
            if (item->is_editing) {
                edit_btn->set_background_color(config->colors.primary);
                edit_btn->set_text_color(config->colors.on_primary);
            }
            edit_btn->set_focusable(true);
            edit_btn->set_on_click_listener([this, item]() {
                if (!item) return;
                if (!item->is_editing) {
                    item->is_editing = true;
                    item->last_action_message.clear();
                    update_preview_ribbon();
                    rebuild_preview_column();
                    notify_state_changed();
                } else {
                    save_editing();
                }
            });
            m_preview_actions->add_view(edit_btn, LayoutParams(WRAP_CONTENT, 26));
        }

        auto open_btn = ButtonBuilder::create()
            ->icon("media-playback-start")
            ->text("Open")
            ->flat(true)
            ->padding(8, 2)
            ->cornerRadius(get_btn_radius(config))
            ->textSize(static_cast<int>(config->metrics.font_size * 0.9f))
            ->build();
        open_btn->set_focusable(true);
        open_btn->set_on_click_listener([item]() {
            if (!item || item->path.empty()) return;
            std::string cmd = "xdg-open \"" + item->path.string() + "\" &";
            (void)std::system(cmd.c_str());
        });
        m_preview_actions->add_view(open_btn, LayoutParams(WRAP_CONTENT, 26));

        auto copy_btn = ButtonBuilder::create()
            ->icon("edit-copy")
            ->text("Copy Path")
            ->flat(true)
            ->padding(8, 2)
            ->cornerRadius(get_btn_radius(config))
            ->textSize(static_cast<int>(config->metrics.font_size * 0.9f))
            ->build();
        copy_btn->set_focusable(true);
        copy_btn->set_on_click_listener([item]() {
            if (!item || item->path.empty()) return;
            std::string p = item->path.string();
            std::string cmd = "printf '%s' " + p + " | wl-copy 2>/dev/null || true";
            (void)std::system(cmd.c_str());
        });
        m_preview_actions->add_view(copy_btn, LayoutParams(WRAP_CONTENT, 26));

        auto term_btn = ButtonBuilder::create()
            ->icon("utilities-terminal")
            ->text("Terminal")
            ->flat(true)
            ->padding(8, 2)
            ->cornerRadius(get_btn_radius(config))
            ->textSize(static_cast<int>(config->metrics.font_size * 0.9f))
            ->build();
        term_btn->set_focusable(true);
        term_btn->set_on_click_listener([item]() {
            if (!item) return;
            fs::path target_dir = item->is_dir ? item->path : item->path.parent_path();
            std::string cmd = "alacritty --working-directory \"" + target_dir.string() + "\" &";
            (void)std::system(cmd.c_str());
        });
        m_preview_actions->add_view(term_btn, LayoutParams(WRAP_CONTENT, 26));

        auto info_btn = ButtonBuilder::create()
            ->icon("dialog-information")
            ->text("Info")
            ->flat(true)
            ->padding(8, 2)
            ->cornerRadius(get_btn_radius(config))
            ->textSize(static_cast<int>(config->metrics.font_size * 0.9f))
            ->tooltip("File Details")
            ->build();
        info_btn->set_focusable(true);
        info_btn->set_on_click_listener([this, item, config, info_btn]() {
            if (!item) return;
            if (m_active_popup && m_active_popup->is_showing()) {
                m_active_popup->dismiss();
                m_active_popup = nullptr;
                return;
            }
            show_file_info_popup(item, info_btn.get(), config);
        });
        m_preview_actions->add_view(info_btn, LayoutParams(WRAP_CONTENT, 26));
    }
}

bool MillerView::on_key(const KeyPressEvent& event) {
    if (event.pressed && event.has_ctrl() && (event.keysym == XKB_KEY_s || event.keysym == XKB_KEY_S)) {
        auto item = get_current_selected_item();
        if (item && item->is_editing) {
            save_editing();
            return true;
        }
    }
    return false;
}

void MillerView::set_preview_focused(bool focused) {
    if (m_preview_focused == focused) return;
    m_preview_focused = focused;
    if (!m_preview_focused) {
        if (m_window) {
            m_window->clear_focus();
        }
    }
    update_pillar_styles();
    notify_state_changed();
}

void MillerView::set_preview_expanded(bool expanded) {
    if (m_preview_expanded == expanded) return;
    m_preview_expanded = expanded;

    if (m_parent_pillar) {
        m_parent_pillar->set_visibility(m_preview_expanded ? Visibility::Gone : Visibility::Visible);
    }
    if (m_current_pillar) {
        m_current_pillar->set_visibility(m_preview_expanded ? Visibility::Gone : Visibility::Visible);
    }

    set_preview_focused(true);

    update_preview_ribbon();
    update_pillar_styles();
    request_redraw();
    notify_state_changed();
}

bool MillerView::on_mouse_move(int lx, int ly, const Rect& bounds) {
    m_last_mouse_x = lx;
    m_last_mouse_y = ly;
    return LinearLayout::on_mouse_move(lx, ly, bounds);
}

bool MillerView::on_mouse_button(int lx, int ly, MouseButton button, bool pressed, const Rect& bounds) {
    if (!is_visible() || !is_enabled()) return false;

    m_last_mouse_x = lx;
    m_last_mouse_y = ly;

    if (button == MouseButton::Left && pressed) {
        bool in_preview_form = m_preview_pillar && m_preview_pillar->get_bounds().contains(Point(lx, ly))
                               && m_preview_scroll && m_preview_scroll->is_visible();
        if (in_preview_form != m_preview_focused) {
            set_preview_focused(in_preview_form);
        }
    }

    return LinearLayout::on_mouse_button(lx, ly, button, pressed, bounds);
}

bool MillerView::on_scroll(double delta) {
    if (!is_visible() || !is_enabled()) return false;
    Point pt(m_last_mouse_x, m_last_mouse_y);

    if (m_preview_pillar && m_preview_pillar->get_bounds().contains(pt)) {
        if (m_category_list && m_category_list->is_visible()) return m_category_list->on_scroll(delta);
        if (m_preview_scroll && m_preview_scroll->is_visible()) return m_preview_scroll->on_scroll(delta);
        return false;
    }

    if (m_current_pillar && m_current_pillar->get_bounds().contains(pt)) {
        return m_current_list && m_current_list->is_visible() && m_current_list->on_scroll(delta);
    }
    if (m_parent_pillar && m_parent_pillar->get_bounds().contains(pt)) {
        return m_parent_list && m_parent_list->is_visible() && m_parent_list->on_scroll(delta);
    }

    return LinearLayout::on_scroll(delta);
}

const std::vector<std::shared_ptr<FileItem>>& MillerView::get_current_list() const {
    if (!m_search_query.empty()) {
        return m_search_results;
    }
    if (m_path.empty()) {
        return m_root_tree;
    }
    m_path.back()->ensure_children();
    return m_path.back()->children;
}

std::shared_ptr<FileItem> MillerView::get_current_selected_item() const {
    const auto& list = get_current_list();
    if (list.empty()) return nullptr;
    int idx = std::clamp(m_current_index, 0, static_cast<int>(list.size()) - 1);
    return list[idx];
}

bool MillerView::is_multi_selected() const {
    return m_current_list && !m_current_list->get_selected_indices().empty();
}

size_t MillerView::get_selected_count() const {
    if (!m_current_list) return 0;
    return m_current_list->get_selected_count();
}

std::vector<std::shared_ptr<FileItem>> MillerView::get_selected_items() const {
    std::vector<std::shared_ptr<FileItem>> result;
    if (!m_current_list) return result;
    const auto& list = get_current_list();
    auto model = m_current_list->get_selection_model();
    if (!model) return result;

    auto indices = model->effective_indices();
    result.reserve(indices.size());
    for (int idx : indices) {
        if (idx >= 0 && idx < static_cast<int>(list.size())) {
            result.push_back(list[idx]);
        }
    }
    return result;
}

bool MillerView::handle_list_key(const KeyPressEvent& event) {
    if (m_preview_focused || m_preview_expanded) return false;
    if (!m_current_list || !m_current_list->is_visible()) return false;
    // Enter / Return is handled by the window/app level (navigate_enter)
    if (event.keysym == XKB_KEY_Return || event.keysym == XKB_KEY_KP_Enter) return false;

    if (m_current_list->on_key(event)) {
        m_current_index = m_current_list->get_selected_index();
        rebuild_preview_column();
        notify_state_changed();
        return true;
    }

    return false;
}

void MillerView::notify_state_changed() {
    update_pillar_styles();
    if (m_on_state_changed) {
        m_on_state_changed();
    }
    request_redraw();
}

void MillerView::rebuild_columns() {
    update_pillar_styles();
    rebuild_parent_column();
    rebuild_current_column();
    rebuild_preview_column();
    request_redraw();
}

std::shared_ptr<View> MillerView::create_parent_hero_box(const std::shared_ptr<Config>& config) {
    auto hero_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->gravity(Gravity::Center)
        ->padding(16, 24)
        ->spacing(10)
        ->build();

    auto hero_icon = ImageViewBuilder::create()
        ->source("system-file-manager")
        ->targetSize(48)
        ->fitMode(FitMode::Contain)
        ->alpha(0.85f)
        ->build();
    hero_box->add_view(hero_icon, LayoutParams(48, 48, Gravity::CenterHorizontal));

    auto hero_title = TextViewBuilder::create()
        ->text("Files")
        ->bold(true)
        ->center(true)
        ->build();
    hero_box->add_view(hero_title);

    auto hero_hint = TextViewBuilder::create()
        ->text("↑/↓ Navigate\nEnter Open\nEsc Back")
        ->caption(true)
        ->muted()
        ->center(true)
        ->build();
    hero_box->add_view(hero_hint);

    return hero_box;
}

std::shared_ptr<View> MillerView::create_empty_search_box(const std::shared_ptr<Config>& config) {
    auto empty_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->gravity(Gravity::Center)
        ->padding(16, 32)
        ->spacing(8)
        ->build();

    auto empty_icon = ImageViewBuilder::create()
        ->source("system-search")
        ->targetSize(40)
        ->fitMode(FitMode::Contain)
        ->alpha(0.5f)
        ->build();
    empty_box->add_view(empty_icon, LayoutParams(40, 40, Gravity::CenterHorizontal));

    auto empty_text = TextViewBuilder::create()
        ->text("No files found")
        ->build();
    empty_box->add_view(empty_text);

    auto empty_hint = TextViewBuilder::create()
        ->text("No matches for \"" + m_search_query + "\"")
        ->caption(true)
        ->muted()
        ->build();
    empty_box->add_view(empty_hint);

    return empty_box;
}

std::shared_ptr<View> MillerView::create_list_row_item(const std::shared_ptr<FileItem>& item,
                                                        const std::shared_ptr<Config>& config) {
    auto item_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(12, 0)
        ->spacing(10)
        ->gravity(Gravity::CenterVertical)
        ->backgroundColor(Color::transparent())
        ->build();

    auto icon_view = ImageViewBuilder::create()
        ->source(item->icon.empty() ? (item->is_category ? "folder" : "text-x-generic") : item->icon)
        ->targetSize(20)
        ->fitMode(FitMode::Contain)
        ->alpha(item->is_hidden ? 0.45f : 1.0f)
        ->build();
    item_row->add_view(icon_view, LayoutParams(20, 20, Gravity::CenterVertical));

    auto title_builder = TextViewBuilder::create()
        ->text(item->title)
        ->ellipsize(true);
    if (item->is_hidden) {
        title_builder->muted();
    }
    auto title_view = title_builder->build();
    item_row->add_view(title_view, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));

    if (item->is_category) {
        auto chevron = TextViewBuilder::create()
            ->text("›")
            ->muted()
            ->build();
        item_row->add_view(chevron, LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity::CenterVertical));
    } else if (!item->size_formatted.empty()) {
        auto size_view = TextViewBuilder::create()
            ->text(item->size_formatted)
            ->caption(true)
            ->muted()
            ->build();
        item_row->add_view(size_view, LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity::CenterVertical));
    }

    return item_row;
}

void MillerView::rebuild_parent_column() {
    auto config = Config::get();

    if (m_path.empty() && m_search_query.empty()) {
        m_displayed_parent_items.clear();
        if (m_parent_hero) m_parent_hero->set_visibility(Visibility::Visible);
        if (m_parent_list) {
            m_parent_list->clear_items();
            m_parent_list->set_visibility(Visibility::Gone);
        }
        return;
    }

    if (m_parent_hero) m_parent_hero->set_visibility(Visibility::Gone);
    if (m_parent_list) {
        m_parent_list->set_visibility(Visibility::Visible);

        if (m_path.size() >= 2) {
            m_path[m_path.size() - 2]->ensure_children();
        }
        const auto* parent_siblings = (!m_search_query.empty() || m_path.size() == 1)
            ? &m_root_tree
            : &(m_path[m_path.size() - 2]->children);

        auto active_parent = (!m_search_query.empty() || m_path.empty()) ? nullptr : m_path.back();
        int active_idx = -1;

        for (size_t i = 0; i < parent_siblings->size(); ++i) {
            if ((*parent_siblings)[i] == active_parent) {
                active_idx = static_cast<int>(i);
                break;
            }
        }

        bool items_match = (m_displayed_parent_items.size() == parent_siblings->size());
        if (items_match) {
            for (size_t i = 0; i < parent_siblings->size(); ++i) {
                if (m_displayed_parent_items[i] != (*parent_siblings)[i]) {
                    items_match = false;
                    break;
                }
            }
        }

        if (items_match) {
            if (m_parent_list->get_selected_index() != active_idx) {
                m_parent_list->set_selected_index(active_idx);
            }
        } else {
            m_displayed_parent_items = *parent_siblings;
            m_parent_list->clear_items();
            for (size_t i = 0; i < parent_siblings->size(); ++i) {
                const auto& item = (*parent_siblings)[i];
                m_parent_list->add_item(create_list_row_item(item, config));
            }
            m_parent_list->set_selected_index(active_idx);
        }
    }
}

void MillerView::rebuild_current_column() {
    auto config = Config::get();
    const auto& list = get_current_list();

    if (list.empty()) {
        m_displayed_current_items.clear();
        if (m_current_list) {
            m_current_list->clear_items();
            m_current_list->set_visibility(Visibility::Gone);
        }
        if (m_empty_search_box && !m_search_query.empty()) {
            m_empty_search_box->set_visibility(Visibility::Visible);
        }
        return;
    }

    if (m_empty_search_box) m_empty_search_box->set_visibility(Visibility::Gone);
    if (m_current_list) {
        m_current_list->set_visibility(Visibility::Visible);

        m_current_index = std::clamp(m_current_index, 0, static_cast<int>(list.size()) - 1);

        bool items_match = (m_displayed_current_items.size() == list.size());
        if (items_match) {
            for (size_t i = 0; i < list.size(); ++i) {
                if (m_displayed_current_items[i] != list[i]) {
                    items_match = false;
                    break;
                }
            }
        }

        if (items_match) {
            if (m_current_list->get_selected_index() != m_current_index) {
                m_current_list->set_selected_index(m_current_index);
            }
        } else {
            m_displayed_current_items = list;
            m_current_list->clear_items();
            for (size_t i = 0; i < list.size(); ++i) {
                const auto& item = list[i];
                m_current_list->add_item(create_list_row_item(item, config));
            }
            m_current_list->set_selected_index(m_current_index);
        }
    }
}

void MillerView::add_preview_card(std::shared_ptr<View> card) {
    if (!card) return;
    LayoutParams lp = card->get_layout_params();
    lp.width = static_cast<int>(LayoutDimension::MatchParent);
    if (lp.height <= 0 && lp.height != static_cast<int>(LayoutDimension::MatchParent)) {
        lp.height = static_cast<int>(LayoutDimension::WrapContent);
    }
    m_preview_layout->add_view(std::move(card), lp);
}

void MillerView::deferred_rebuild_preview(int delay_ms) {
    if (auto* eng = AppEngine::instance()) {
        eng->post_delayed(delay_ms, [this]() {
            rebuild_preview_column();
            notify_state_changed();
        });
    } else {
        rebuild_preview_column();
        notify_state_changed();
    }
}

void MillerView::rebuild_preview_column() {
    int focused_index = -1;
    if (m_window) {
        View* current_focus = m_window->get_focused_view();
        if (current_focus) {
            std::vector<View*> old_focusables;
            collect_preview_focusable_views(old_focusables);
            for (size_t i = 0; i < old_focusables.size(); ++i) {
                if (old_focusables[i] == current_focus) {
                    focused_index = static_cast<int>(i);
                    break;
                }
            }
        }
    }

    if (m_preview_layout) m_preview_layout->clear_views();
    if (m_preview_frame) m_preview_frame->clear_views();

    if (m_active_popup && m_active_popup->is_showing()) {
        m_active_popup->dismiss();
        m_active_popup = nullptr;
    }

    auto config = Config::get();
    auto item = get_current_selected_item();

    update_preview_ribbon();

    if (is_multi_selected()) {
        m_displayed_category_items.clear();
        if (m_category_list) m_category_list->set_visibility(Visibility::Gone);
        if (m_preview_frame) m_preview_frame->set_visibility(Visibility::Gone);
        if (m_preview_scroll) m_preview_scroll->set_visibility(Visibility::Visible);
        m_preview_layout->set_padding(Padding(14, 4, 14, 14));
        m_preview_layout->set_divider_spacing(10);
        render_multi_selection_preview(get_selected_items(), config);
        return;
    }

    if (!item) {
        m_displayed_category_items.clear();
        if (m_category_list) m_category_list->set_visibility(Visibility::Gone);
        if (m_preview_frame) m_preview_frame->set_visibility(Visibility::Gone);
        if (m_preview_scroll) m_preview_scroll->set_visibility(Visibility::Visible);
        render_empty_preview(config);
        return;
    }

    if (item->is_category) {
        if (m_preview_frame) m_preview_frame->set_visibility(Visibility::Gone);
        render_category_children_preview(item, config);
        return;
    }

    m_displayed_category_items.clear();

    // Leaf file preview
    if (item->category == FileCategory::Image) {
        if (m_category_list) m_category_list->set_visibility(Visibility::Gone);
        if (m_preview_scroll) m_preview_scroll->set_visibility(Visibility::Gone);
        if (m_preview_frame) m_preview_frame->set_visibility(Visibility::Visible);
        render_photo_preview(item, config);
    } else {
        if (m_category_list) m_category_list->set_visibility(Visibility::Gone);
        if (m_preview_frame) m_preview_frame->set_visibility(Visibility::Gone);
        if (m_preview_scroll) m_preview_scroll->set_visibility(Visibility::Visible);
        m_preview_layout->set_padding(Padding(14, 4, 14, 14));
        m_preview_layout->set_divider_spacing(10);
        render_file_preview(item, config);
    }

    // Preserve focus across live updates
    if (m_window) {
        if (m_preview_focused || m_preview_expanded) {
            std::vector<View*> new_focusables;
            collect_preview_focusable_views(new_focusables);
            if (focused_index >= 0) {
                if (focused_index < static_cast<int>(new_focusables.size())) {
                    m_window->set_focused_view(new_focusables[focused_index]);
                } else if (!new_focusables.empty()) {
                    m_window->set_focused_view(new_focusables.front());
                }
            } else if (!new_focusables.empty() && m_window->get_focused_view() == nullptr) {
                m_window->set_focused_view(new_focusables.front());
            }
        } else {
            View* cur = m_window->get_focused_view();
            if (cur) {
                std::vector<View*> preview_views;
                collect_preview_focusable_views(preview_views);
                for (auto* pv : preview_views) {
                    if (pv == cur) {
                        m_window->clear_focus();
                        break;
                    }
                }
            }
        }
    }
}

bool MillerView::navigate_up() {
    if (m_preview_expanded) return false;
    if (m_preview_focused) {
        return focus_first_preview_control();
    }
    const auto& list = get_current_list();
    if (list.empty()) return false;

    if (m_current_index > 0) {
        m_current_index--;
        if (m_current_list) {
            m_current_list->set_selected_index(m_current_index);
        }
        rebuild_preview_column();
        notify_state_changed();
        return true;
    }
    return false;
}

bool MillerView::navigate_down() {
    if (m_preview_expanded) return false;
    if (m_preview_focused) {
        return focus_first_preview_control();
    }
    const auto& list = get_current_list();
    if (list.empty()) return false;

    if (m_current_index < static_cast<int>(list.size()) - 1) {
        m_current_index++;
        if (m_current_list) {
            m_current_list->set_selected_index(m_current_index);
        }
        rebuild_preview_column();
        notify_state_changed();
        return true;
    }
    return false;
}

bool MillerView::navigate_enter() {
    if (m_preview_expanded) return false;
    if (m_preview_focused) {
        return focus_first_preview_control();
    }
    auto item = get_current_selected_item();
    if (!item) return false;

    auto now = std::chrono::steady_clock::now();
    uint64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    constexpr uint64_t NAV_DEBOUNCE_MS = 150;
    if (m_last_navigate_enter_time_ms > 0 && (now_ms - m_last_navigate_enter_time_ms) < NAV_DEBOUNCE_MS) {
        return false;
    }
    m_last_navigate_enter_time_ms = now_ms;

    if (item->is_category) {
        if (!m_search_query.empty()) {
            m_search_query.clear();
            m_search_results.clear();
        }
        m_path.push_back(item);
        m_history_indices.push_back(m_current_index);
        m_current_index = 0;
        m_preview_focused = false;
        if (m_window) {
            m_window->clear_focus();
        }
        rebuild_columns();
        notify_state_changed();
        return true;
    } else {
        return focus_first_preview_control();
    }
}

void MillerView::set_search_query(const std::string& query) {
    m_search_query = query;
    m_search_results.clear();
    m_current_index = 0;
    if (m_preview_expanded) {
        set_preview_expanded(false);
    }
    m_preview_focused = false;

    if (!m_search_query.empty()) {
        std::string lower_query = m_search_query;
        std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        const auto* pool = m_path.empty() ? &m_root_tree : &(m_path.back()->children);
        for (const auto& item : *pool) {
            std::string t = item->title;
            std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c) { return std::tolower(c); });
            std::string sub = item->subtitle;
            std::transform(sub.begin(), sub.end(), sub.begin(), [](unsigned char c) { return std::tolower(c); });

            if (t.find(lower_query) != std::string::npos ||
                sub.find(lower_query) != std::string::npos ||
                item->id.find(lower_query) != std::string::npos) {
                m_search_results.push_back(item);
            }
        }
    }

    rebuild_columns();
    notify_state_changed();
}

bool MillerView::navigate_back() {
    auto now = std::chrono::steady_clock::now();
    uint64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    constexpr uint64_t NAV_DEBOUNCE_MS = 150;
    if (m_last_navigate_back_time_ms > 0 && (now_ms - m_last_navigate_back_time_ms) < NAV_DEBOUNCE_MS) {
        return false;
    }
    m_last_navigate_back_time_ms = now_ms;

    if (m_window) {
        m_window->clear_focus();
    }
    if (!m_search_query.empty()) {
        m_search_query.clear();
        m_search_results.clear();
        m_current_index = 0;
        if (m_preview_expanded) {
            set_preview_expanded(false);
        }
        m_preview_focused = false;
        rebuild_columns();
        notify_state_changed();
        return true;
    }

    if (m_preview_expanded) {
        set_preview_expanded(false);
        return true;
    }

    if (!m_path.empty()) {
        m_path.pop_back();
        if (!m_history_indices.empty()) {
            m_current_index = m_history_indices.back();
            m_history_indices.pop_back();
        } else {
            m_current_index = 0;
        }
        m_preview_focused = false;
        m_preview_expanded = false;
        rebuild_columns();
        notify_state_changed();
        return true;
    }

    if (m_preview_focused) {
        set_preview_focused(false);
        return true;
    }

    return false;
}

bool MillerView::navigate_right() {
    return navigate_enter();
}

bool MillerView::navigate_left() {
    if (m_preview_expanded) return false;
    return navigate_back();
}

void MillerView::navigate_to_depth(size_t target_depth) {
    if (m_window) {
        m_window->clear_focus();
    }
    if (!m_search_query.empty()) {
        m_search_query.clear();
        m_search_results.clear();
    }
    if (m_preview_expanded) {
        set_preview_expanded(false);
    }
    m_preview_focused = false;

    if (target_depth >= m_path.size()) {
        rebuild_columns();
        notify_state_changed();
        return;
    }

    while (m_path.size() > target_depth) {
        m_path.pop_back();
        if (!m_history_indices.empty()) {
            m_current_index = m_history_indices.back();
            m_history_indices.pop_back();
        } else {
            m_current_index = 0;
        }
    }

    rebuild_columns();
    notify_state_changed();
}

void MillerView::collect_preview_focusable_views(std::vector<View*>& out) {
    if (m_preview_actions && m_preview_actions->is_visible()) {
        m_preview_actions->collect_focusable_views(out);
    }
    if (m_preview_toggle_btn && m_preview_toggle_btn->is_visible() && m_preview_toggle_btn->is_focusable()) {
        out.push_back(m_preview_toggle_btn.get());
    }
    if (m_preview_frame && m_preview_frame->is_visible()) {
        m_preview_frame->collect_focusable_views(out);
    } else if (m_preview_layout) {
        m_preview_layout->collect_focusable_views(out);
    }
}

bool MillerView::focus_first_preview_control() {
    if (!m_window) return false;
    std::vector<View*> focusables;
    collect_preview_focusable_views(focusables);
    if (focusables.empty()) return false;
    set_preview_focused(true);
    m_window->set_focused_view(focusables.front());
    return true;
}

bool MillerView::focus_next_preview_control() {
    if (!m_window) return false;
    std::vector<View*> focusables;
    collect_preview_focusable_views(focusables);
    if (focusables.empty()) return false;

    set_preview_focused(true);
    View* cur = m_window->get_focused_view();
    auto it = std::find(focusables.begin(), focusables.end(), cur);
    if (it == focusables.end() || it + 1 == focusables.end()) {
        m_window->set_focused_view(focusables.front());
    } else {
        m_window->set_focused_view(*(it + 1));
    }
    return true;
}

bool MillerView::focus_prev_preview_control() {
    if (!m_window) return false;
    std::vector<View*> focusables;
    collect_preview_focusable_views(focusables);
    if (focusables.empty()) return false;

    set_preview_focused(true);
    View* cur = m_window->get_focused_view();
    auto it = std::find(focusables.begin(), focusables.end(), cur);
    if (it == focusables.end() || it == focusables.begin()) {
        m_window->set_focused_view(focusables.back());
    } else {
        m_window->set_focused_view(*(it - 1));
    }
    return true;
}

bool MillerView::is_editor_focused() const {
    if (!m_window) return false;
    View* cur = m_window->get_focused_view();
    if (auto* et = dynamic_cast<EditText*>(cur)) {
        return et->is_editable();
    }
    auto item = get_current_selected_item();
    return item && item->is_editing;
}

void MillerView::save_editing() {
    auto item = get_current_selected_item();
    if (item && item->is_editing) {
        std::ofstream ofs(item->path);
        if (ofs.is_open()) {
            ofs << item->text_content;
            ofs.flush();
            ofs.close();
            std::error_code ec;
            item->size_bytes = fs::file_size(item->path, ec);
            item->size_formatted = FilesModel::format_file_size(item->size_bytes);
            item->subtitle = item->size_formatted + " • " + item->category_label;
            item->last_action_message = "Saved";
        } else {
            item->last_action_message = "Error saving";
        }
        item->is_editing = false;
        update_preview_ribbon();
        rebuild_preview_column();
        notify_state_changed();
    }
}

void MillerView::cancel_editing() {
    auto item = get_current_selected_item();
    if (item && item->is_editing) {
        item->is_editing = false;
        update_preview_ribbon();
        rebuild_preview_column();
        notify_state_changed();
    }
}

bool MillerView::is_preview_content_focused() const {
    if (!m_window) return false;
    View* cur = m_window->get_focused_view();
    if (!cur) return false;
    if (m_preview_layout) {
        if (cur == m_preview_layout.get()) return true;
        ViewGroup* p = cur->get_parent();
        while (p) {
            if (p == m_preview_layout.get()) return true;
            p = p->get_parent();
        }
    }
    if (m_preview_frame) {
        if (cur == m_preview_frame.get()) return true;
        ViewGroup* p = cur->get_parent();
        while (p) {
            if (p == m_preview_frame.get()) return true;
            p = p->get_parent();
        }
    }
    return false;
}

bool MillerView::is_preview_ribbon_focused() const {
    if (!m_window || !m_preview_ribbon) return false;
    View* cur = m_window->get_focused_view();
    if (!cur) return false;
    if (cur == m_preview_ribbon.get()) return true;
    ViewGroup* p = cur->get_parent();
    while (p) {
        if (p == m_preview_ribbon.get()) return true;
        p = p->get_parent();
    }
    return false;
}

bool MillerView::focus_preview_ribbon() {
    if (!m_window) return false;
    std::vector<View*> ribbon_focusables;
    if (m_preview_actions && m_preview_actions->is_visible()) {
        m_preview_actions->collect_focusable_views(ribbon_focusables);
    }
    if (m_preview_toggle_btn && m_preview_toggle_btn->is_visible() && m_preview_toggle_btn->is_focusable()) {
        ribbon_focusables.push_back(m_preview_toggle_btn.get());
    }
    if (ribbon_focusables.empty()) return false;
    set_preview_focused(true);
    m_window->set_focused_view(ribbon_focusables.front());
    return true;
}

bool MillerView::focus_prev_ribbon_control() {
    if (!m_window) return false;
    std::vector<View*> ribbon_focusables;
    if (m_preview_actions && m_preview_actions->is_visible()) {
        m_preview_actions->collect_focusable_views(ribbon_focusables);
    }
    if (m_preview_toggle_btn && m_preview_toggle_btn->is_visible() && m_preview_toggle_btn->is_focusable()) {
        ribbon_focusables.push_back(m_preview_toggle_btn.get());
    }
    if (ribbon_focusables.empty()) return false;

    View* cur = m_window->get_focused_view();
    auto it = std::find(ribbon_focusables.begin(), ribbon_focusables.end(), cur);
    if (it != ribbon_focusables.end()) {
        if (it != ribbon_focusables.begin()) {
            m_window->set_focused_view(*(it - 1));
            return true;
        }
        return false;
    }
    m_window->set_focused_view(ribbon_focusables.back());
    return true;
}

bool MillerView::focus_next_ribbon_control() {
    if (!m_window) return false;
    std::vector<View*> ribbon_focusables;
    if (m_preview_actions && m_preview_actions->is_visible()) {
        m_preview_actions->collect_focusable_views(ribbon_focusables);
    }
    if (m_preview_toggle_btn && m_preview_toggle_btn->is_visible() && m_preview_toggle_btn->is_focusable()) {
        ribbon_focusables.push_back(m_preview_toggle_btn.get());
    }
    if (ribbon_focusables.empty()) return false;

    View* cur = m_window->get_focused_view();
    auto it = std::find(ribbon_focusables.begin(), ribbon_focusables.end(), cur);
    if (it != ribbon_focusables.end()) {
        if (it + 1 != ribbon_focusables.end()) {
            m_window->set_focused_view(*(it + 1));
        }
        return true;
    }
    m_window->set_focused_view(ribbon_focusables.front());
    return true;
}

bool MillerView::focus_preview_content() {
    if (!m_window) return false;
    std::vector<View*> content_focusables;
    if (m_preview_layout && m_preview_layout->is_visible()) {
        m_preview_layout->collect_focusable_views(content_focusables);
    } else if (m_preview_frame && m_preview_frame->is_visible()) {
        m_preview_frame->collect_focusable_views(content_focusables);
    }
    if (content_focusables.empty()) return false;
    set_preview_focused(true);
    m_window->set_focused_view(content_focusables.front());
    return true;
}



bool MillerView::scroll_preview(double delta) {
    if (m_preview_scroll && m_preview_scroll->is_visible()) {
        return m_preview_scroll->on_scroll(delta);
    }
    return false;
}

bool MillerView::is_preview_scroll_at_top() const {
    if (m_preview_scroll && m_preview_scroll->is_visible()) {
        return m_preview_scroll->get_scroll_y() <= 0.0;
    }
    return true;
}

bool MillerView::trigger_active_action() {
    auto item = get_current_selected_item();
    if (item && item->has_action) {
        item->last_action_message = "Opened";
        rebuild_preview_column();
        notify_state_changed();
        return true;
    }
    return false;
}

std::string MillerView::get_breadcrumb_path() const {
    if (!m_search_query.empty()) {
        std::string path = "/ › Search: \"" + m_search_query + "\"";
        auto current = get_current_selected_item();
        if (current) {
            path += " › " + current->title;
        }
        return path;
    }
    std::string path = "/";
    for (const auto& item : m_path) {
        path += " › " + item->title;
    }
    auto current = get_current_selected_item();
    if (current) {
        path += " › " + current->title;
    }
    return path;
}

} // namespace miqu
