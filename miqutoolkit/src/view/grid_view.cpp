#include "miqutoolkit/view/grid_view.hpp"
#include "miqutoolkit/view/card_view.hpp"
#include "miqutoolkit/core/config.hpp"
#include "miqutoolkit/core/window.hpp"
#include <xkbcommon/xkbcommon-keysyms.h>
#include <algorithm>
#include <cmath>

namespace miqu {

GridView::GridView() {
    set_focusable(true);
    set_selection_model(std::make_shared<SelectionModel>(SelectionMode::Single));
}

GridView::~GridView() {
    teardown_selection_model_listeners();
}

void GridView::setup_selection_model_listeners() {
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

void GridView::teardown_selection_model_listeners() {
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

void GridView::set_selection_model(std::shared_ptr<SelectionModel> model) {
    if (m_selection_model == model) return;
    teardown_selection_model_listeners();
    m_selection_model = model ? std::move(model) : std::make_shared<SelectionModel>(SelectionMode::Single);
    setup_selection_model_listeners();
    request_redraw();
}

void GridView::set_selection_mode(SelectionMode mode) {
    if (m_selection_model) {
        m_selection_model->set_mode(mode);
    }
    request_redraw();
}

SelectionMode GridView::get_selection_mode() const {
    return m_selection_model ? m_selection_model->mode() : SelectionMode::Single;
}

bool GridView::is_multi_select() const {
    return m_selection_model && m_selection_model->mode() == SelectionMode::Multiple;
}

const std::unordered_set<int>& GridView::get_selected_indices() const {
    static const std::unordered_set<int> empty;
    return m_selection_model ? m_selection_model->marks() : empty;
}

bool GridView::is_index_selected(int index) const {
    return m_selection_model && m_selection_model->is_selected(index);
}

size_t GridView::get_selected_count() const {
    if (get_item_count() == 0) return 0;
    return m_selection_model ? m_selection_model->selection_count() : 0;
}

void GridView::set_selected_indices(const std::unordered_set<int>& indices) {
    if (m_selection_model) {
        m_selection_model->set_marks(indices);
    }
    request_redraw();
}

void GridView::add_selected_index(int index) {
    if (m_selection_model) {
        m_selection_model->select(index);
    }
    request_redraw();
}

void GridView::remove_selected_index(int index) {
    if (m_selection_model) {
        m_selection_model->deselect(index);
    }
    request_redraw();
}

void GridView::toggle_selected_index(int index) {
    if (m_selection_model) {
        m_selection_model->toggle(index);
    }
    request_redraw();
}

void GridView::select_range(int from, int to) {
    if (m_selection_model) {
        m_selection_model->select_range(from, to);
    }
    request_redraw();
}

void GridView::select_all() {
    if (m_selection_model) {
        m_selection_model->select_all(get_item_count());
    }
    request_redraw();
}

void GridView::clear_selection() {
    if (m_selection_model) {
        m_selection_model->clear_marks();
    }
    request_redraw();
}

int GridView::get_anchor_index() const {
    return m_selection_model ? m_selection_model->anchor() : -1;
}

void GridView::set_anchor_index(int index) {
    if (m_selection_model) {
        m_selection_model->set_anchor(index);
    }
}

void GridView::set_selected_index(int index) {
    size_t count = get_item_count();
    if (count == 0 || index < 0) {
        if (m_selection_model) {
            m_selection_model->reset();
        }
        request_redraw();
        return;
    }
    int clamped = std::clamp(index, 0, static_cast<int>(count) - 1);
    if (m_selection_model) {
        m_selection_model->select_single(clamped);
    }

    if (m_last_height > 0 && m_effective_cols > 0) {
        int row_stride = m_cell_h + m_space_y;
        ensure_visible(m_last_height, m_effective_cols, row_stride);
    }
    request_redraw();
}

int GridView::get_selected_index() const {
    return m_selection_model ? m_selection_model->cursor() : -1;
}

std::shared_ptr<View> GridView::get_selected_item() const {
    if (!m_selection_model) return nullptr;
    int cur = m_selection_model->cursor();
    if (cur >= 0 && cur < static_cast<int>(get_item_count())) {
        return get_item_at(static_cast<size_t>(cur));
    }
    return nullptr;
}

std::vector<std::shared_ptr<View>> GridView::get_selected_items() const {
    std::vector<std::shared_ptr<View>> result;
    size_t count = get_item_count();
    if (count == 0 || !m_selection_model) return result;
    auto indices = m_selection_model->effective_indices();
    result.reserve(indices.size());
    for (int idx : indices) {
        if (idx >= 0 && idx < static_cast<int>(count)) {
            auto item = get_item_at(static_cast<size_t>(idx));
            if (item) result.push_back(std::move(item));
        }
    }
    return result;
}

void GridView::set_items(std::vector<std::shared_ptr<View>> items) {
    m_items = std::move(items);
    m_item_provider = nullptr;
    m_virtual_count = 0;
    m_hovered_index = -1;
    if (m_selection_model) {
        if (m_items.empty()) {
            m_selection_model->reset();
        } else {
            m_selection_model->select_single(0);
        }
    }
    m_scroll_y = 0.0;
    request_redraw();
}

void GridView::add_item(std::shared_ptr<View> item) {
    if (item) {
        m_items.push_back(std::move(item));
        if (m_selection_model && m_selection_model->cursor() < 0 && !m_items.empty()) {
            m_selection_model->select_single(0);
        }
        request_redraw();
    }
}

void GridView::set_item_provider(size_t total_count, ItemProviderCallback provider) {
    m_items.clear();
    m_virtual_count = total_count;
    m_item_provider = std::move(provider);
    m_hovered_index = -1;
    if (m_selection_model) {
        if (total_count == 0) {
            m_selection_model->reset();
        } else {
            m_selection_model->select_single(0);
        }
    }
    m_scroll_y = 0.0;
    request_redraw();
}

void GridView::clear_items() {
    m_items.clear();
    m_virtual_count = 0;
    m_item_provider = nullptr;
    m_hovered_index = -1;
    if (m_selection_model) {
        m_selection_model->reset(false);
    }
    m_scroll_y = 0.0;
    request_redraw();
}

std::shared_ptr<View> GridView::get_item_at(size_t index) const {
    if (m_item_provider && index < m_virtual_count) {
        return m_item_provider(index);
    }
    if (index < m_items.size()) {
        return m_items[index];
    }
    return nullptr;
}

int GridView::compute_columns(int bounds_w, int& out_cell_w) const {
    if (bounds_w <= 0) {
        out_cell_w = m_cell_w;
        return m_auto_fit ? std::max(1, m_effective_cols) : std::max(1, m_cols);
    }

    int cols = m_cols;
    if (m_auto_fit) {
        cols = std::max(1, (bounds_w + m_space_x) / (m_min_col_w + m_space_x));
    }

    if (m_stretch_mode == StretchMode::ColumnWidth && cols > 0) {
        int total_spacing = (cols - 1) * m_space_x;
        out_cell_w = std::max(20, (bounds_w - total_spacing) / cols);
    } else {
        out_cell_w = m_cell_w;
    }

    m_effective_cols = cols;
    m_effective_cell_w = out_cell_w;
    return cols;
}

void GridView::ensure_visible(int viewport_height, int cols, int row_stride) {
    if (!m_selection_model || viewport_height <= 0 || cols <= 0) return;
    int cur = m_selection_model->cursor();
    if (cur < 0 || cur >= static_cast<int>(get_item_count())) return;

    int row = cur / cols;
    double item_top = row * row_stride;
    double item_bottom = item_top + m_cell_h;

    if (item_top < m_scroll_y) {
        m_scroll_y = item_top;
    } else if (item_bottom > m_scroll_y + viewport_height) {
        m_scroll_y = item_bottom - viewport_height;
    }

    int total_rows = (static_cast<int>(get_item_count()) + cols - 1) / cols;
    double content_h = total_rows * row_stride - m_space_y;
    double max_scroll = std::max(0.0, content_h - viewport_height);
    m_scroll_y = std::clamp(m_scroll_y, 0.0, max_scroll);
}

int GridView::item_at(int lx, int ly, const Rect& bounds) const {
    int cell_w = m_effective_cell_w;
    int cols = compute_columns(bounds.width, cell_w);

    int rel_x = lx - bounds.x;
    int rel_y = ly - bounds.y + static_cast<int>(m_scroll_y);

    if (rel_x < 0 || rel_x >= bounds.width || rel_y < 0) {
        return -1;
    }

    int col = rel_x / (cell_w + m_space_x);
    int row = rel_y / (m_cell_h + m_space_y);

    if (col >= cols) return -1;

    int idx = row * cols + col;
    if (idx >= 0 && idx < static_cast<int>(get_item_count())) {
        return idx;
    }
    return -1;
}

void GridView::draw(cairo_t* cr, const Rect& bounds) {
    if (!is_visible() || !cr || bounds.width <= 0 || bounds.height <= 0) return;

    m_last_width = bounds.width;
    m_last_height = bounds.height;
    auto config = Config::get();

    int cell_w = 0;
    int cols = compute_columns(bounds.width, cell_w);

    size_t count = get_item_count();
    int total_rows = (static_cast<int>(count) + cols - 1) / cols;
    int row_stride = m_cell_h + m_space_y;
    double content_h = std::max(0, total_rows * row_stride - m_space_y);
    double max_scroll = std::max(0.0, content_h - bounds.height);

    if (m_kinetic.step(m_scroll_y, max_scroll)) {
        request_redraw();
    }
    m_scroll_y = std::clamp(m_scroll_y, 0.0, max_scroll);

    cairo_save(cr);
    cairo_rectangle(cr, bounds.x, bounds.y, bounds.width, bounds.height);
    cairo_clip(cr);

    int start_row = std::max(0, static_cast<int>(m_scroll_y / row_stride));
    int end_row = std::min(total_rows, static_cast<int>((m_scroll_y + bounds.height) / row_stride) + 1);

    int start_idx = start_row * cols;
    int end_idx = std::min(end_row * cols, static_cast<int>(count));

    for (int i = start_idx; i < end_idx; ++i) {
        auto item = get_item_at(static_cast<size_t>(i));
        if (!item) continue;

        int grid_row = i / cols;
        int grid_col = i % cols;

        int cell_x = bounds.x + grid_col * (cell_w + m_space_x);
        double cell_y = bounds.y + grid_row * row_stride - m_scroll_y;
        Rect cell_rect(cell_x, static_cast<int>(cell_y), cell_w, m_cell_h);

        bool is_cursor = (m_selection_model && m_selection_model->is_cursor(i));
        bool in_multi_select = (m_selection_model &&
                                m_selection_model->mode() == SelectionMode::Multiple &&
                                m_selection_model->is_marked(i));
        bool is_hovered = (i == m_hovered_index && !is_cursor);

        double cell_radius = static_cast<double>(std::max(0, config->metrics.corner_radius));

        // Tile background (Cursor / Selected / Hovered highlight)
        if (is_cursor) {
            CardView::draw_rounded_rect(cr, cell_x, cell_y, cell_w, m_cell_h, cell_radius);
            cairo_set_source_rgba(cr, config->colors.primary_container.r,
                                      config->colors.primary_container.g,
                                      config->colors.primary_container.b,
                                      0.75f);
            cairo_fill(cr);

            if (m_selection_border) {
                CardView::draw_rounded_rect(cr, cell_x + 0.5, cell_y + 0.5, cell_w - 1.0, m_cell_h - 1.0, cell_radius);
                cairo_set_source_rgba(cr, config->colors.primary.r,
                                          config->colors.primary.g,
                                          config->colors.primary.b,
                                          0.85f);
                cairo_set_line_width(cr, 1.5);
                cairo_stroke(cr);
            }
        } else if (in_multi_select) {
            CardView::draw_rounded_rect(cr, cell_x, cell_y, cell_w, m_cell_h, cell_radius);
            cairo_set_source_rgba(cr, config->colors.primary.r,
                                      config->colors.primary.g,
                                      config->colors.primary.b,
                                      0.14f);
            cairo_fill(cr);

            CardView::draw_rounded_rect(cr, cell_x + 0.5, cell_y + 0.5, cell_w - 1.0, m_cell_h - 1.0, cell_radius);
            cairo_set_source_rgba(cr, config->colors.primary.r,
                                      config->colors.primary.g,
                                      config->colors.primary.b,
                                      0.5f);
            cairo_set_line_width(cr, 1.0);
            cairo_stroke(cr);
        } else if (is_hovered) {
            CardView::draw_rounded_rect(cr, cell_x, cell_y, cell_w, m_cell_h, cell_radius);
            cairo_set_source_rgba(cr, config->colors.on_surface.r,
                                      config->colors.on_surface.g,
                                      config->colors.on_surface.b,
                                      0.08f);
            cairo_fill(cr);
        }

        // Delegate drawing the cell content to the child View!
        item->draw(cr, cell_rect);

        // Visual indicator for marked items in Multiple mode:
        // A check badge at top-right of cell
        if (in_multi_select) {
            double badge_size = 18.0;
            double badge_x = cell_x + cell_w - badge_size - 6.0;
            double badge_y = cell_y + 6.0;
            CardView::draw_rounded_rect(cr, badge_x, badge_y, badge_size, badge_size, badge_size / 2.0);
            cairo_set_source_rgba(cr, config->colors.primary.r,
                                      config->colors.primary.g,
                                      config->colors.primary.b,
                                      0.95f);
            cairo_fill(cr);

            // Draw checkmark inside badge
            cairo_set_source_rgba(cr, config->colors.on_primary.r,
                                      config->colors.on_primary.g,
                                      config->colors.on_primary.b,
                                      1.0f);
            cairo_set_line_width(cr, 1.8);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
            cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
            cairo_move_to(cr, badge_x + 4.5, badge_y + 9.5);
            cairo_line_to(cr, badge_x + 7.5, badge_y + 12.5);
            cairo_line_to(cr, badge_x + 13.5, badge_y + 6.5);
            cairo_stroke(cr);
        }
    }

    // Scrollbar Thumb
    if (max_scroll > 0.0) {
        double track_h = bounds.height;
        double thumb_h = std::max(24.0, (static_cast<double>(bounds.height) / content_h) * track_h);
        double scroll_ratio = m_scroll_y / max_scroll;
        double thumb_y = bounds.y + scroll_ratio * (track_h - thumb_h);
        double thumb_x = bounds.x + bounds.width - 4.0;

        cairo_save(cr);
        CardView::draw_rounded_rect(cr, thumb_x, thumb_y, 4.0, thumb_h, 2.0);
        cairo_set_source_rgba(cr, config->colors.outline.r,
                                  config->colors.outline.g,
                                  config->colors.outline.b,
                                  0.6f);
        cairo_fill(cr);
        cairo_restore(cr);
    }

    cairo_restore(cr);
}

bool GridView::on_mouse_move(int lx, int ly, const Rect& bounds) {
    int old_hover = m_hovered_index;
    m_hovered_index = item_at(lx, ly, bounds);

    if (m_hovered_index != old_hover) {
        request_redraw();
        return true;
    }
    return false;
}

bool GridView::on_mouse_button(int lx, int ly, MouseButton button, bool pressed, const Rect& bounds) {
    int count = static_cast<int>(get_item_count());
    if (button == MouseButton::Left && pressed) {
        int idx = item_at(lx, ly, bounds);
        if (idx >= 0 && idx < count) {
            Window* win = get_window();
            bool is_shift = win && win->has_shift();
            bool is_ctrl = win && win->has_ctrl();

            if (m_on_item_click) {
                m_on_item_click(idx, get_item_at(static_cast<size_t>(idx)));
            }
            if (m_selection_model) {
                m_selection_model->handle_click(idx, is_shift, is_ctrl, count);
            }
            request_redraw();
            return true;
        }
    } else if (button == MouseButton::Right && pressed) {
        int idx = item_at(lx, ly, bounds);
        if (idx >= 0 && idx < count) {
            if (m_selection_model) {
                m_selection_model->handle_context_click(idx);
            }
            if (m_on_item_context_click) {
                m_on_item_context_click(idx, get_item_at(static_cast<size_t>(idx)), lx, ly);
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

bool GridView::on_scroll(double delta, uint32_t axis_source) {
    int cell_w = 0;
    int cols = compute_columns(m_last_width, cell_w);
    int total_rows = (static_cast<int>(get_item_count()) + cols - 1) / cols;
    int row_stride = m_cell_h + m_space_y;
    double content_h = std::max(0, total_rows * row_stride - m_space_y);
    double max_scroll = std::max(0.0, content_h - m_last_height);
    if (max_scroll <= 0.0) return false;

    const double multiplier = (axis_source == WL_POINTER_AXIS_SOURCE_FINGER) ? 4.5 : 30.0;
    const double pixel_delta = delta * multiplier;
    m_kinetic.record_scroll(pixel_delta, axis_source);

    double old_scroll = m_scroll_y;
    m_scroll_y = std::clamp(m_scroll_y + pixel_delta, 0.0, max_scroll);

    if (m_scroll_y != old_scroll) {
        request_redraw();
        return true;
    }
    return false;
}

bool GridView::on_scroll_stop(uint32_t axis_source) {
    int cell_w = 0;
    int cols = compute_columns(m_last_width, cell_w);
    int total_rows = (static_cast<int>(get_item_count()) + cols - 1) / cols;
    int row_stride = m_cell_h + m_space_y;
    double content_h = std::max(0, total_rows * row_stride - m_space_y);
    double max_scroll = std::max(0.0, content_h - m_last_height);
    if (!is_visible() || max_scroll <= 0.0) {
        m_kinetic.cancel();
        return false;
    }
    if (m_kinetic.start_fling(axis_source)) {
        request_redraw();
        return true;
    }
    return false;
}

bool GridView::on_key(const KeyPressEvent& event) {
    if (!m_keyboard_navigation_enabled || !m_selection_model) return false;
    int total = static_cast<int>(get_item_count());
    if (!event.pressed || total == 0) return false;

    // Ctrl+A: Select All
    if (m_selection_model->mode() == SelectionMode::Multiple &&
        event.has_ctrl() && (event.keysym == XKB_KEY_a || event.keysym == XKB_KEY_A)) {
        if (m_selection_model->handle_select_all(total)) {
            request_redraw();
            return true;
        }
    }

    // Escape: Clear marks
    if (event.keysym == XKB_KEY_Escape) {
        if (m_selection_model->handle_escape()) {
            request_redraw();
            return true;
        }
        return false;
    }

    int cell_w = 0;
    int cols = compute_columns(m_last_width, cell_w);
    int old_cursor = m_selection_model->cursor();

    // Space: Toggle & step
    if (event.keysym == XKB_KEY_space) {
        if (m_selection_model->handle_space(total, true)) {
            int row_stride = m_cell_h + m_space_y;
            ensure_visible(m_last_height, cols, row_stride);
            request_redraw();
            return true;
        }
        return false;
    }

    // Return / KP_Enter
    if (event.keysym == XKB_KEY_Return || event.keysym == XKB_KEY_KP_Enter) {
        if (old_cursor >= 0 && old_cursor < total && m_on_item_click) {
            m_on_item_click(old_cursor, get_item_at(static_cast<size_t>(old_cursor)));
            return true;
        }
        return false;
    }

    int target = old_cursor;

    // Navigation keys
    switch (event.keysym) {
        case XKB_KEY_Left:
        case XKB_KEY_h:
        case XKB_KEY_H:
            if (old_cursor > 0) target = old_cursor - 1;
            break;

        case XKB_KEY_Right:
        case XKB_KEY_l:
        case XKB_KEY_L:
            if (old_cursor < total - 1) target = old_cursor + 1;
            break;

        case XKB_KEY_Up:
        case XKB_KEY_k:
        case XKB_KEY_K:
            if (old_cursor - cols >= 0) target = old_cursor - cols;
            break;

        case XKB_KEY_Down:
        case XKB_KEY_j:
        case XKB_KEY_J:
            if (old_cursor + cols < total) target = old_cursor + cols;
            else if (old_cursor < total - 1) target = total - 1;
            break;

        case XKB_KEY_Page_Up: {
            int page_rows = std::max(1, m_last_height / (m_cell_h + m_space_y));
            target = std::max(0, old_cursor - page_rows * cols);
            break;
        }

        case XKB_KEY_Page_Down: {
            int page_rows = std::max(1, m_last_height / (m_cell_h + m_space_y));
            target = std::min(total - 1, old_cursor + page_rows * cols);
            break;
        }

        case XKB_KEY_Home:
            target = 0;
            break;

        case XKB_KEY_End:
            target = total - 1;
            break;

        case XKB_KEY_Tab:
            if (event.has_shift()) {
                target = (old_cursor > 0) ? old_cursor - 1 : total - 1;
            } else {
                target = (old_cursor < total - 1) ? old_cursor + 1 : 0;
            }
            break;

        default:
            return false;
    }

    if (target != old_cursor) {
        m_selection_model->handle_nav(target, event.has_shift());
        int row_stride = m_cell_h + m_space_y;
        ensure_visible(m_last_height, cols, row_stride);
        request_redraw();
        return true;
    }

    return false;
}

bool GridView::on_touch(const TouchEvent& event, const Rect& bounds) {
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
        request_redraw();
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
            int cell_w = 0;
            int cols = compute_columns(m_last_width, cell_w);
            int total_rows = (static_cast<int>(get_item_count()) + cols - 1) / cols;
            int row_stride = m_cell_h + m_space_y;
            double content_h = total_rows * row_stride - m_space_y;
            double max_scroll = std::max(0.0, content_h - m_last_height);
            if (max_scroll > 0.0) {
                double old_scroll = m_scroll_y;
                m_scroll_y = std::clamp(m_scroll_y - dy, 0.0, max_scroll);
                if (m_scroll_y != old_scroll) {
                    request_redraw();
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
            request_redraw();
            return true;
        }

        m_touch_scrolling = false;
        int idx = item_at(static_cast<int>(event.x), static_cast<int>(event.y), bounds);
        if (idx >= 0 && idx < static_cast<int>(get_item_count())) {
            set_selected_index(idx);
            m_hovered_index = idx;
            if (m_on_item_click) {
                m_on_item_click(idx, get_item_at(static_cast<size_t>(idx)));
            }
            request_redraw();
            return true;
        }
        return true;
    }

    if (event.phase == TouchPhase::Cancel) {
        m_touch_id = -1;
        m_touch_scrolling = false;
        m_hovered_index = -1;
        request_redraw();
        return true;
    }

    return false;
}

} // namespace miqu
