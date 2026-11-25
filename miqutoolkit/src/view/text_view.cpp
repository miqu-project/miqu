#include "miqutoolkit/view/text_view.hpp"
#include "miqutoolkit/core/config.hpp"
#include <pango/pangocairo.h>
#include <algorithm>

namespace miqu {

static int resolve_font_size(int explicit_size, int heading_level, bool caption) {
    if (explicit_size > 0) return explicit_size;
    auto config = Config::get();
    if (heading_level == 1) return config->metrics.h1_size;
    if (heading_level == 2) return config->metrics.h2_size;
    if (heading_level == 3) return config->metrics.h3_size;
    if (caption) return config->metrics.caption_size;
    return config->metrics.font_size > 0 ? config->metrics.font_size : 11;
}

TextView::~TextView() {
    if (m_cached_layout) {
        g_object_unref(m_cached_layout);
        m_cached_layout = nullptr;
    }
}

PangoLayout* TextView::ensure_layout(cairo_t* cr, int target_w, int target_h, int& out_text_w, int& out_text_h) const {
    auto config = Config::get();
    std::string font_family = !m_font_family.empty() ? m_font_family : config->metrics.font_family;
    if (font_family.empty()) font_family = "Sans";

    int font_size = resolve_font_size(m_font_size, m_heading_level, m_caption);
    bool is_bold = m_bold || (m_heading_level >= 1 && m_heading_level <= 3);

    std::string font_desc_str = font_family + " " + std::to_string(font_size);
    if (is_bold) font_desc_str += " Bold";
    if (m_italic) font_desc_str += " Italic";

    int effective_target_h = (m_multiline && m_max_lines <= 0 && m_ellipsize && m_ellipsize_mode != EllipsizeMode::None)
        ? target_h : -1;

    if (!m_cached_layout) {
        if (cr) {
            m_cached_layout = pango_cairo_create_layout(cr);
        } else {
            cairo_surface_t* temp_surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
            cairo_t* temp_cr = cairo_create(temp_surf);
            m_cached_layout = pango_cairo_create_layout(temp_cr);
            cairo_destroy(temp_cr);
            cairo_surface_destroy(temp_surf);
        }
        m_layout_dirty = true;
    } else if (cr) {
        pango_cairo_update_layout(cr, m_cached_layout);
    }

    if (m_layout_dirty ||
        m_cached_target_w != target_w ||
        m_cached_target_h != effective_target_h ||
        m_cached_font_desc != font_desc_str) {

        pango_layout_set_text(m_cached_layout, m_text.c_str(), -1);

        PangoFontDescription* desc = pango_font_description_from_string(font_desc_str.c_str());
        pango_layout_set_font_description(m_cached_layout, desc);
        pango_font_description_free(desc);

        if (m_line_spacing != 1.0f && m_line_spacing > 0.0f) {
            pango_layout_set_line_spacing(m_cached_layout, m_line_spacing);
        } else {
            pango_layout_set_line_spacing(m_cached_layout, 0.0f);
        }

        if (m_align == TextAlignment::Center) {
            pango_layout_set_alignment(m_cached_layout, PANGO_ALIGN_CENTER);
            pango_layout_set_justify(m_cached_layout, FALSE);
        } else if (m_align == TextAlignment::Right) {
            pango_layout_set_alignment(m_cached_layout, PANGO_ALIGN_RIGHT);
            pango_layout_set_justify(m_cached_layout, FALSE);
        } else if (m_align == TextAlignment::Justify) {
            pango_layout_set_alignment(m_cached_layout, PANGO_ALIGN_LEFT);
            pango_layout_set_justify(m_cached_layout, TRUE);
        } else {
            pango_layout_set_alignment(m_cached_layout, PANGO_ALIGN_LEFT);
            pango_layout_set_justify(m_cached_layout, FALSE);
        }

        if (m_multiline) {
            pango_layout_set_single_paragraph_mode(m_cached_layout, FALSE);
            if (target_w > 0) {
                pango_layout_set_width(m_cached_layout, target_w * PANGO_SCALE);
            } else {
                pango_layout_set_width(m_cached_layout, -1);
            }
            PangoWrapMode pango_wrap = PANGO_WRAP_WORD_CHAR;
            if (m_wrap_mode == WrapMode::Word) pango_wrap = PANGO_WRAP_WORD;
            else if (m_wrap_mode == WrapMode::Char) pango_wrap = PANGO_WRAP_CHAR;
            pango_layout_set_wrap(m_cached_layout, pango_wrap);

            if (m_max_lines > 0) {
                pango_layout_set_height(m_cached_layout, -m_max_lines);
                if (m_ellipsize && m_ellipsize_mode != EllipsizeMode::None) {
                    pango_layout_set_ellipsize(m_cached_layout, PANGO_ELLIPSIZE_END);
                } else {
                    pango_layout_set_ellipsize(m_cached_layout, PANGO_ELLIPSIZE_NONE);
                }
            } else {
                pango_layout_set_height(m_cached_layout, -1);
                pango_layout_set_ellipsize(m_cached_layout, PANGO_ELLIPSIZE_NONE);
                if (effective_target_h > 0) {
                    int unconstrained_w = 0, unconstrained_h = 0;
                    pango_layout_get_pixel_size(m_cached_layout, &unconstrained_w, &unconstrained_h);
                    if (unconstrained_h > effective_target_h) {
                        int lines = pango_layout_get_line_count(m_cached_layout);
                        if (lines > 1) {
                            int line_h = unconstrained_h / lines;
                            if (line_h > 0) {
                                int fitted = std::max(1, effective_target_h / line_h);
                                pango_layout_set_height(m_cached_layout, -fitted);
                                pango_layout_set_ellipsize(m_cached_layout, PANGO_ELLIPSIZE_END);
                            }
                        }
                    }
                }
            }
        } else {
            pango_layout_set_height(m_cached_layout, -1);
            pango_layout_set_single_paragraph_mode(m_cached_layout, TRUE);
            if (target_w > 0) {
                pango_layout_set_width(m_cached_layout, target_w * PANGO_SCALE);
            } else {
                pango_layout_set_width(m_cached_layout, -1);
            }
            if (m_ellipsize && m_ellipsize_mode != EllipsizeMode::None && target_w > 0) {
                PangoEllipsizeMode em = PANGO_ELLIPSIZE_END;
                if (m_ellipsize_mode == EllipsizeMode::Start) em = PANGO_ELLIPSIZE_START;
                else if (m_ellipsize_mode == EllipsizeMode::Middle) em = PANGO_ELLIPSIZE_MIDDLE;
                pango_layout_set_ellipsize(m_cached_layout, em);
            } else {
                pango_layout_set_ellipsize(m_cached_layout, PANGO_ELLIPSIZE_NONE);
            }
        }

        pango_layout_get_pixel_size(m_cached_layout, &m_cached_text_w, &m_cached_text_h);
        m_cached_target_w = target_w;
        m_cached_target_h = effective_target_h;
        m_cached_font_desc = std::move(font_desc_str);
        m_layout_dirty = false;
    }

    out_text_w = m_cached_text_w;
    out_text_h = m_cached_text_h;
    return m_cached_layout;
}

Size TextView::measure_size() const {
    return measure_size(-1);
}

Size TextView::measure_size(int avail_width) const {
    if (m_text.empty()) return Size(0, 0);

    int target_w = -1;
    if (m_layout_params.width >= 0) {
        target_w = m_layout_params.width - m_padding.left - m_padding.right;
    } else if (avail_width >= 0) {
        target_w = avail_width - m_padding.left - m_padding.right;
    }

    int text_w = 0, text_h = 0;
    ensure_layout(nullptr, target_w, -1, text_w, text_h);

    int total_w = text_w + m_padding.left + m_padding.right;
    int total_h = text_h + m_padding.top + m_padding.bottom;
    return Size(total_w, total_h);
}

void TextView::draw(cairo_t* cr, const Rect& bounds) {
    if (!is_visible() || !cr || m_text.empty() || bounds.width <= 0 || bounds.height <= 0) return;

    Rect content_bounds = get_content_rect(bounds);
    int draw_x = content_bounds.x;
    int draw_y = content_bounds.y;
    int draw_w = content_bounds.width;
    int draw_h = content_bounds.height;

    if (draw_w <= 0 || draw_h <= 0) return;

    cairo_save(cr);

    // Clip to content rect
    cairo_rectangle(cr, draw_x, draw_y, draw_w, draw_h);
    cairo_clip(cr);

    int text_w = 0, text_h = 0;
    PangoLayout* layout = ensure_layout(cr, draw_w, draw_h, text_w, text_h);

    TextVerticalAlignment v_align = m_has_custom_vertical_align ?
        m_vertical_align :
        (m_multiline ? TextVerticalAlignment::Top : TextVerticalAlignment::Center);

    double baseline_y = draw_y;
    if (v_align == TextVerticalAlignment::Center) {
        if (draw_h > text_h) {
            baseline_y = draw_y + (draw_h - text_h) / 2.0;
        }
    } else if (v_align == TextVerticalAlignment::Bottom) {
        if (draw_h > text_h) {
            baseline_y = draw_y + (draw_h - text_h);
        }
    }

    auto config = Config::get();
    Color text_col;
    if (m_has_custom_color) {
        text_col = m_color;
    } else if (m_muted) {
        text_col = config->colors.on_surface_variant;
    } else if (m_heading_level == 1) {
        text_col = config->colors.primary;
    } else if (m_heading_level == 2 || m_heading_level == 3) {
        text_col = config->colors.on_surface;
    } else if (m_caption) {
        text_col = config->colors.on_surface_variant;
    } else {
        text_col = config->colors.on_surface;
    }

    cairo_set_source_rgba(cr, text_col.r, text_col.g, text_col.b, text_col.a);

    if (m_multiline && m_max_lines <= 0 && !(m_ellipsize && m_ellipsize_mode != EllipsizeMode::None)) {
        double clip_x1 = 0.0, clip_y1 = 0.0, clip_x2 = 0.0, clip_y2 = 0.0;
        cairo_clip_extents(cr, &clip_x1, &clip_y1, &clip_x2, &clip_y2);

        PangoLayoutIter* iter = pango_layout_get_iter(layout);
        do {
            PangoRectangle logical_rect;
            pango_layout_iter_get_line_extents(iter, nullptr, &logical_rect);
            double line_top = baseline_y + static_cast<double>(logical_rect.y) / PANGO_SCALE;
            double line_bottom = line_top + static_cast<double>(logical_rect.height) / PANGO_SCALE;

            if (line_bottom < clip_y1) continue;
            if (line_top > clip_y2) break;

            int baseline = pango_layout_iter_get_baseline(iter);
            double line_x = draw_x + static_cast<double>(logical_rect.x) / PANGO_SCALE;
            double line_baseline_y = baseline_y + static_cast<double>(baseline) / PANGO_SCALE;
            cairo_move_to(cr, line_x, line_baseline_y);
            pango_cairo_show_layout_line(cr, pango_layout_iter_get_line_readonly(iter));
        } while (pango_layout_iter_next_line(iter));
        pango_layout_iter_free(iter);
    } else {
        cairo_move_to(cr, draw_x, baseline_y);
        pango_cairo_show_layout(cr, layout);
    }

    cairo_restore(cr);
}

} // namespace miqu
