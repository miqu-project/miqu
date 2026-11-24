#include "miqutoolkit/view/list_view.hpp"
#include "miqutoolkit/view/card_view.hpp"
#include "miqutoolkit/core/config.hpp"
#include "miqutoolkit/core/window.hpp"
#include <cmath>
#include <algorithm>

namespace miqu {

ListView::ListView() {
    set_focusable(true);
    set_selection_model(std::make_shared<SelectionModel>(SelectionMode::Single));
}

ListView::~ListView() {
    teardown_selection_model_listeners();
}

void ListView::setup_selection_model_listeners() {
    if (!m_selection_model) return;
    m_selection_listener_id = m_selection_model->add_selection_changed_listener([this](const std::unordered_set<int>& indices) {
        if (m_on_selection_changed) {
            m_on_selection_changed(indices);
        }
        request_redraw();
    });
    m_cursor_listener_id = m_selection_model->add_cursor_changed_listener([this](int cur) {
        if (m_on_cursor_changed) {
            m_on_cursor_changed(cur);
        }
        request_redraw();
    });
}

void ListView::teardown_selection_model_listeners() {
    if (!m_selection_model) return;
    if (m_selection_listener_id != 0) {
        m_selection_model->remove_selection_changed_listener(m_selection_listener_id);
        m_selection_listener_id = 0;
    }
    if (m_cursor_listener_id != 0) {
        m_selection_model->remove_cursor_changed_listener(m_cursor_listener_id);
        m_cursor_listener_id = 0;
    }
}

void ListView::set_selection_model(std::shared_ptr<SelectionModel> model) {
    if (m_selection_model == model) return;
    teardown_selection_model_listeners();
    m_selection_model = model ? std::move(model) : std::make_shared<SelectionModel>(SelectionMode::Single);
    setup_selection_model_listeners();
    request_redraw();
}

void ListView::set_selectable(bool selectable) {
    m_selectable = selectable;
    if (!m_selectable && m_selection_model) {
        m_selection_model->reset();
    }
    request_redraw();
}

void ListView::set_selection_mode(SelectionMode mode) {
    if (m_selection_model) {
        m_selection_model->set_mode(mode);
    }
    request_redraw();
}

SelectionMode ListView::get_selection_mode() const {
    return m_selection_model ? m_selection_model->mode() : SelectionMode::Single;
}

bool ListView::is_multi_select() const {
    return m_selection_model && m_selection_model->mode() == SelectionMode::Multiple;
}

const std::unordered_set<int>& ListView::get_selected_indices() const {
    static const std::unordered_set<int> empty;
    return m_selection_model ? m_selection_model->marks() : empty;
}

bool ListView::is_index_selected(int index) const {
    if (!m_selectable) return false;
    return m_selection_model && m_selection_model->is_selected(index);
}

size_t ListView::get_selected_count() const {
    if (!m_selectable || m_items.empty()) return 0;
    return m_selection_model ? m_selection_model->selection_count() : 0;
}

void ListView::set_selected_indices(const std::unordered_set<int>& indices) {
    if (m_selection_model) {
        m_selection_model->set_marks(indices);
    }
    request_redraw();
}

void ListView::add_selected_index(int index) {
    if (m_selection_model) {
        m_selection_model->select(index);
    }
    request_redraw();
}

void ListView::remove_selected_index(int index) {
    if (m_selection_model) {
        m_selection_model->deselect(index);
    }
    request_redraw();
}

void ListView::toggle_selected_index(int index) {
    if (m_selection_model) {
        m_selection_model->toggle(index);
    }
    request_redraw();
}

void ListView::select_range(int from, int to) {
    if (m_selection_model) {
        m_selection_model->select_range(from, to);
    }
    request_redraw();
}

void ListView::select_all() {
    if (m_selection_model) {
        m_selection_model->select_all(m_items.size());
    }
    request_redraw();
}

void ListView::clear_selection() {
    if (m_selection_model) {
        m_selection_model->clear_marks();
    }
    request_redraw();
}

int ListView::get_anchor_index() const {
    return m_selection_model ? m_selection_model->anchor() : -1;
}

void ListView::set_anchor_index(int index) {
    if (m_selection_model) {
        m_selection_model->set_anchor(index);
    }
}

void ListView::set_selected_index(int index) {
    if (!m_selectable || m_items.empty() || index < 0) {
        if (m_selection_model) {
            m_selection_model->reset();
        }
        request_redraw();
        return;
    }
    int clamped = std::clamp(index, 0, static_cast<int>(m_items.size()) - 1);
    if (m_selection_model) {
        m_selection_model->select_single(clamped);
    }
    if (m_last_height > 0) {
        ensure_visible(m_last_height);
    } else {
        m_scroll_to_selected = true;
    }
    request_redraw();
}

int ListView::get_selected_index() const {
    return m_selection_model ? m_selection_model->cursor() : -1;
}

std::shared_ptr<View> ListView::get_selected_item() const {
    if (!m_selectable || m_items.empty() || !m_selection_model) return nullptr;
    int cur = m_selection_model->cursor();
    if (cur >= 0 && cur < static_cast<int>(m_items.size())) {
        return m_items[cur];
    }
    return nullptr;
}

std::vector<std::shared_ptr<View>> ListView::get_selected_items() const {
    std::vector<std::shared_ptr<View>> result;
    if (!m_selectable || m_items.empty() || !m_selection_model) return result;
    auto indices = m_selection_model->effective_indices();
    result.reserve(indices.size());
    for (int idx : indices) {
        if (idx >= 0 && idx < static_cast<int>(m_items.size())) {
            result.push_back(m_items[idx]);
        }
    }
    return result;
}

void ListView::set_items(std::vector<std::shared_ptr<View>> items) {
    m_items = std::move(items);
    m_hovered_index = -1;
    if (m_selection_model) {
        if (m_items.empty()) {
            m_selection_model->reset();
        } else if (m_selection_model->cursor() >= static_cast<int>(m_items.size())) {
            m_selection_model->set_cursor(static_cast<int>(m_items.size()) - 1);
        }
    }
    request_redraw();
}

void ListView::add_item(std::shared_ptr<View> item) {
    m_items.push_back(std::move(item));
    request_redraw();
}

void ListView::clear_items() {
    m_items.clear();
    m_hovered_index = -1;
    if (m_selection_model) {
        m_selection_model->reset(false);
    }
    m_scroll_y = 0.0;
    request_redraw();
}

std::shared_ptr<View> ListView::get_item_at(size_t index) const {
    if (index < m_items.size()) {
        return m_items[index];
    }
    return nullptr;
}

void ListView::ensure_visible(int viewport_height) {
    if (!m_selection_model) return;
    int cur = m_selection_model->cursor();
    if (cur < 0 || cur >= static_cast<int>(m_items.size())) return;

    int row_stride = m_item_h + m_spacing;
    double item_top = cur * row_stride;
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
    if (m_items.empty()) return -1;
    if (lx < bounds.x || lx >= bounds.x + bounds.width) return -1;

    double relative_y = ly - bounds.y + m_scroll_y;
    if (relative_y < 0) return -1;

    int row_stride = m_item_h + m_spacing;
    int index = static_cast<int>(relative_y / row_stride);

    if (index >= 0 && index < static_cast<int>(m_items.size())) {
        double offset_in_row = relative_y - (index * row_stride);
        if (offset_in_row <= m_item_h) {
            return index;
        }
    }
    return -1;
}

void ListView::draw(cairo_t* cr, const Rect& bounds) {
    if (!is_visible() || bounds.width <= 0 || bounds.height <= 0) return;
    m_last_height = bounds.height;

    if (m_scroll_to_selected) {
        ensure_visible(m_last_height);
        m_scroll_to_selected = false;
    }

    auto config = Config::get();
    if (!config) return;

    cairo_save(cr);
    cairo_rectangle(cr, bounds.x, bounds.y, bounds.width, bounds.height);
    cairo_clip(cr);

    int row_stride = m_item_h + m_spacing;
    int start_idx = std::max(0, static_cast<int>(m_scroll_y / row_stride));
    int end_idx = std::min(static_cast<int>(m_items.size()),
                           static_cast<int>((m_scroll_y + bounds.height) / row_stride) + 1);

    double radius = m_corner_radius > 0 ? static_cast<double>(m_corner_radius) : 6.0;

    for (int i = start_idx; i < end_idx; ++i) {
        int item_y = bounds.y + i * row_stride - static_cast<int>(m_scroll_y);
        int h_margin = 4;
        Rect item_rect(bounds.x + h_margin, item_y, std::max(0, bounds.width - (h_margin * 2)), m_item_h);

        bool is_cursor = (m_selectable && m_selection_model && m_selection_model->is_cursor(i));
        bool in_multi_select = (m_selectable && m_selection_model &&
                                m_selection_model->mode() == SelectionMode::Multiple &&
                                m_selection_model->is_marked(i));
        bool is_hovered = (i == m_hovered_index && !is_cursor);

        // Background highlight
        if (is_cursor) {
            CardView::draw_rounded_rect(cr, item_rect.x, item_rect.y, item_rect.width, item_rect.height, radius);
            cairo_set_source_rgba(cr, config->colors.primary_container.r,
                                      config->colors.primary_container.g,
                                      config->colors.primary_container.b,
                                      0.75f);
            cairo_fill(cr);

            CardView::draw_rounded_rect(cr, item_rect.x + 0.5, item_rect.y + 0.5,
                                        item_rect.width - 1.0, item_rect.height - 1.0, radius);
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

        // Child item
        if (m_items[i]) {
            m_items[i]->draw(cr, item_rect);
        }

        // Left accent bar for marked items (Yazi style)
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

        // Divider
        if (m_show_dividers && i < static_cast<int>(m_items.size()) - 1) {
            int div_y = item_y + m_item_h + m_spacing / 2;
            cairo_set_source_rgba(cr, config->colors.outline_variant.r,
                                      config->colors.outline_variant.g,
                                      config->colors.outline_variant.b,
                                      0.3f);
            cairo_set_line_width(cr, 1.0);
            cairo_move_to(cr, bounds.x + 12, div_y + 0.5);
            cairo_line_to(cr, bounds.x + bounds.width - 12, div_y + 0.5);
            cairo_stroke(cr);
        }
    }

    // Scrollbar
    double content_h = m_items.size() * row_stride - m_spacing;
    double max_scroll = std::max(0.0, content_h - bounds.height);
    if (m_scrollbar_enabled && max_scroll > 0.0) {
        double track_h = bounds.height - 8;
        double thumb_h = std::max(20.0, (bounds.height / content_h) * track_h);
        double thumb_y = bounds.y + 4 + (m_scroll_y / max_scroll) * (track_h - thumb_h);
        double thumb_x = bounds.x + bounds.width - 6;

        CardView::draw_rounded_rect(cr, thumb_x, thumb_y, 4, thumb_h, 2.0);
        cairo_set_source_rgba(cr, config->colors.on_surface.r,
                                  config->colors.on_surface.g,
                                  config->colors.on_surface.b,
                                  0.25f);
        cairo_fill(cr);
    }

    cairo_restore(cr);
}

bool ListView::on_mouse_move(int lx, int ly, const Rect& bounds) {
    int old_hover = m_hovered_index;
    m_hovered_index = item_at(lx, ly, bounds);

    if (m_hovered_index != old_hover) {
        request_redraw();
        return true;
    }
    return false;
}

bool ListView::on_mouse_button(int lx, int ly, MouseButton button, bool pressed, const Rect& bounds) {
    if (m_items.empty()) return false;
    if (!m_selectable && !m_on_item_click && !m_on_item_context_click) return false;
    int count = static_cast<int>(m_items.size());

    if (button == MouseButton::Left && pressed) {
        int idx = item_at(lx, ly, bounds);
        if (idx >= 0 && idx < count) {
            Window* win = get_window();
            bool is_shift = win && win->has_shift();
            bool is_ctrl = win && win->has_ctrl();

            if (m_on_item_click) {
                m_on_item_click(idx, m_items[idx]);
            }
            if (m_selectable && m_selection_model) {
                m_selection_model->handle_click(idx, is_shift, is_ctrl, count);
            }
            request_redraw();
            return true;
        }
    } else if (button == MouseButton::Right && pressed) {
        int idx = item_at(lx, ly, bounds);
        if (idx >= 0 && idx < count) {
            if (m_selectable && m_selection_model) {
                m_selection_model->handle_context_click(idx);
            }
            if (m_on_item_context_click) {
                m_on_item_context_click(idx, m_items[idx], lx, ly);
                request_redraw();
                return true;
            }
        } else if (m_on_empty_context_click) {
            m_on_empty_context_click(lx, ly);
            request_redraw();
            return true;
        }
    }
    return false;
}

bool ListView::on_scroll(double delta, uint32_t axis_source) {
    int row_stride = m_item_h + m_spacing;
    double content_h = m_items.size() * row_stride - m_spacing;
    double max_scroll = std::max(0.0, content_h - m_last_height);

    // WL_POINTER_AXIS_SOURCE_FINGER == 1: touchpad deltas are already in
    // surface pixels — use 1:1. Mouse wheel notches are ~10 px raw, so
    // multiply by 30 to scroll a comfortable distance per click.
    const double multiplier = (axis_source == WL_POINTER_AXIS_SOURCE_FINGER) ? 1.0 : 30.0;

    double old_scroll = m_scroll_y;
    m_scroll_y = std::clamp(m_scroll_y + delta * multiplier, 0.0, max_scroll);
    return std::abs(m_scroll_y - old_scroll) > 0.01;
}

bool ListView::on_key(const KeyPressEvent& event) {
    if (!event.pressed || !m_keyboard_navigation_enabled || !m_selectable || m_items.empty() || !m_selection_model) {
        return false;
    }
    int count = static_cast<int>(m_items.size());

    // Ctrl+A: Select All
    if (m_selection_model->mode() == SelectionMode::Multiple &&
        event.has_ctrl() && (event.keysym == XKB_KEY_a || event.keysym == XKB_KEY_A)) {
        if (m_selection_model->handle_select_all(count)) {
            request_redraw();
            return true;
        }
    }

    // Escape: Clear explicit marks
    if (event.keysym == XKB_KEY_Escape) {
        if (m_selection_model->handle_escape()) {
            request_redraw();
            return true;
        }
        return false;
    }

    // Space: Toggle & step
    if (event.keysym == XKB_KEY_space) {
        if (m_selection_model->handle_space(count, true)) {
            if (m_last_height > 0) {
                ensure_visible(m_last_height);
            }
            request_redraw();
            return true;
        }
        return false;
    }

    int old_cursor = m_selection_model->cursor();
    int target = old_cursor;

    switch (event.keysym) {
        case XKB_KEY_Up:
        case XKB_KEY_k:
        case XKB_KEY_K:
            if (old_cursor > 0) target = old_cursor - 1;
            break;

        case XKB_KEY_Down:
        case XKB_KEY_j:
        case XKB_KEY_J:
            if (old_cursor < count - 1) target = old_cursor + 1;
            break;

        case XKB_KEY_Page_Up: {
            int page_items = std::max(1, m_last_height / (m_item_h + m_spacing));
            target = std::max(0, old_cursor - page_items);
            break;
        }

        case XKB_KEY_Page_Down: {
            int page_items = std::max(1, m_last_height / (m_item_h + m_spacing));
            target = std::min(count - 1, old_cursor + page_items);
            break;
        }

        case XKB_KEY_Home:
            target = 0;
            break;

        case XKB_KEY_End:
            target = count - 1;
            break;

        case XKB_KEY_Return:
        case XKB_KEY_KP_Enter:
            if (old_cursor >= 0 && old_cursor < count && m_on_item_click) {
                m_on_item_click(old_cursor, m_items[old_cursor]);
                return true;
            }
            return false;

        default:
            return false;
    }

    if (target != old_cursor) {
        m_selection_model->handle_nav(target, event.has_shift());
        if (m_last_height > 0) {
            ensure_visible(m_last_height);
        }
        request_redraw();
        return true;
    }

    return false;
}

bool ListView::on_touch(const TouchEvent& event, const Rect& bounds) {
    if (!is_visible() || !is_enabled()) return false;

    int row_stride = m_item_h + m_spacing;
    double content_h = m_items.size() * row_stride - m_spacing;
    double max_scroll = std::max(0.0, content_h - bounds.height);

    if (event.phase == TouchPhase::Down && !bounds.contains(Point(static_cast<int>(event.x), static_cast<int>(event.y)))) {
        return false;
    }

    switch (event.phase) {
        case TouchPhase::Down: {
            if (m_touch_id == -1) {
                m_touch_id = event.id;
                m_touch_start_x = event.x;
                m_touch_start_y = event.y;
                m_touch_last_y = event.y;
                m_touch_scrolling = false;
                m_hovered_index = item_at(static_cast<int>(event.x), static_cast<int>(event.y), bounds);
                request_redraw();
                return true;
            }
            break;
        }
        case TouchPhase::Motion: {
            if (event.id == m_touch_id) {
                double dy = event.y - m_touch_last_y;
                double total_dy = std::abs(event.y - m_touch_start_y);
                double total_dx = std::abs(event.x - m_touch_start_x);

                if (!m_touch_scrolling && total_dy > 8.0 && total_dy > total_dx) {
                    m_touch_scrolling = true;
                    m_hovered_index = -1;
                }

                if (m_touch_scrolling) {
                    m_scroll_y = std::clamp(m_scroll_y - dy, 0.0, max_scroll);
                    request_redraw();
                }
                m_touch_last_y = event.y;
                return true;
            }
            break;
        }
        case TouchPhase::Up: {
            if (event.id == m_touch_id) {
                m_touch_id = -1;
                if (!m_touch_scrolling) {
                    int idx = item_at(static_cast<int>(event.x), static_cast<int>(event.y), bounds);
                    if (idx >= 0 && idx < static_cast<int>(m_items.size())) {
                        set_selected_index(idx);
                        if (m_on_item_click) {
                            m_on_item_click(idx, m_items[idx]);
                        }
                    }
                }
                m_touch_scrolling = false;
                m_hovered_index = -1;
                request_redraw();
                return true;
            }
            break;
        }
        case TouchPhase::Cancel: {
            if (event.id == m_touch_id) {
                m_touch_id = -1;
                m_touch_scrolling = false;
                m_hovered_index = -1;
                request_redraw();
                return true;
            }
            break;
        }
    }
    return false;
}

} // namespace miqu
