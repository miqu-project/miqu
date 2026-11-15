#include "miqutoolkit/view/search_view.hpp"
#include "miqutoolkit/view/card_view.hpp"
#include "miqutoolkit/core/config.hpp"
#include "miqutoolkit/core/window.hpp"
#include <xkbcommon/xkbcommon-keysyms.h>
#include <algorithm>
#include <cmath>

namespace miqu {

SearchView::SearchView() {
    set_focusable(true);
    set_draw_focus_ring(true);

    m_title_view = TextViewBuilder::create()->bold(true)->textSize(12)->build();
    m_title_view->set_focusable(false);

    m_edit_text = EditTextBuilder::create()->focused(false)->padding(0, 0)->drawBackground(false)->build();
    m_edit_text->set_focusable(false);
    m_edit_text->set_draw_focus_ring(false);

    m_edit_text->set_on_text_changed_listener([this](auto, const std::string& text) {
        if (m_on_text_change) {
            m_on_text_change(text);
        }
    });

    add_view(m_title_view);
    add_view(m_edit_text);
}

void SearchView::set_title(std::string title) {
    m_title = std::move(title);
    if (m_title_view) {
        m_title_view->set_text(m_title);
    }
}

void SearchView::set_hint(std::string hint) {
    if (m_edit_text) {
        m_edit_text->set_hint(std::move(hint));
    }
}

const std::string& SearchView::get_hint() const {
    static const std::string empty;
    return m_edit_text ? m_edit_text->get_hint() : empty;
}

void SearchView::set_query(std::string query) {
    if (m_edit_text) {
        m_edit_text->set_text(std::move(query));
    }
}

const std::string& SearchView::get_query() const {
    static const std::string empty;
    return m_edit_text ? m_edit_text->get_text() : empty;
}

void SearchView::set_focused(bool focus) {
    if (focus) {
        if (is_focusable()) {
            if (auto* w = get_window()) {
                w->set_focused_view(this);
            }
            if (m_edit_text) {
                m_edit_text->set_focused(true);
            }
        }
    } else {
        if (has_focus()) {
            clear_focus();
        }
        if (m_edit_text) {
            m_edit_text->set_focused(false);
        }
    }
    request_redraw();
}

bool SearchView::is_focused() const {
    return has_focus() || (m_edit_text && m_edit_text->is_focused());
}

void SearchView::on_focus_changed(bool focused) {
    ViewGroup::on_focus_changed(focused);
    if (m_edit_text) {
        m_edit_text->set_focused(focused);
    }
    request_redraw();
}

void SearchView::set_on_query_text_listener(std::function<void(const std::string&)> on_change,
                                            std::function<void(const std::string&)> on_submit) {
    m_on_text_change = std::move(on_change);
    m_on_text_submit = std::move(on_submit);
    if (m_edit_text) {
        m_edit_text->set_on_submit_listener([this](const std::string& text) {
            if (m_on_text_submit) {
                m_on_text_submit(text);
            }
        });
    }
}

Size SearchView::measure_size() const {
    int pad_h = (m_padding.left > 0 || m_padding.right > 0) ? (m_padding.left + m_padding.right) : 44;
    int pad_v = (m_padding.top > 0 || m_padding.bottom > 0) ? (m_padding.top + m_padding.bottom) : 16;
    return Size(m_bounds.width > 0 ? m_bounds.width : 200 + pad_h, 24 + pad_v);
}

void SearchView::draw(cairo_t* cr, const Rect& bounds) {
    m_bounds = bounds;
    if (!is_visible() || !cr || bounds.width <= 0 || bounds.height <= 0) return;

    auto config = Config::get();
    m_child_entries.clear();

    // 1. Calculate Content Bounds
    int draw_x = bounds.x;
    int draw_y = bounds.y;
    int draw_w = bounds.width;
    int draw_h = bounds.height;

    if (draw_w <= 0 || draw_h <= 0) return;

    int radius = (m_corner_radius >= 0) ? m_corner_radius : (draw_h / 2);

    // 1b. Ambient drop shadow if elevated
    if (m_elevation > 0.0) {
        CardView::draw_elevation_shadow(cr, draw_x, draw_y, draw_w, draw_h, radius, m_elevation);
    }

    cairo_save(cr);

    // 2. Draw Unified SearchView Pill Surface
    CardView::draw_rounded_rect(cr, draw_x, draw_y, draw_w, draw_h, radius);
    if (m_has_custom_bg) {
        cairo_set_source_rgba(cr, m_bg_color.r, m_bg_color.g, m_bg_color.b, m_bg_color.a);
    } else {
        cairo_set_source_rgba(cr, config->colors.surface_variant.r,
                                  config->colors.surface_variant.g,
                                  config->colors.surface_variant.b,
                                  config->colors.surface_variant.a);
    }
    cairo_fill(cr);

    // 3. Focused Outline / Stroke
    if (is_focused() && m_focus_border && should_draw_focus_ring()) {
        draw_focus_ring(cr, bounds);
    } else if (m_stroke_width > 0 && m_stroke_color.a > 0.0f) {
        double offset = m_stroke_width / 2.0;
        CardView::draw_rounded_rect(cr, draw_x + offset, draw_y + offset, draw_w - m_stroke_width, draw_h - m_stroke_width, std::max(0.0, radius - offset));
        cairo_set_source_rgba(cr, m_stroke_color.r, m_stroke_color.g, m_stroke_color.b, m_stroke_color.a);
        cairo_set_line_width(cr, m_stroke_width);
        cairo_stroke(cr);
    }

    cairo_restore(cr);

    // 4. Inset by Padding
    int pad_l = m_padding.left > 0 ? m_padding.left : 22;
    int pad_r = m_padding.right > 0 ? m_padding.right : 22;
    int pad_t = m_padding.top > 0 ? m_padding.top : 8;
    int pad_b = m_padding.bottom > 0 ? m_padding.bottom : 8;

    int content_x = draw_x + pad_l;
    int content_y = draw_y + pad_t;
    int content_w = std::max(0, draw_w - pad_l - pad_r);
    int content_h = std::max(0, draw_h - pad_t - pad_b);

    if (content_w <= 0 || content_h <= 0) return;

    int current_x = content_x;

    // 5. Draw Left Title/Label if present
    if (!m_title.empty() && m_title_view) {
        m_title_view->set_text_color(config->colors.primary);
        Size title_size = m_title_view->measure_size();
        int title_w = title_size.width;
        int title_h = title_size.height;
        int title_y = content_y + (content_h - title_h) / 2;

        Rect title_rect(current_x, title_y, title_w, title_h);
        m_child_entries.push_back({m_title_view, title_rect});
        m_title_view->draw(cr, title_rect);

        current_x += title_w + 14; // Generous breathing gap between title and input field
    }

    // 6. Draw Inner EditText spanning the rest of the right side
    if (m_edit_text) {
        int text_w = std::max(0, content_x + content_w - current_x);
        Rect text_rect(current_x, content_y, text_w, content_h);

        m_child_entries.push_back({m_edit_text, text_rect});
        m_edit_text->draw(cr, text_rect);
    }
}

void SearchView::draw_focus_ring(cairo_t* cr, const Rect& bounds) {
    if (!should_draw_focus_ring() || !m_focus_border || !cr || bounds.width <= 0 || bounds.height <= 0) return;
    auto config = Config::get();
    Color focus_color = config ? config->colors.primary : Color(0.2f, 0.6f, 1.0f);

    cairo_save(cr);
    cairo_set_source_rgba(cr, focus_color.r, focus_color.g, focus_color.b, 0.90f);
    cairo_set_line_width(cr, 2.0);

    // Floating focus ring with 2.5px outset breathing gap outside the search pill
    double gap = 2.5;
    double x = bounds.x - gap;
    double y = bounds.y - gap;
    double w = bounds.width + (gap * 2.0);
    double h = bounds.height + (gap * 2.0);
    double base_r = (m_corner_radius >= 0) ? static_cast<double>(m_corner_radius) : (bounds.height / 2.0);
    double r = base_r + gap;

    CardView::draw_rounded_rect(cr, x, y, w, h, r);
    cairo_stroke(cr);
    cairo_restore(cr);

    if (m_window) {
        m_window->notify_focus_ring_drawn();
    }
}

bool SearchView::on_key(const KeyPressEvent& event) {
    if (!is_visible() || !is_enabled()) return false;
    bool has_any_focus = (has_focus() || is_focused() || (m_edit_text && m_edit_text->is_focused()));
    if (!has_any_focus) {
        return false;
    }

    if (event.pressed && (event.keysym == XKB_KEY_Return || event.keysym == XKB_KEY_KP_Enter)) {
        if (m_on_text_submit && m_edit_text) {
            m_on_text_submit(m_edit_text->get_text());
            return true;
        }
    }

    if (m_edit_text) {
        return m_edit_text->on_key(event);
    }
    return false;
}

bool SearchView::on_mouse_button(int lx, int ly, MouseButton button, bool pressed, const Rect& bounds) {
    if (!is_visible() || !is_enabled()) return false;

    int draw_x = bounds.x;
    int draw_y = bounds.y;
    int draw_w = bounds.width;
    int draw_h = bounds.height;
    Rect pill_rect(draw_x, draw_y, draw_w, draw_h);

    if (button == MouseButton::Left && pressed) {
        if (pill_rect.contains(lx, ly)) {
            if (is_focusable()) {
                if (auto* w = get_window()) {
                    w->set_focused_view(this);
                } else {
                    set_focused(true);
                }
                if (m_edit_text) {
                    for (const auto& entry : m_child_entries) {
                        if (entry.view == m_edit_text) {
                            if (entry.allocated_bounds.contains(lx, ly)) {
                                m_edit_text->on_mouse_button(lx, ly, button, pressed, entry.allocated_bounds);
                            } else if (lx < entry.allocated_bounds.x) {
                                m_edit_text->on_mouse_button(entry.allocated_bounds.x, entry.allocated_bounds.y + 1, button, pressed, entry.allocated_bounds);
                            } else {
                                m_edit_text->on_mouse_button(entry.allocated_bounds.x + entry.allocated_bounds.width - 1, entry.allocated_bounds.y + 1, button, pressed, entry.allocated_bounds);
                            }
                            break;
                        }
                    }
                }
            }
            return true;
        } else {
            if (has_focus()) {
                clear_focus();
            }
        }
    }
    return false;
}

bool SearchView::on_mouse_move(int lx, int ly, const Rect& bounds) {
    return ViewGroup::on_mouse_move(lx, ly, bounds);
}

void SearchView::set_focusable(bool focusable) {
    View::set_focusable(focusable);
    if (!focusable) {
        if (has_focus()) {
            clear_focus();
        }
        if (m_edit_text) {
            m_edit_text->set_focused(false);
        }
    }
}

void SearchView::collect_focusable_views(std::vector<View*>& out) {
    if (!is_visible() || !is_enabled() || !is_focusable()) return;
    out.push_back(this);
}

} // namespace miqu
