#include "miqutoolkit/view/list_view.hpp"
#include "miqutoolkit/view/card_view.hpp"
#include "miqutoolkit/core/config.hpp"
#include "miqutoolkit/core/window.hpp"
#include <xkbcommon/xkbcommon-keysyms.h>
#include <algorithm>
#include <cmath>

namespace miqu {

ListView::ListView() {}

void ListView::set_items(std::vector<std::shared_ptr<View>> items) {
    m_items = std::move(items);
    m_selected_index = (!m_selectable || m_items.empty()) ? -1 : 0;
    m_anchor_index = m_selected_index;
    m_selected_indices.clear();
    if (m_selectable && m_selected_index >= 0) {
        m_selected_indices.insert(m_selected_index);
    }
    m_scroll_y = 0.0;
}

void ListView::add_item(std::shared_ptr<View> item) {
    if (item) {
        m_items.push_back(std::move(item));
        if (m_selectable && m_selected_index < 0 && !m_items.empty()) {
            m_selected_index = 0;
            m_anchor_index = 0;
            m_selected_indices.insert(0);
        }
    }
}

void ListView::clear_items() {
    m_items.clear();
    m_selected_index = -1;
    m_anchor_index = -1;
    m_hovered_index = -1;
    m_selected_indices.clear();
    m_scroll_y = 0.0;
}

std::shared_ptr<View> ListView::get_item_at(size_t index) const {
    if (index < m_items.size()) {
        return m_items[index];
    }
    return nullptr;
}

void ListView::set_selectable(bool selectable) {
    m_selectable = selectable;
    if (!m_selectable) {
        m_selected_index = -1;
        m_anchor_index = -1;
        m_selected_indices.clear();
        if (m_on_selection_changed) m_on_selection_changed(m_selected_indices);
    }
    request_redraw();
}

void ListView::set_selection_mode(ListViewSelectionMode mode) {
    if (m_selection_mode != mode) {
        m_selection_mode = mode;
        if (m_selection_mode == ListViewSelectionMode::None) {
            m_selected_index = -1;
            m_anchor_index = -1;
            m_selected_indices.clear();
        } else if (m_selection_mode == ListViewSelectionMode::Single) {
            m_selected_indices.clear();
            if (m_selected_index >= 0) {
                m_selected_indices.insert(m_selected_index);
            }
        }
        if (m_on_selection_changed) {
            m_on_selection_changed(m_selected_indices);
        }
        request_redraw();
    }
}

void ListView::set_selected_indices(const std::unordered_set<int>& indices) {
    m_selected_indices = indices;
    if (m_on_selection_changed) {
        m_on_selection_changed(m_selected_indices);
    }
    request_redraw();
}

void ListView::add_selected_index(int index) {
    if (index >= 0 && index < static_cast<int>(m_items.size())) {
        m_selected_indices.insert(index);
        if (m_on_selection_changed) {
            m_on_selection_changed(m_selected_indices);
        }
        request_redraw();
    }
}

void ListView::remove_selected_index(int index) {
    auto it = m_selected_indices.find(index);
    if (it != m_selected_indices.end()) {
        m_selected_indices.erase(it);
        if (m_on_selection_changed) {
            m_on_selection_changed(m_selected_indices);
        }
        request_redraw();
    }
}

void ListView::toggle_selected_index(int index) {
    if (index >= 0 && index < static_cast<int>(m_items.size())) {
        auto it = m_selected_indices.find(index);
        if (it != m_selected_indices.end()) {
            m_selected_indices.erase(it);
        } else {
            m_selected_indices.insert(index);
        }
        if (m_on_selection_changed) {
            m_on_selection_changed(m_selected_indices);
        }
        request_redraw();
    }
}

void ListView::select_range(int from, int to) {
    if (m_items.empty()) return;
    int start = std::clamp(std::min(from, to), 0, static_cast<int>(m_items.size()) - 1);
    int end = std::clamp(std::max(from, to), 0, static_cast<int>(m_items.size()) - 1);
    m_selected_indices.clear();
    for (int i = start; i <= end; ++i) {
        m_selected_indices.insert(i);
    }
    if (m_on_selection_changed) {
        m_on_selection_changed(m_selected_indices);
    }
    request_redraw();
}

void ListView::select_all() {
    m_selected_indices.clear();
    for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
        m_selected_indices.insert(i);
    }
    if (m_on_selection_changed) {
        m_on_selection_changed(m_selected_indices);
    }
    request_redraw();
}

void ListView::clear_selection() {
    m_selected_indices.clear();
    if (m_on_selection_changed) {
        m_on_selection_changed(m_selected_indices);
    }
    request_redraw();
}

void ListView::set_selected_index(int index) {
    if (!m_selectable || m_items.empty() || index < 0) {
        m_selected_index = -1;
        m_anchor_index = -1;
        m_selected_indices.clear();
        if (m_on_selection_changed) m_on_selection_changed(m_selected_indices);
        request_redraw();
        return;
    }
    m_selected_index = std::clamp(index, 0, static_cast<int>(m_items.size()) - 1);
    m_selected_indices.clear();
    m_selected_indices.insert(m_selected_index);
    m_anchor_index = m_selected_index;
    if (m_on_selection_changed) m_on_selection_changed(m_selected_indices);

    if (m_last_height > 0) {
        ensure_visible(m_last_height);
    } else {
        m_scroll_to_selected = true;
    }
    request_redraw();
}

std::shared_ptr<View> ListView::get_selected_item() const {
    if (m_selected_index >= 0 && m_selected_index < static_cast<int>(m_items.size())) {
        return m_items[m_selected_index];
    }
    return nullptr;
}

std::vector<std::shared_ptr<View>> ListView::get_selected_items() const {
    std::vector<std::shared_ptr<View>> result;
    if (m_selection_mode == ListViewSelectionMode::Multiple) {
        for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
            if (m_selected_indices.find(i) != m_selected_indices.end()) {
                result.push_back(m_items[i]);
            }
        }
    } else {
        if (m_selected_index >= 0 && m_selected_index < static_cast<int>(m_items.size())) {
            result.push_back(m_items[m_selected_index]);
        }
    }
    return result;
}

void ListView::ensure_visible(int viewport_height) {
    if (m_selected_index < 0 || viewport_height <= 0 || m_items.empty()) return;

    int row_stride = m_item_h + m_spacing;
    double item_top = m_selected_index * row_stride;
    double item_bottom = item_top + m_item_h;

    if (item_top < m_scroll_y) {
        m_scroll_y = item_top;
    } else if (item_bottom > m_scroll_y + viewport_height) {
        m_scroll_y = item_bottom - viewport_height;
    }

    double content_h = m_items.size() * row_stride - m_spacing;
    double max_scroll = std::max(0.0, content_h - viewport_height);
    m_scroll_y = std::clamp(m_scroll_y, 0.0, max_scroll);
}

int ListView::item_at(int lx, int ly, const Rect& bounds) const {
    int rel_x = lx - bounds.x;
    int rel_y = ly - bounds.y + static_cast<int>(m_scroll_y);

    if (rel_x < 0 || rel_x >= bounds.width || rel_y < 0) {
        return -1;
    }

    int row_stride = m_item_h + m_spacing;
    if (row_stride <= 0) return -1;

    int idx = rel_y / row_stride;
    int offset_in_row = rel_y % row_stride;

    if (offset_in_row > m_item_h) {
        return -1; // Spacing gap
    }

    if (idx >= 0 && idx < static_cast<int>(m_items.size())) {
        return idx;
    }
    return -1;
}

void ListView::draw(cairo_t* cr, const Rect& bounds) {
    if (!is_visible() || !cr || bounds.width <= 0 || bounds.height <= 0) return;

    m_last_height = bounds.height;
    auto config = Config::get();

    int row_stride = m_item_h + m_spacing;
    double content_h = std::max(0, static_cast<int>(m_items.size()) * row_stride - m_spacing);
    double max_scroll = std::max(0.0, content_h - bounds.height);

    if (m_scroll_to_selected) {
        ensure_visible(bounds.height);
        m_scroll_to_selected = false;
    }

    m_scroll_y = std::clamp(m_scroll_y, 0.0, max_scroll);

    cairo_save(cr);
    cairo_rectangle(cr, bounds.x, bounds.y, bounds.width, bounds.height);
    cairo_clip(cr);

    int start_idx = std::max(0, static_cast<int>(m_scroll_y / row_stride));
    int end_idx = std::min(static_cast<int>(m_items.size()), static_cast<int>((m_scroll_y + bounds.height) / row_stride) + 1);

    double radius = m_corner_radius > 0 ? static_cast<double>(m_corner_radius) : 6.0;

    for (int i = start_idx; i < end_idx; ++i) {
        int item_y = bounds.y + i * row_stride - static_cast<int>(m_scroll_y);
        Rect item_rect(bounds.x, item_y, bounds.width, m_item_h);

        bool is_cursor = (m_selectable && i == m_selected_index);
        bool in_multi_select = (m_selection_mode == ListViewSelectionMode::Multiple &&
                                m_selected_indices.find(i) != m_selected_indices.end());
        bool is_hovered = (i == m_hovered_index && !is_cursor);

        // Highlight background
        if (is_cursor) {
            CardView::draw_rounded_rect(cr, item_rect.x, item_rect.y, item_rect.width, item_rect.height, radius);
            cairo_set_source_rgba(cr, config->colors.primary_container.r,
                                      config->colors.primary_container.g,
                                      config->colors.primary_container.b,
                                      0.75f);
            cairo_fill(cr);

            CardView::draw_rounded_rect(cr, item_rect.x + 0.5, item_rect.y + 0.5, item_rect.width - 1.0, item_rect.height - 1.0, radius);
            cairo_set_source_rgba(cr, config->colors.primary.r,
                                      config->colors.primary.g,
                                      config->colors.primary.b,
                                      0.45f);
            cairo_set_line_width(cr, 1.0);
            cairo_stroke(cr);
        } else if (in_multi_select) {
            CardView::draw_rounded_rect(cr, item_rect.x, item_rect.y, item_rect.width, item_rect.height, radius);
            cairo_set_source_rgba(cr, config->colors.primary.r,
                                      config->colors.primary.g,
                                      config->colors.primary.b,
                                      0.12f);
            cairo_fill(cr);
        } else if (is_hovered) {
            CardView::draw_rounded_rect(cr, item_rect.x, item_rect.y, item_rect.width, item_rect.height, radius);
            cairo_set_source_rgba(cr, config->colors.on_surface.r,
                                      config->colors.on_surface.g,
                                      config->colors.on_surface.b,
                                      0.08f);
            cairo_fill(cr);
        }

        // Draw child item
        if (m_items[i]) {
            m_items[i]->draw(cr, item_rect);
        }

        // Left vertical accent marker for multi-selected items (Pure Yazi style)
        if (in_multi_select) {
            double bar_w = 3.5;
            double bar_h = item_rect.height - 14.0;
            double bar_x = item_rect.x + 3.0;
            double bar_y = item_rect.y + 7.0;
            CardView::draw_rounded_rect(cr, bar_x, bar_y, bar_w, bar_h, 1.75);
            cairo_set_source_rgba(cr, config->colors.primary.r,
                                      config->colors.primary.g,
                                      config->colors.primary.b,
                                      0.95f);
            cairo_fill(cr);
        }

        // Optional row divider
        if (m_show_dividers && i < static_cast<int>(m_items.size()) - 1) {
            int div_y = item_y + m_item_h + m_spacing / 2;
            cairo_set_source_rgba(cr, config->colors.outline_variant.r,
                                      config->colors.outline_variant.g,
                                      config->colors.outline_variant.b,
                                      0.3f);
            cairo_set_line_width(cr, 1.0);
            cairo_move_to(cr, bounds.x + 12, div_y);
            cairo_line_to(cr, bounds.x + bounds.width - 12, div_y);
            cairo_stroke(cr);
        }
    }

    // Scrollbar (if enabled and content overflows)
    if (m_scrollbar_enabled && content_h > bounds.height) {
        double scrollbar_w = 4.0;
        double scrollbar_x = bounds.x + bounds.width - scrollbar_w - 2.0;
        double visible_ratio = static_cast<double>(bounds.height) / content_h;
        double thumb_h = std::max(20.0, bounds.height * visible_ratio);
        double thumb_y = bounds.y + (m_scroll_y / max_scroll) * (bounds.height - thumb_h);

        cairo_set_source_rgba(cr, config->colors.on_surface.r,
                                  config->colors.on_surface.g,
                                  config->colors.on_surface.b,
                                  0.25f);
        double sr = scrollbar_w / 2.0;
        cairo_new_sub_path(cr);
        cairo_arc(cr, scrollbar_x + scrollbar_w - sr, thumb_y + sr, sr, -M_PI / 2, 0);
        cairo_arc(cr, scrollbar_x + scrollbar_w - sr, thumb_y + thumb_h - sr, sr, 0, M_PI / 2);
        cairo_arc(cr, scrollbar_x + sr, thumb_y + thumb_h - sr, sr, M_PI / 2, M_PI);
        cairo_arc(cr, scrollbar_x + sr, thumb_y + sr, sr, M_PI, 3 * M_PI / 2);
        cairo_close_path(cr);
        cairo_fill(cr);
    }

    cairo_restore(cr);
}

bool ListView::on_mouse_move(int lx, int ly, const Rect& bounds) {
    int old_hover = m_hovered_index;
    m_hovered_index = item_at(lx, ly, bounds);
    return m_hovered_index != old_hover;
}

bool ListView::on_mouse_button(int lx, int ly, MouseButton button, bool pressed, const Rect& bounds) {
    if (button == MouseButton::Left && pressed) {
        int idx = item_at(lx, ly, bounds);
        if (idx >= 0 && idx < static_cast<int>(m_items.size())) {
            Window* win = get_window();
            bool is_shift = win && win->has_shift();
            bool is_ctrl = win && win->has_ctrl();

            if (m_selectable) {
                if (m_selection_mode == ListViewSelectionMode::Multiple) {
                    if (is_shift) {
                        if (m_anchor_index < 0) {
                            m_anchor_index = (m_selected_index >= 0 ? m_selected_index : idx);
                        }
                        select_range(m_anchor_index, idx);
                        m_selected_index = idx;
                    } else if (is_ctrl) {
                        toggle_selected_index(idx);
                        m_selected_index = idx;
                        m_anchor_index = idx;
                    } else {
                        m_selected_index = idx;
                        m_anchor_index = idx;
                        m_selected_indices.clear();
                        m_selected_indices.insert(idx);
                        if (m_on_selection_changed) {
                            m_on_selection_changed(m_selected_indices);
                        }
                    }
                } else {
                    set_selected_index(idx);
                }
            }
            if (m_on_item_click) {
                m_on_item_click(idx, m_items[idx]);
            }
            request_redraw();
            return true;
        }
    } else if (button == MouseButton::Right && pressed) {
        int idx = item_at(lx, ly, bounds);
        if (idx >= 0 && idx < static_cast<int>(m_items.size())) {
            if (m_selectable) {
                if (m_selection_mode == ListViewSelectionMode::Multiple) {
                    if (m_selected_indices.find(idx) == m_selected_indices.end()) {
                        m_selected_index = idx;
                        m_anchor_index = idx;
                        m_selected_indices.clear();
                        m_selected_indices.insert(idx);
                        if (m_on_selection_changed) {
                            m_on_selection_changed(m_selected_indices);
                        }
                    }
                } else {
                    m_selected_index = idx;
                }
            }
            if (m_on_item_context_click) {
                m_on_item_context_click(idx, m_items[idx], lx, ly);
                return true;
            }
        } else if (m_on_empty_context_click) {
            m_on_empty_context_click(lx, ly);
            return true;
        }
        if (handle_context_click(lx, ly, bounds)) {
            return true;
        }
    }
    return false;
}

bool ListView::on_scroll(double delta) {
    if (m_items.empty() || m_last_height <= 0) return false;

    int row_stride = m_item_h + m_spacing;
    double content_h = std::max(0, static_cast<int>(m_items.size()) * row_stride - m_spacing);
    double max_scroll = std::max(0.0, content_h - m_last_height);

    if (max_scroll <= 0.0) return false;

    double old_scroll = m_scroll_y;
    m_scroll_y = std::clamp(m_scroll_y + delta * 30.0, 0.0, max_scroll);
    return std::abs(m_scroll_y - old_scroll) > 0.01;
}

bool ListView::on_key(const KeyPressEvent& event) {
    if (!m_keyboard_navigation_enabled) return false;
    if (!m_selectable || m_items.empty()) return false;

    int count = static_cast<int>(m_items.size());
    int old_selected = m_selected_index;

    // Multi-select specific shortcuts
    if (m_selection_mode == ListViewSelectionMode::Multiple) {
        // Ctrl+A: Select All
        if (event.has_ctrl() && (event.keysym == XKB_KEY_a || event.keysym == XKB_KEY_A)) {
            select_all();
            return true;
        }

        // Escape: Cancel multi-selection, collapse back to 1 selected item at cursor
        if (event.keysym == XKB_KEY_Escape) {
            if (m_selected_indices.size() > 1 || m_selected_indices.find(m_selected_index) == m_selected_indices.end()) {
                m_selected_indices.clear();
                if (m_selected_index >= 0 && m_selected_index < count) {
                    m_selected_indices.insert(m_selected_index);
                }
                m_anchor_index = m_selected_index;
                if (m_on_selection_changed) {
                    m_on_selection_changed(m_selected_indices);
                }
                request_redraw();
                return true;
            }
            return false;
        }

        // Space: Sporadic toggle current item & step down 1 row
        if (event.keysym == XKB_KEY_space) {
            if (m_selected_index >= 0 && m_selected_index < count) {
                auto it = m_selected_indices.find(m_selected_index);
                if (it != m_selected_indices.end()) {
                    m_selected_indices.erase(it);
                } else {
                    m_selected_indices.insert(m_selected_index);
                }
                m_anchor_index = m_selected_index;
                if (m_on_selection_changed) {
                    m_on_selection_changed(m_selected_indices);
                }
                // Step down 1 row if available
                if (m_selected_index < count - 1) {
                    m_selected_index++;
                    if (m_last_height > 0) {
                        ensure_visible(m_last_height);
                    }
                }
                request_redraw();
                return true;
            }
        }
    }

    switch (event.keysym) {
        case XKB_KEY_Up:
        case XKB_KEY_k:
        case XKB_KEY_K:
            if (m_selected_index > 0) {
                m_selected_index--;
                if (m_selection_mode == ListViewSelectionMode::Multiple) {
                    if (event.has_shift()) {
                        if (m_anchor_index < 0) m_anchor_index = old_selected;
                        select_range(m_anchor_index, m_selected_index);
                    } else {
                        m_selected_indices.clear();
                        m_selected_indices.insert(m_selected_index);
                        m_anchor_index = m_selected_index;
                        if (m_on_selection_changed) m_on_selection_changed(m_selected_indices);
                    }
                } else {
                    m_selected_indices.clear();
                    m_selected_indices.insert(m_selected_index);
                    m_anchor_index = m_selected_index;
                    if (m_on_selection_changed) m_on_selection_changed(m_selected_indices);
                }
            }
            break;

        case XKB_KEY_Down:
        case XKB_KEY_j:
        case XKB_KEY_J:
            if (m_selected_index < count - 1) {
                m_selected_index++;
                if (m_selection_mode == ListViewSelectionMode::Multiple) {
                    if (event.has_shift()) {
                        if (m_anchor_index < 0) m_anchor_index = old_selected;
                        select_range(m_anchor_index, m_selected_index);
                    } else {
                        m_selected_indices.clear();
                        m_selected_indices.insert(m_selected_index);
                        m_anchor_index = m_selected_index;
                        if (m_on_selection_changed) m_on_selection_changed(m_selected_indices);
                    }
                } else {
                    m_selected_indices.clear();
                    m_selected_indices.insert(m_selected_index);
                    m_anchor_index = m_selected_index;
                    if (m_on_selection_changed) m_on_selection_changed(m_selected_indices);
                }
            }
            break;

        case XKB_KEY_Page_Up: {
            int page_items = std::max(1, m_last_height / (m_item_h + m_spacing));
            m_selected_index = std::max(0, m_selected_index - page_items);
            if (m_selection_mode == ListViewSelectionMode::Multiple) {
                if (event.has_shift()) {
                    if (m_anchor_index < 0) m_anchor_index = old_selected;
                    select_range(m_anchor_index, m_selected_index);
                } else {
                    m_selected_indices.clear();
                    m_selected_indices.insert(m_selected_index);
                    m_anchor_index = m_selected_index;
                    if (m_on_selection_changed) m_on_selection_changed(m_selected_indices);
                }
            } else {
                m_selected_indices.clear();
                m_selected_indices.insert(m_selected_index);
                m_anchor_index = m_selected_index;
                if (m_on_selection_changed) m_on_selection_changed(m_selected_indices);
            }
            break;
        }

        case XKB_KEY_Page_Down: {
            int page_items = std::max(1, m_last_height / (m_item_h + m_spacing));
            m_selected_index = std::min(count - 1, m_selected_index + page_items);
            if (m_selection_mode == ListViewSelectionMode::Multiple) {
                if (event.has_shift()) {
                    if (m_anchor_index < 0) m_anchor_index = old_selected;
                    select_range(m_anchor_index, m_selected_index);
                } else {
                    m_selected_indices.clear();
                    m_selected_indices.insert(m_selected_index);
                    m_anchor_index = m_selected_index;
                    if (m_on_selection_changed) m_on_selection_changed(m_selected_indices);
                }
            } else {
                m_selected_indices.clear();
                m_selected_indices.insert(m_selected_index);
                m_anchor_index = m_selected_index;
                if (m_on_selection_changed) m_on_selection_changed(m_selected_indices);
            }
            break;
        }

        case XKB_KEY_Home:
            m_selected_index = 0;
            if (m_selection_mode == ListViewSelectionMode::Multiple) {
                if (event.has_shift()) {
                    if (m_anchor_index < 0) m_anchor_index = old_selected;
                    select_range(m_anchor_index, m_selected_index);
                } else {
                    m_selected_indices.clear();
                    m_selected_indices.insert(m_selected_index);
                    m_anchor_index = m_selected_index;
                    if (m_on_selection_changed) m_on_selection_changed(m_selected_indices);
                }
            } else {
                m_selected_indices.clear();
                m_selected_indices.insert(m_selected_index);
                m_anchor_index = m_selected_index;
                if (m_on_selection_changed) m_on_selection_changed(m_selected_indices);
            }
            break;

        case XKB_KEY_End:
            m_selected_index = count - 1;
            if (m_selection_mode == ListViewSelectionMode::Multiple) {
                if (event.has_shift()) {
                    if (m_anchor_index < 0) m_anchor_index = old_selected;
                    select_range(m_anchor_index, m_selected_index);
                } else {
                    m_selected_indices.clear();
                    m_selected_indices.insert(m_selected_index);
                    m_anchor_index = m_selected_index;
                    if (m_on_selection_changed) m_on_selection_changed(m_selected_indices);
                }
            } else {
                m_selected_indices.clear();
                m_selected_indices.insert(m_selected_index);
                m_anchor_index = m_selected_index;
                if (m_on_selection_changed) m_on_selection_changed(m_selected_indices);
            }
            break;

        case XKB_KEY_Return:
        case XKB_KEY_KP_Enter:
            if (m_selected_index >= 0 && m_selected_index < count) {
                if (m_on_item_click) {
                    m_on_item_click(m_selected_index, m_items[m_selected_index]);
                }
                return true;
            }
            break;

        default:
            return false;
    }

    if (m_selected_index != old_selected) {
        if (m_last_height > 0) {
            ensure_visible(m_last_height);
        }
        request_redraw();
        return true;
    }

    return false;
}

bool ListView::on_touch(const TouchEvent& event, const Rect& bounds) {
    if (!is_visible()) return false;

    if (event.phase == TouchPhase::Down && !bounds.contains(Point(static_cast<int>(event.x), static_cast<int>(event.y)))) {
        return false;
    }

    if (event.phase == TouchPhase::Down) {
        m_touch_id = event.id;
        m_touch_start_x = event.x;
        m_touch_start_y = event.y;
        m_touch_last_y = event.y;
        m_touch_scrolling = false;
        m_hovered_index = item_at(static_cast<int>(event.x), static_cast<int>(event.y), bounds);
        if (get_window()) get_window()->schedule_redraw();
        return true;
    }

    if (event.id != m_touch_id) return false;

    if (event.phase == TouchPhase::Motion) {
        double dy = event.y - m_touch_last_y;
        m_touch_last_y = event.y;

        if (!m_touch_scrolling) {
            double total_dy = std::abs(event.y - m_touch_start_y);
            double total_dx = std::abs(event.x - m_touch_start_x);
            if (total_dy > 8.0 && total_dy > total_dx) {
                m_touch_scrolling = true;
                m_hovered_index = -1;
            }
        }

        if (m_touch_scrolling) {
            int row_stride = m_item_h + m_spacing;
            double content_h = std::max(0, static_cast<int>(m_items.size()) * row_stride - m_spacing);
            double max_scroll = std::max(0.0, content_h - m_last_height);
            if (max_scroll > 0.0) {
                double old_scroll = m_scroll_y;
                m_scroll_y = std::clamp(m_scroll_y - dy, 0.0, max_scroll);
                if (m_scroll_y != old_scroll && get_window()) {
                    get_window()->schedule_redraw();
                }
            }
            return true;
        }
        return true;
    }

    if (event.phase == TouchPhase::Up) {
        m_touch_id = -1;
        if (m_touch_scrolling) {
            m_touch_scrolling = false;
            m_hovered_index = -1;
            if (get_window()) get_window()->schedule_redraw();
            return true;
        }

        m_touch_scrolling = false;
        int idx = item_at(static_cast<int>(event.x), static_cast<int>(event.y), bounds);
        if (idx >= 0 && idx < static_cast<int>(m_items.size())) {
            if (m_selectable) {
                m_selected_index = idx;
            }
            m_hovered_index = idx;
            if (m_on_item_click) {
                m_on_item_click(idx, m_items[idx]);
            }
            if (get_window()) get_window()->schedule_redraw();
            return true;
        }
        return true;
    }

    if (event.phase == TouchPhase::Cancel) {
        m_touch_id = -1;
        m_touch_scrolling = false;
        m_hovered_index = -1;
        if (get_window()) get_window()->schedule_redraw();
        return true;
    }

    return false;
}

} // namespace miqu

