#include "miqutoolkit/view/edit_text.hpp"
#include "miqutoolkit/view/card_view.hpp"
#include "miqutoolkit/view/scroll_view.hpp"
#include "miqutoolkit/core/config.hpp"
#include "miqutoolkit/core/window.hpp"
#include <pango/pangocairo.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <algorithm>
namespace miqu {

void EditText::set_window(Window* win) {
    View::set_window(win);
    if (win && m_focused && is_focusable()) {
        win->set_focused_view(this);
    }
}

void EditText::set_editable(bool editable) {
    if (m_editable != editable) {
        m_editable = editable;
        if (!m_editable && has_focus()) {
            clear_focus();
        }
        request_redraw();
    }
}

void EditText::set_focused(bool focus) {
    if (focus && !m_editable) return;
    if (m_focused != focus) {
        m_focused = focus;
        if (focus) {
            m_needs_scroll_to_cursor = true;
            if (is_focusable()) {
                request_focus();
            }
        } else {
            if (has_focus()) {
                clear_focus();
            }
        }
        request_redraw();
    }
}

void EditText::on_focus_changed(bool focused) {
    if (m_focused != focused) {
        m_focused = focused;
        if (focused) {
            m_needs_scroll_to_cursor = true;
        }
        request_redraw();
    }
}

void EditText::set_text(std::string text) {
    m_text = std::move(text);
    m_cursor_pos = static_cast<int>(m_text.size());
    m_needs_scroll_to_cursor = true;
}

void EditText::clear() {
    m_text.clear();
    m_cursor_pos = 0;
}

Size EditText::measure_size() const {
    return measure_size(-1);
}

Size EditText::measure_size(int avail_width) const {
    if (m_multiline) {
        int pad_left = m_padding.left > 0 ? m_padding.left : (m_draw_background ? 14 : 0);
        int pad_right = m_padding.right > 0 ? m_padding.right : (m_draw_background ? 14 : 0);
        int pad_top = m_padding.top > 0 ? m_padding.top : (m_draw_background && m_multiline ? 12 : 0);
        int pad_bottom = m_padding.bottom > 0 ? m_padding.bottom : (m_draw_background && m_multiline ? 12 : 0);

        int target_w = -1;
        if (m_layout_params.width >= 0) {
            target_w = m_layout_params.width - pad_left - pad_right;
        } else if (avail_width >= 0) {
            target_w = avail_width - pad_left - pad_right;
        }

        std::string display_text = m_text.empty() ? m_hint : m_text;
        if (display_text.empty()) {
            return Size(std::max(100, avail_width), std::max(240, 22 + pad_top + pad_bottom));
        }

        cairo_surface_t* temp_surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
        cairo_t* cr = cairo_create(temp_surf);
        PangoLayout* layout = pango_cairo_create_layout(cr);
        pango_layout_set_text(layout, display_text.c_str(), -1);

        auto config = Config::get();
        std::string font_family = !m_font_family.empty() ? m_font_family :
            (m_multiline ? "Monospace" : (!config->metrics.font_family.empty() ? config->metrics.font_family : "Sans"));
        int font_size = m_font_size > 0 ? m_font_size : (config->metrics.font_size > 0 ? config->metrics.font_size : 11);
        std::string font_desc_str = font_family + " " + std::to_string(font_size);
        PangoFontDescription* desc = pango_font_description_from_string(font_desc_str.c_str());
        pango_layout_set_font_description(layout, desc);
        pango_font_description_free(desc);

        if (target_w > 0) {
            pango_layout_set_width(layout, target_w * PANGO_SCALE);
            pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
        }

        int text_w = 0, text_h = 0;
        pango_layout_get_pixel_size(layout, &text_w, &text_h);

        g_object_unref(layout);
        cairo_destroy(cr);
        cairo_surface_destroy(temp_surf);

        int total_w = text_w + pad_left + pad_right;
        int total_h = text_h + pad_top + pad_bottom;
        return Size(total_w, std::max(240, total_h));
    }
    int pad_v = (m_padding.top + m_padding.bottom > 0) ? (m_padding.top + m_padding.bottom) : 16;
    return Size(m_bounds.width, 22 + pad_v);
}

void EditText::draw(cairo_t* cr, const Rect& bounds) {
    m_bounds = bounds;
    if (!is_visible() || !cr || bounds.width <= 0 || bounds.height <= 0) return;

    auto config = Config::get();

    int draw_x = bounds.x;
    int draw_y = bounds.y;
    int draw_w = bounds.width;
    int draw_h = bounds.height;

    if (draw_w <= 0 || draw_h <= 0) return;

    cairo_save(cr);
    cairo_rectangle(cr, draw_x, draw_y, draw_w, draw_h);
    cairo_clip(cr);

    double radius = static_cast<double>(std::max(0, config->metrics.corner_radius));

    if (m_draw_background) {
        // Background input pill / card
        CardView::draw_rounded_rect(cr, draw_x, draw_y, draw_w, draw_h, radius);
        Color bg_col = m_has_custom_bg_color ? m_bg_color : (m_editable ? config->colors.surface_variant : config->colors.surface_variant.with_alpha(0.35f));
        cairo_set_source_rgba(cr, bg_col.r,
                                  bg_col.g,
                                  bg_col.b,
                                  bg_col.a);
        cairo_fill(cr);

        // Border / outline
        if (!m_editable) {
            CardView::draw_rounded_rect(cr, draw_x + 0.5, draw_y + 0.5, draw_w - 1.0, draw_h - 1.0, radius);
            cairo_set_source_rgba(cr, config->colors.outline.r,
                                      config->colors.outline.g,
                                      config->colors.outline.b,
                                      0.2f);
            cairo_set_line_width(cr, 1.0);
            cairo_stroke(cr);
        } else if (m_focused && should_draw_focus_ring()) {
            CardView::draw_rounded_rect(cr, draw_x + 0.5, draw_y + 0.5, draw_w - 1.0, draw_h - 1.0, radius);
            cairo_set_source_rgba(cr, config->colors.primary.r,
                                      config->colors.primary.g,
                                      config->colors.primary.b,
                                      0.9f);
            cairo_set_line_width(cr, 1.5);
            cairo_stroke(cr);
            if (m_window) m_window->notify_focus_ring_drawn();
        }
    }

    // Text content or placeholder
    bool is_hint = m_text.empty();
    std::string display_text;
    int pango_cursor_index = m_cursor_pos;

    if (is_hint) {
        display_text = m_hint;
    } else if (m_password_mode) {
        display_text = "";
        for (size_t i = 0; i < m_text.size(); ++i) {
            display_text += "\xe2\x80\xa2"; // UTF-8 for bullet • (U+2022)
        }
        pango_cursor_index = m_cursor_pos * 3;
    } else {
        display_text = m_text;
    }

    PangoLayout* layout = pango_cairo_create_layout(cr);
    pango_layout_set_text(layout, display_text.c_str(), -1);

    std::string font_family = !m_font_family.empty() ? m_font_family :
        (m_multiline ? "Monospace" : (!config->metrics.font_family.empty() ? config->metrics.font_family : "Sans"));
    int font_size = m_font_size > 0 ? m_font_size : (config->metrics.font_size > 0 ? config->metrics.font_size : 11);
    std::string font_desc_str = font_family + " " + std::to_string(font_size);
    PangoFontDescription* desc = pango_font_description_from_string(font_desc_str.c_str());
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);

    int pad_left = m_padding.left > 0 ? m_padding.left : (m_draw_background ? 14 : 0);
    int pad_right = m_padding.right > 0 ? m_padding.right : (m_draw_background ? 14 : 0);
    int pad_top = m_padding.top > 0 ? m_padding.top : (m_draw_background && m_multiline ? 12 : 0);
    int pad_bottom = m_padding.bottom > 0 ? m_padding.bottom : (m_draw_background && m_multiline ? 12 : 0);
    int avail_text_w = std::max(0, draw_w - pad_left - pad_right);
    int avail_text_h = std::max(0, draw_h - pad_top - pad_bottom);

    pango_layout_set_width(layout, avail_text_w * PANGO_SCALE);
    if (!m_multiline) {
        pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
    } else {
        pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
    }

    int text_w = 0, text_h = 0;
    pango_layout_get_pixel_size(layout, &text_w, &text_h);

    double text_draw_x = draw_x + pad_left;
    double text_draw_y = m_multiline ? (draw_y + pad_top) : (draw_y + pad_top + (avail_text_h - text_h) / 2.0);

    cairo_move_to(cr, text_draw_x, text_draw_y);

    if (is_hint) {
        if (m_has_custom_hint_color) {
            cairo_set_source_rgba(cr, m_hint_color.r,
                                      m_hint_color.g,
                                      m_hint_color.b,
                                      m_hint_color.a);
        } else {
            cairo_set_source_rgba(cr, config->colors.on_surface_variant.r,
                                      config->colors.on_surface_variant.g,
                                      config->colors.on_surface_variant.b,
                                      0.6f);
        }
    } else {
        if (!m_editable) {
            cairo_set_source_rgba(cr, config->colors.on_surface_variant.r,
                                      config->colors.on_surface_variant.g,
                                      config->colors.on_surface_variant.b,
                                      0.7f);
        } else if (m_has_custom_text_color) {
            cairo_set_source_rgba(cr, m_text_color.r,
                                      m_text_color.g,
                                      m_text_color.b,
                                      m_text_color.a);
        } else {
            cairo_set_source_rgba(cr, config->colors.on_surface.r,
                                      config->colors.on_surface.g,
                                      config->colors.on_surface.b,
                                      config->colors.on_surface.a);
        }
    }

    pango_cairo_show_layout(cr, layout);

    // Draw Cursor
    if (m_editable && m_focused && !is_hint) {
        PangoRectangle strong_pos;
        pango_layout_get_cursor_pos(layout, pango_cursor_index, &strong_pos, nullptr);

        double cur_x = text_draw_x + static_cast<double>(strong_pos.x) / PANGO_SCALE;
        double cur_y = text_draw_y + static_cast<double>(strong_pos.y) / PANGO_SCALE;
        double cur_h = static_cast<double>(strong_pos.height) / PANGO_SCALE;

        cairo_set_source_rgba(cr, config->colors.primary.r,
                                  config->colors.primary.g,
                                  config->colors.primary.b,
                                  1.0f);
        cairo_set_line_width(cr, 2.0);
        cairo_move_to(cr, cur_x, cur_y);
        cairo_line_to(cr, cur_x, cur_y + cur_h);
        cairo_stroke(cr);

        if (m_multiline && m_needs_scroll_to_cursor) {
            m_needs_scroll_to_cursor = false;
            ViewGroup* p = get_parent();
            while (p) {
                if (auto* sv = dynamic_cast<ScrollView*>(p)) {
                    Rect cursor_rect(static_cast<int>(cur_x), static_cast<int>(cur_y), 4, static_cast<int>(cur_h));
                    sv->scroll_to_rect(cursor_rect);
                    break;
                }
                p = p->get_parent();
            }
        }
    } else if (m_editable && m_focused && is_hint) {
        cairo_set_source_rgba(cr, config->colors.primary.r,
                                  config->colors.primary.g,
                                  config->colors.primary.b,
                                  1.0f);
        cairo_set_line_width(cr, 2.0);
        cairo_move_to(cr, text_draw_x, text_draw_y);
        cairo_line_to(cr, text_draw_x, text_draw_y + text_h);
        cairo_stroke(cr);

        if (m_multiline && m_needs_scroll_to_cursor) {
            m_needs_scroll_to_cursor = false;
            ViewGroup* p = get_parent();
            while (p) {
                if (auto* sv = dynamic_cast<ScrollView*>(p)) {
                    Rect cursor_rect(static_cast<int>(text_draw_x), static_cast<int>(text_draw_y), 4, text_h);
                    sv->scroll_to_rect(cursor_rect);
                    break;
                }
                p = p->get_parent();
            }
        }
    }

    g_object_unref(layout);
    cairo_restore(cr);

    if (has_focus() && should_draw_focus_ring() && !m_draw_background) {
        draw_focus_ring(cr, bounds);
    }
}

bool EditText::on_key(const KeyPressEvent& event) {
    if (!m_editable || !m_focused || !event.pressed) return false;

    bool handled = false;

    if (event.keysym == XKB_KEY_Return || event.keysym == XKB_KEY_KP_Enter) {
        if (m_multiline) {
            m_text.insert(m_cursor_pos, "\n");
            m_cursor_pos++;
            if (m_on_text_changed) m_on_text_changed(std::static_pointer_cast<EditText>(shared_from_this()), m_text);
            handled = true;
        } else if (m_on_submit) {
            m_on_submit(m_text);
            handled = true;
        }
    } else if (event.keysym == XKB_KEY_BackSpace) {
        if (!m_text.empty() && m_cursor_pos > 0) {
            m_text.erase(m_cursor_pos - 1, 1);
            m_cursor_pos--;
            if (m_on_text_changed) m_on_text_changed(std::static_pointer_cast<EditText>(shared_from_this()), m_text);
        }
        handled = true;
    } else if (event.keysym == XKB_KEY_Delete) {
        if (m_cursor_pos < static_cast<int>(m_text.size())) {
            m_text.erase(m_cursor_pos, 1);
            if (m_on_text_changed) m_on_text_changed(std::static_pointer_cast<EditText>(shared_from_this()), m_text);
            handled = true;
        }
    } else if (event.keysym == XKB_KEY_Left) {
        if (m_cursor_pos > 0) {
            m_cursor_pos--;
        }
        handled = true;
    } else if (event.keysym == XKB_KEY_Right) {
        if (m_cursor_pos < static_cast<int>(m_text.size())) {
            m_cursor_pos++;
        }
        handled = true;
    } else if (event.keysym == XKB_KEY_Up) {
        if (m_multiline) {
            if (m_cursor_pos > 0) {
                size_t current_line_start = m_text.rfind('\n', m_cursor_pos - 1);
                if (current_line_start == std::string::npos) {
                    m_cursor_pos = 0;
                } else {
                    size_t col = m_cursor_pos - (current_line_start + 1);
                    size_t prev_line_start = (current_line_start > 0) ? m_text.rfind('\n', current_line_start - 1) : std::string::npos;
                    size_t prev_start = (prev_line_start == std::string::npos) ? 0 : (prev_line_start + 1);
                    size_t prev_line_len = current_line_start - prev_start;
                    m_cursor_pos = prev_start + std::min(col, prev_line_len);
                }
            } else {
                m_cursor_pos = 0;
            }
            handled = true;
        }
    } else if (event.keysym == XKB_KEY_Down) {
        if (m_multiline) {
            size_t current_line_start = (m_cursor_pos > 0) ? m_text.rfind('\n', m_cursor_pos - 1) : std::string::npos;
            size_t col = (current_line_start == std::string::npos) ? m_cursor_pos : (m_cursor_pos - (current_line_start + 1));
            size_t next_line_start = m_text.find('\n', m_cursor_pos);
            if (next_line_start == std::string::npos) {
                m_cursor_pos = static_cast<int>(m_text.size());
            } else {
                next_line_start++;
                size_t next_line_end = m_text.find('\n', next_line_start);
                if (next_line_end == std::string::npos) next_line_end = m_text.size();
                size_t next_line_len = next_line_end - next_line_start;
                m_cursor_pos = next_line_start + std::min(col, next_line_len);
            }
            handled = true;
        }
    } else if (event.keysym == XKB_KEY_Home) {
        m_cursor_pos = 0;
        handled = true;
    } else if (event.keysym == XKB_KEY_End) {
        m_cursor_pos = static_cast<int>(m_text.size());
        handled = true;
    } else if (event.has_ctrl() && (event.keysym == XKB_KEY_u || event.keysym == XKB_KEY_U)) {
        clear();
        if (m_on_text_changed) m_on_text_changed(std::static_pointer_cast<EditText>(shared_from_this()), m_text);
        handled = true;
    } else if (event.has_ctrl() && (event.keysym == XKB_KEY_w || event.keysym == XKB_KEY_W)) {
        while (!m_text.empty() && m_cursor_pos > 0 && m_text[m_cursor_pos - 1] == ' ') {
            m_text.erase(m_cursor_pos - 1, 1);
            m_cursor_pos--;
        }
        while (!m_text.empty() && m_cursor_pos > 0 && m_text[m_cursor_pos - 1] != ' ') {
            m_text.erase(m_cursor_pos - 1, 1);
            m_cursor_pos--;
        }
        if (m_on_text_changed) m_on_text_changed(std::static_pointer_cast<EditText>(shared_from_this()), m_text);
        handled = true;
    } else if (!event.utf8_text.empty() && event.utf8_text[0] >= 32 && event.utf8_text[0] != 127 && !event.has_ctrl() && !event.has_alt()) {
        m_text.insert(m_cursor_pos, event.utf8_text);
        m_cursor_pos += static_cast<int>(event.utf8_text.size());
        if (m_on_text_changed) m_on_text_changed(std::static_pointer_cast<EditText>(shared_from_this()), m_text);
        handled = true;
    }

    if (handled) {
        m_needs_scroll_to_cursor = true;
        if (m_window) m_window->schedule_redraw();
        return true;
    }

    return false;
}

bool EditText::on_mouse_button(int lx, int ly, MouseButton button, bool pressed, const Rect& bounds) {
    if (!m_editable) return false;
    if (button == MouseButton::Left && pressed) {
        int draw_x = bounds.x;
        int draw_y = bounds.y;
        int draw_w = bounds.width;
        int draw_h = bounds.height;
        Rect pill_rect(draw_x, draw_y, draw_w, draw_h);

        if (pill_rect.contains(lx, ly)) {
            if (is_focusable()) {
                request_focus();
            }
            int pad_left = m_padding.left > 0 ? m_padding.left : (m_draw_background ? 14 : 0);
            int pad_right = m_padding.right > 0 ? m_padding.right : (m_draw_background ? 14 : 0);
            int pad_top = m_padding.top > 0 ? m_padding.top : (m_draw_background && m_multiline ? 12 : 0);
            int rel_x = lx - (draw_x + pad_left);
            int rel_y = ly - (draw_y + pad_top);
            int avail_text_w = std::max(0, draw_w - pad_left - pad_right);

            if (rel_x <= 0 && rel_y <= 0) {
                m_cursor_pos = 0;
            } else if (!m_text.empty()) {
                cairo_surface_t* temp_surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
                cairo_t* cr = cairo_create(temp_surf);
                PangoLayout* layout = pango_cairo_create_layout(cr);
                pango_layout_set_text(layout, m_text.c_str(), -1);
                pango_layout_set_width(layout, avail_text_w * PANGO_SCALE);
                if (m_multiline) {
                    pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
                }

                auto config = Config::get();
                std::string font_family = !m_font_family.empty() ? m_font_family :
                    (m_multiline ? "Monospace" : (!config->metrics.font_family.empty() ? config->metrics.font_family : "Sans"));
                int font_size = m_font_size > 0 ? m_font_size : (config->metrics.font_size > 0 ? config->metrics.font_size : 11);
                std::string font_desc_str = font_family + " " + std::to_string(font_size);
                PangoFontDescription* desc = pango_font_description_from_string(font_desc_str.c_str());
                pango_layout_set_font_description(layout, desc);
                pango_font_description_free(desc);

                int trailing = 0;
                int target_y = m_multiline ? std::max(0, rel_y) * PANGO_SCALE : 0;
                pango_layout_xy_to_index(layout, std::max(0, rel_x) * PANGO_SCALE, target_y, &m_cursor_pos, &trailing);
                if (trailing > 0) m_cursor_pos += trailing;
                m_cursor_pos = std::clamp(m_cursor_pos, 0, static_cast<int>(m_text.size()));

                g_object_unref(layout);
                cairo_destroy(cr);
                cairo_surface_destroy(temp_surf);
            }
            m_needs_scroll_to_cursor = true;
            if (m_window) m_window->schedule_redraw();
            return true;
        }
    }
    return false;
}

} // namespace miqu
