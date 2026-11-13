#include "miqutoolkit/view/tab_bar.hpp"
#include "miqutoolkit/view/card_view.hpp"
#include "miqutoolkit/core/config.hpp"
#include "miqutoolkit/core/window.hpp"
#include <pango/pangocairo.h>
#include <algorithm>
#include <iostream>

namespace miqu {

// ============================================================================
// TabItemView
// ============================================================================

TabItemView::TabItemView(TabBar* parent_bar, int index, std::string text)
    : m_parent_bar(parent_bar), m_index(index), m_text(std::move(text)) {
    m_focusable = true;
    m_draw_focus_ring = true;
}

void TabItemView::set_text(const std::string& text) {
    m_text = text;
    request_redraw();
}

Size TabItemView::measure_size() const {
    auto config = Config::get();
    int font_sz = 11;
    if (m_parent_bar && m_parent_bar->get_font_size() > 0) {
        font_sz = m_parent_bar->get_font_size();
    } else if (config && config->metrics.font_size > 0) {
        font_sz = config->metrics.font_size;
    }
    int w = static_cast<int>(m_text.size() * (font_sz * 0.65f)) + 24;
    int h = font_sz + 18;
    return Size(w, h);
}

void TabItemView::draw(cairo_t* cr, const Rect& bounds) {
    if (!is_visible() || !cr || bounds.width <= 0 || bounds.height <= 0 || !m_parent_bar) return;

    auto config = Config::get();
    int radius = (m_parent_bar->get_corner_radius() >= 0) ? m_parent_bar->get_corner_radius()
                 : (config ? config->metrics.corner_radius : 12);
    int inner_radius = std::max(2, radius - 2);

    bool is_selected = (m_index == m_parent_bar->get_selected_index());
    bool is_hovered = m_hovered;
    auto style = m_parent_bar->get_style();

    cairo_save(cr);

    // Active selection pill or underline
    if (style == TabBarStyle::Segmented) {
        if (is_selected) {
            CardView::draw_rounded_rect(cr, bounds.x, bounds.y, bounds.width, bounds.height, inner_radius);
            cairo_set_source_rgba(cr, config->colors.primary_container.r,
                                      config->colors.primary_container.g,
                                      config->colors.primary_container.b,
                                      config->colors.primary_container.a);
            cairo_fill(cr);

            CardView::draw_rounded_rect(cr, bounds.x + 0.5, bounds.y + 0.5, bounds.width - 1.0, bounds.height - 1.0, std::max(0, inner_radius - 1));
            cairo_set_source_rgba(cr, config->colors.primary.r,
                                      config->colors.primary.g,
                                      config->colors.primary.b,
                                      0.35f);
            cairo_set_line_width(cr, 1.0);
            cairo_stroke(cr);
        } else if (is_hovered) {
            CardView::draw_rounded_rect(cr, bounds.x, bounds.y, bounds.width, bounds.height, inner_radius);
            cairo_set_source_rgba(cr, config->colors.surface_variant.r,
                                      config->colors.surface_variant.g,
                                      config->colors.surface_variant.b,
                                      0.50f);
            cairo_fill(cr);
        }
    } else { // Underline style
        if (is_selected) {
            int line_h = 3;
            cairo_rectangle(cr, bounds.x + 8, bounds.y + bounds.height - line_h, bounds.width - 16, line_h);
            cairo_set_source_rgba(cr, config->colors.primary.r,
                                      config->colors.primary.g,
                                      config->colors.primary.b,
                                      config->colors.primary.a);
            cairo_fill(cr);
        }
    }

    // Tab Label text
    PangoLayout* layout = pango_cairo_create_layout(cr);
    pango_layout_set_text(layout, m_text.c_str(), -1);

    int font_sz = (m_parent_bar->get_font_size() > 0) ? m_parent_bar->get_font_size()
                  : (config ? config->metrics.font_size : 11);
    if (font_sz <= 0) font_sz = 11;
    std::string font_family = (config && !config->metrics.font_family.empty()) ? config->metrics.font_family : "Sans";

    std::string font_desc = font_family + " " + std::to_string(font_sz);
    if (is_selected && m_parent_bar->is_bold_active()) font_desc += " Bold";
    PangoFontDescription* desc = pango_font_description_from_string(font_desc.c_str());
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);

    pango_layout_set_width(layout, std::max(1, (bounds.width - 8)) * PANGO_SCALE);
    pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);

    int text_w = 0, text_h = 0;
    pango_layout_get_pixel_size(layout, &text_w, &text_h);

    double label_x = bounds.x + 4.0;
    double label_y = bounds.y + (bounds.height - text_h) / 2.0;

    cairo_move_to(cr, label_x, label_y);
    Color fg = is_selected ? (config ? config->colors.on_primary_container : Color(1, 1, 1))
                           : (config ? config->colors.on_surface_variant : Color(0.7f, 0.7f, 0.7f));
    cairo_set_source_rgba(cr, fg.r, fg.g, fg.b, fg.a);
    pango_cairo_show_layout(cr, layout);

    g_object_unref(layout);
    cairo_restore(cr);
}

void TabItemView::draw_focus_ring(cairo_t* cr, const Rect& bounds) {
    if (!m_draw_focus_ring || !cr || bounds.width <= 0 || bounds.height <= 0) return;
    auto config = Config::get();
    int radius = (m_parent_bar && m_parent_bar->get_corner_radius() >= 0) ? m_parent_bar->get_corner_radius()
                 : (config ? config->metrics.corner_radius : 12);
    int inner_radius = (m_parent_bar && m_parent_bar->get_style() == TabBarStyle::Underline)
                       ? 4 : std::max(2, radius - 2);

    cairo_save(cr);
    Color focus_color = config ? config->colors.primary : Color(0.2f, 0.6f, 1.0f);
    cairo_set_source_rgba(cr, focus_color.r, focus_color.g, focus_color.b, 0.90f);
    cairo_set_line_width(cr, 2.0);

    double offset = 1.0;
    CardView::draw_rounded_rect(cr, bounds.x + offset, bounds.y + offset,
                                bounds.width - (offset * 2.0), bounds.height - (offset * 2.0),
                                inner_radius);
    cairo_stroke(cr);
    cairo_restore(cr);

    if (m_window) {
        m_window->notify_focus_ring_drawn();
    }
}

bool TabItemView::on_mouse_move(int lx, int ly, const Rect& bounds) {
    if (!is_visible()) return false;
    bool was_hovered = m_hovered;
    m_hovered = bounds.contains(Point(lx, ly));
    if (was_hovered != m_hovered) {
        request_redraw();
    }
    return m_hovered;
}

bool TabItemView::on_mouse_button(int lx, int ly, MouseButton button, bool pressed, const Rect& bounds) {
    if (!is_visible() || !is_enabled()) return false;
    if (button != MouseButton::Left) return false;

    bool contains = bounds.contains(Point(lx, ly));
    if (pressed) {
        if (!contains) return false;
        m_pressed = true;
        request_redraw();
        return true;
    } else {
        bool was_pressed = m_pressed;
        m_pressed = false;
        if (was_pressed && contains) {
            if (m_parent_bar) {
                m_parent_bar->set_selected_index(m_index);
            }
            request_focus();
            request_redraw();
            return true;
        }
    }
    return contains;
}

bool TabItemView::on_key(const KeyPressEvent& event) {
    if (!is_visible() || !is_enabled()) return false;
    if (m_window && !has_focus()) return false;
    if (event.pressed && (event.keysym == XKB_KEY_space || event.keysym == XKB_KEY_Return || event.keysym == XKB_KEY_KP_Enter)) {
        if (m_parent_bar) {
            m_parent_bar->set_selected_index(m_index);
        }
        request_redraw();
        return true;
    }
    return false;
}

// ============================================================================
// TabBar
// ============================================================================

TabBar::TabBar() {
    m_padding = Padding(4, 4, 4, 4);
    set_focusable(false); // TabBar container is not focused directly; children TabItemViews are focused
}

void TabBar::set_tabs(const std::vector<std::string>& tabs) {
    m_tabs = tabs;
    clear_views();
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        auto item = std::make_shared<TabItemView>(this, static_cast<int>(i), m_tabs[i]);
        if (m_window) item->set_window(m_window);
        add_view(item);
    }
    if (m_selected_index >= static_cast<int>(m_tabs.size())) {
        m_selected_index = m_tabs.empty() ? 0 : static_cast<int>(m_tabs.size()) - 1;
    }
    if (m_window) m_window->schedule_redraw();
}

void TabBar::add_tab(const std::string& label) {
    int idx = static_cast<int>(m_tabs.size());
    m_tabs.push_back(label);
    auto item = std::make_shared<TabItemView>(this, idx, label);
    if (m_window) item->set_window(m_window);
    add_view(item);
    if (m_window) m_window->schedule_redraw();
}

void TabBar::set_tab_text(int index, const std::string& text) {
    if (index >= 0 && index < static_cast<int>(m_tabs.size())) {
        m_tabs[index] = text;
        if (index < static_cast<int>(m_children.size())) {
            if (auto item = std::dynamic_pointer_cast<TabItemView>(m_children[index])) {
                item->set_text(text);
            }
        }
        if (m_window) m_window->schedule_redraw();
    }
}

void TabBar::set_selected_index(int index) {
    if (index >= 0 && index < static_cast<int>(m_tabs.size()) && index != m_selected_index) {
        m_selected_index = index;
        if (m_on_tab_selected) {
            m_on_tab_selected(m_selected_index);
        }
        if (m_window) m_window->schedule_redraw();
    }
}

Size TabBar::measure_size() const {
    auto config = Config::get();
    int font_sz = (m_font_size > 0) ? m_font_size : (config ? config->metrics.font_size : 11);
    if (font_sz <= 0) font_sz = 11;

    int total_w = m_padding.left + m_padding.right;
    for (const auto& tab : m_tabs) {
        total_w += static_cast<int>(tab.size() * (font_sz * 0.65f)) + 24;
    }
    int h = font_sz + 18 + m_padding.top + m_padding.bottom;
    return Size(std::max(120, total_w), std::max(34, h));
}

void TabBar::draw(cairo_t* cr, const Rect& bounds) {
    if (!is_visible() || !cr || bounds.width <= 0 || bounds.height <= 0 || m_tabs.empty()) return;

    auto config = Config::get();
    Rect content_rect = get_content_rect(bounds);
    if (content_rect.width <= 0 || content_rect.height <= 0) return;

    int radius = (m_corner_radius >= 0) ? m_corner_radius : (config ? config->metrics.corner_radius : 12);

    cairo_save(cr);

    // 1. Segmented background track
    if (m_style == TabBarStyle::Segmented) {
        CardView::draw_rounded_rect(cr, bounds.x, bounds.y, bounds.width, bounds.height, radius);
        cairo_set_source_rgba(cr, config->colors.surface_variant.r,
                                  config->colors.surface_variant.g,
                                  config->colors.surface_variant.b,
                                  0.45f);
        cairo_fill(cr);

        CardView::draw_rounded_rect(cr, bounds.x + 0.5, bounds.y + 0.5, bounds.width - 1.0, bounds.height - 1.0, std::max(0, radius - 1));
        cairo_set_source_rgba(cr, config->colors.outline_variant.r,
                                  config->colors.outline_variant.g,
                                  config->colors.outline_variant.b,
                                  0.60f);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);
    }

    cairo_restore(cr);

    // 2. Measure and position children
    m_child_entries.clear();
    size_t count = m_children.size();
    if (count == 0) return;

    int tab_w = content_rect.width / static_cast<int>(count);
    for (size_t i = 0; i < count; ++i) {
        auto child = m_children[i];
        if (!child) continue;

        int tx = content_rect.x + static_cast<int>(i) * tab_w;
        int tw = (i == count - 1) ? (content_rect.x + content_rect.width - tx) : tab_w;
        int ty = content_rect.y;
        int th = content_rect.height;

        Rect item_bounds(tx, ty, tw, th);
        m_child_entries.push_back({child, item_bounds});
        child->set_bounds(item_bounds);
        child->draw(cr, item_bounds);
    }
}

TabBarBuilder::TabBarBuilder() : m_view(std::make_shared<TabBar>()) {}

} // namespace miqu
