#include "miller_view.hpp"
#include "preview_common.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
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

void MillerView::set_client(std::shared_ptr<miqumusic::MpdClient> client) {
    m_client = std::move(client);
    if (m_preview_layout) {
        refresh_model();
    }
}

void MillerView::refresh_model() {
    m_root_tree = MusicModel::build_default_tree(m_client);
    if (m_preview_layout) {
        rebuild_columns();
        notify_state_changed();
    }
}

void MillerView::execute_sibling_swap(const std::shared_ptr<MusicItem>& item, size_t index) {
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

void MillerView::init() {
    m_root_tree = MusicModel::build_default_tree(m_client);

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
        ->build();
    m_current_list->set_keyboard_navigation_enabled(false);
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
            rebuild_columns();
            notify_state_changed();
        } else {
            auto item = get_current_selected_item();
            if (item) {
                if (item->is_category) {
                    navigate_enter();
                } else {
                    focus_first_preview_control();
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
        ->addView(icon_box)
        ->build();
    m_preview_toggle_btn->set_on_click_listener([this]() {
        if (!check_and_update_click_debounce()) return;
        set_preview_expanded(!m_preview_expanded);
    });
    m_preview_toggle_btn->set_focusable(false);

    m_preview_title = TextViewBuilder::create()
        ->text("")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface_variant)
        ->ellipsize(true)
        ->build();
    m_preview_ribbon->add_view(m_preview_title, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));
    m_preview_ribbon->add_view(m_preview_toggle_btn, LayoutParams(WRAP_CONTENT, 26, Gravity::CenterVertical));
    m_preview_pillar->add_view(m_preview_ribbon, LayoutParams(MATCH_PARENT, 30));

    // 3b. Category Overview ListView
    m_category_list = ListViewBuilder::create()
        ->itemHeight(40)
        ->spacing(4)
        ->itemCornerRadius(inner_rad)
        ->scrollbar(false)
        ->build();
    m_category_list->set_selectable(false);
    m_category_list->set_keyboard_navigation_enabled(false);
    m_category_list->set_margin(10, 0, 10, 0);
    m_category_list->set_on_item_click_listener([this](size_t idx, std::shared_ptr<View>) {
        if (!check_and_update_click_debounce()) return;
        auto item = get_current_selected_item();
        if (!item || !item->is_category) return;

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
    add_view(m_preview_pillar, LayoutParams(MATCH_PARENT, MATCH_PARENT, 1.0f));

    sync_theme();
    rebuild_columns();
}

void MillerView::sync_theme() {
    auto config = Config::get();
    if (config && m_preview_toggle_btn) {
        m_preview_toggle_btn->set_card_background_color(config->colors.surface_variant.with_alpha(0.35f));
    }
    update_preview_ribbon();
    update_pillar_styles();
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
    auto item = get_current_selected_item();

    // Only show ribbon for leaf items
    if (!item || item->is_category) {
        m_preview_ribbon->set_visibility(Visibility::Gone);
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
}

bool MillerView::on_key(const KeyPressEvent& event) {
    (void)event;
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

const std::vector<std::shared_ptr<MusicItem>>& MillerView::get_current_list() const {
    if (!m_search_query.empty()) {
        return m_search_results;
    }
    if (m_path.empty()) {
        return m_root_tree;
    }
    m_path.back()->ensure_children();
    return m_path.back()->children;
}

std::shared_ptr<MusicItem> MillerView::get_current_selected_item() const {
    const auto& list = get_current_list();
    if (list.empty()) return nullptr;
    int idx = std::clamp(m_current_index, 0, static_cast<int>(list.size()) - 1);
    return list[idx];
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

    auto hero_icon = TextViewBuilder::create()
        ->text("󰎆")
        ->textSize(static_cast<int>(config->metrics.font_size * 2.8f))
        ->textColor(config->colors.primary.with_alpha(0.6f))
        ->center(true)
        ->build();
    hero_box->add_view(hero_icon);

    auto hero_title = TextViewBuilder::create()
        ->text("Music")
        ->textSize(static_cast<int>(config->metrics.font_size * 1.3f))
        ->bold(true)
        ->textColor(config->colors.on_surface)
        ->center(true)
        ->build();
    hero_box->add_view(hero_title);

    auto hero_hint = TextViewBuilder::create()
        ->text("↑/↓ Navigate\nEnter Select\nEsc Back")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.6f))
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

    auto empty_icon = TextViewBuilder::create()
        ->text("🔍")
        ->textSize(static_cast<int>(config->metrics.font_size * 2))
        ->textColor(config->colors.on_surface_variant.with_alpha(0.5f))
        ->build();
    empty_box->add_view(empty_icon);

    auto empty_text = TextViewBuilder::create()
        ->text("No songs found")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    empty_box->add_view(empty_text);

    auto empty_hint = TextViewBuilder::create()
        ->text("No matches for \"" + m_search_query + "\"")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.6f))
        ->build();
    empty_box->add_view(empty_hint);

    return empty_box;
}

std::shared_ptr<View> MillerView::create_list_row_item(const std::shared_ptr<MusicItem>& item,
                                                        const std::shared_ptr<Config>& config) {
    auto item_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(12, 0)
        ->spacing(10)
        ->gravity(Gravity::CenterVertical)
        ->backgroundColor(Color::transparent())
        ->build();

    auto icon_view = TextViewBuilder::create()
        ->text(item->icon.empty() ? "󰎆" : item->icon)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    item_row->add_view(icon_view, LayoutParams(24, WRAP_CONTENT, Gravity::CenterVertical));

    auto title_view = TextViewBuilder::create()
        ->text(item->title)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->ellipsize(true)
        ->build();
    item_row->add_view(title_view, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));

    return item_row;
}

void MillerView::rebuild_parent_column() {
    auto config = Config::get();

    if (m_path.empty() && m_search_query.empty()) {
        if (m_parent_hero) m_parent_hero->set_visibility(Visibility::Visible);
        if (m_parent_list) m_parent_list->set_visibility(Visibility::Gone);
        return;
    }

    if (m_parent_hero) m_parent_hero->set_visibility(Visibility::Gone);
    if (m_parent_list) {
        m_parent_list->set_visibility(Visibility::Visible);
        m_parent_list->clear_items();

        if (m_path.size() >= 2) {
            m_path[m_path.size() - 2]->ensure_children();
        }
        const auto* parent_siblings = (!m_search_query.empty() || m_path.size() == 1)
            ? &m_root_tree
            : &(m_path[m_path.size() - 2]->children);

        auto active_parent = (!m_search_query.empty() || m_path.empty()) ? nullptr : m_path.back();
        int active_idx = -1;

        for (size_t i = 0; i < parent_siblings->size(); ++i) {
            const auto& item = (*parent_siblings)[i];
            if (item == active_parent) {
                active_idx = static_cast<int>(i);
            }
            m_parent_list->add_item(create_list_row_item(item, config));
        }

        m_parent_list->set_selected_index(active_idx);
    }
}

void MillerView::rebuild_current_column() {
    auto config = Config::get();
    const auto& list = get_current_list();

    if (list.empty()) {
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
        m_current_list->clear_items();

        m_current_index = std::clamp(m_current_index, 0, static_cast<int>(list.size()) - 1);

        for (size_t i = 0; i < list.size(); ++i) {
            const auto& item = list[i];
            m_current_list->add_item(create_list_row_item(item, config));
        }

        m_current_list->set_selected_index(m_current_index);
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
    if (m_window && m_preview_layout) {
        View* current_focus = m_window->get_focused_view();
        if (current_focus) {
            std::vector<View*> old_focusables;
            m_preview_layout->collect_focusable_views(old_focusables);
            for (size_t i = 0; i < old_focusables.size(); ++i) {
                if (old_focusables[i] == current_focus) {
                    focused_index = static_cast<int>(i);
                    break;
                }
            }
        }
    }

    m_preview_layout->clear_views();

    auto config = Config::get();
    auto item = get_current_selected_item();

    update_preview_ribbon();

    if (!item) {
        if (m_category_list) m_category_list->set_visibility(Visibility::Gone);
        if (m_preview_scroll) m_preview_scroll->set_visibility(Visibility::Visible);
        render_empty_preview(config);
        return;
    }

    if (item->is_category) {
        render_category_children_preview(item, config);
        return;
    }

    // Leaf preview (Song or Visualizer)
    if (m_category_list) m_category_list->set_visibility(Visibility::Gone);
    if (m_preview_scroll) m_preview_scroll->set_visibility(Visibility::Visible);
    m_preview_layout->set_padding(Padding(14, 4, 14, 14));
    m_preview_layout->set_divider_spacing(10);

    if (item->is_visualizer) {
        render_visualizer_preview(item, config);
    } else if (item->is_song) {
        render_song_preview(item, config);
    } else {
        render_empty_preview(config);
    }

    // Preserve focus across live updates
    if (m_window && m_preview_layout) {
        if (m_preview_focused || m_preview_expanded) {
            std::vector<View*> new_focusables;
            m_preview_layout->collect_focusable_views(new_focusables);
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
                m_preview_layout->collect_focusable_views(preview_views);
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

        std::function<void(const std::vector<std::shared_ptr<MusicItem>>&)> search_recursive =
            [&](const std::vector<std::shared_ptr<MusicItem>>& items) {
                for (const auto& item : items) {
                    item->ensure_children();
                    std::string t = item->title;
                    std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c) { return std::tolower(c); });
                    std::string sub = item->subtitle;
                    std::transform(sub.begin(), sub.end(), sub.begin(), [](unsigned char c) { return std::tolower(c); });

                    if (t.find(lower_query) != std::string::npos ||
                        sub.find(lower_query) != std::string::npos ||
                        item->id.find(lower_query) != std::string::npos) {
                        m_search_results.push_back(item);
                    }

                    if (!item->children.empty()) {
                        search_recursive(item->children);
                    }
                }
            };

        search_recursive(m_root_tree);
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

bool MillerView::focus_first_preview_control() {
    if (!m_preview_layout || !m_window) return false;
    std::vector<View*> focusables;
    m_preview_layout->collect_focusable_views(focusables);
    if (focusables.empty()) return false;
    set_preview_focused(true);
    m_window->set_focused_view(focusables.front());
    return true;
}

bool MillerView::focus_next_preview_control() {
    if (!m_preview_layout || !m_window) return false;
    std::vector<View*> focusables;
    m_preview_layout->collect_focusable_views(focusables);
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
    if (!m_preview_layout || !m_window) return false;
    std::vector<View*> focusables;
    m_preview_layout->collect_focusable_views(focusables);
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

bool MillerView::trigger_active_action() {
    auto item = get_current_selected_item();
    if (item && item->has_action) {
        item->last_action_message = "Done at " + std::to_string(std::time(nullptr));
        rebuild_preview_column();
        notify_state_changed();
        return true;
    }
    return false;
}

std::string MillerView::get_breadcrumb_path() const {
    if (!m_search_query.empty()) {
        std::string path = "Music › Search: \"" + m_search_query + "\"";
        auto current = get_current_selected_item();
        if (current) {
            path += " › " + current->title;
        }
        return path;
    }
    std::string path = "Music";
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
