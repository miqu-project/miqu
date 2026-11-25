#pragma once

#include "miqutoolkit/view/view_builder.hpp"
#include <string>

struct _PangoLayout;

namespace miqu {

enum class WrapMode {
    None,
    Word,
    Char,
    WordChar
};

enum class EllipsizeMode {
    None,
    Start,
    Middle,
    End
};

class TextView : public View {
public:
    TextView() = default;
    explicit TextView(std::string text) : m_text(std::move(text)) {}
    ~TextView() override;

    void set_text(std::string text) {
        if (m_text != text) {
            m_text = std::move(text);
            m_layout_dirty = true;
            request_redraw();
        }
    }
    const std::string& get_text() const { return m_text; }

    void set_text_color(const Color& color) { m_color = color; m_has_custom_color = true; request_redraw(); }
    const Color& get_text_color() const { return m_color; }
    bool has_custom_color() const { return m_has_custom_color; }

    void set_font_family(std::string family) {
        if (m_font_family != family) {
            m_font_family = std::move(family);
            m_layout_dirty = true;
            request_redraw();
        }
    }
    const std::string& get_font_family() const { return m_font_family; }

    void set_text_size(int size) {
        if (m_font_size != size) {
            m_font_size = size;
            m_layout_dirty = true;
            request_redraw();
        }
    }
    int get_text_size() const { return m_font_size; }

    void set_bold(bool bold) {
        if (m_bold != bold) {
            m_bold = bold;
            m_layout_dirty = true;
            request_redraw();
        }
    }
    bool is_bold() const { return m_bold; }

    void set_italic(bool italic) {
        if (m_italic != italic) {
            m_italic = italic;
            m_layout_dirty = true;
            request_redraw();
        }
    }
    bool is_italic() const { return m_italic; }

    void set_heading_level(int level) {
        if (m_heading_level != level) {
            m_heading_level = level;
            m_layout_dirty = true;
            request_redraw();
        }
    }
    int get_heading_level() const { return m_heading_level; }

    void set_caption(bool c) {
        if (m_caption != c) {
            m_caption = c;
            m_layout_dirty = true;
            request_redraw();
        }
    }
    bool is_caption() const { return m_caption; }

    void set_muted(bool m) {
        if (m_muted != m) {
            m_muted = m;
            request_redraw();
        }
    }
    bool is_muted() const { return m_muted; }

    void set_text_alignment(TextAlignment align) {
        if (m_align != align) {
            m_align = align;
            m_layout_dirty = true;
            request_redraw();
        }
    }
    TextAlignment get_text_alignment() const { return m_align; }

    void set_center_aligned(bool center = true) {
        set_text_alignment(center ? TextAlignment::Center : TextAlignment::Left);
    }
    bool is_center_aligned() const { return m_align == TextAlignment::Center; }

    void set_vertical_alignment(TextVerticalAlignment align) {
        if (m_vertical_align != align || !m_has_custom_vertical_align) {
            m_vertical_align = align;
            m_has_custom_vertical_align = true;
            request_redraw();
        }
    }
    TextVerticalAlignment get_vertical_alignment() const { return m_vertical_align; }

    void set_multiline(bool m) {
        if (m_multiline != m) {
            m_multiline = m;
            if (m && m_wrap_mode == WrapMode::None) {
                m_wrap_mode = WrapMode::WordChar;
            }
            m_layout_dirty = true;
            request_redraw();
        }
    }
    bool is_multiline() const { return m_multiline; }

    void set_wrap(bool w) { set_multiline(w); }
    bool is_wrap() const { return m_multiline; }

    void set_wrap_mode(WrapMode mode) {
        if (m_wrap_mode != mode) {
            m_wrap_mode = mode;
            if (mode != WrapMode::None) {
                m_multiline = true;
            }
            m_layout_dirty = true;
            request_redraw();
        }
    }
    WrapMode get_wrap_mode() const { return m_wrap_mode; }

    void set_max_lines(int max_lines) {
        if (m_max_lines != max_lines) {
            m_max_lines = max_lines;
            m_layout_dirty = true;
            request_redraw();
        }
    }
    int get_max_lines() const { return m_max_lines; }

    void set_ellipsize(bool ellipsize) {
        if (m_ellipsize != ellipsize) {
            m_ellipsize = ellipsize;
            if (!ellipsize) {
                m_ellipsize_mode = EllipsizeMode::None;
            } else if (m_ellipsize_mode == EllipsizeMode::None) {
                m_ellipsize_mode = EllipsizeMode::End;
            }
            m_layout_dirty = true;
            request_redraw();
        }
    }
    bool is_ellipsize() const { return m_ellipsize && m_ellipsize_mode != EllipsizeMode::None; }

    void set_ellipsize_mode(EllipsizeMode mode) {
        if (m_ellipsize_mode != mode) {
            m_ellipsize_mode = mode;
            m_ellipsize = (mode != EllipsizeMode::None);
            m_layout_dirty = true;
            request_redraw();
        }
    }
    EllipsizeMode get_ellipsize_mode() const { return m_ellipsize_mode; }

    void set_line_spacing(float spacing) {
        if (std::abs(m_line_spacing - spacing) > 0.001f) {
            m_line_spacing = spacing;
            m_layout_dirty = true;
            request_redraw();
        }
    }
    float get_line_spacing() const { return m_line_spacing; }

    void draw(cairo_t* cr, const Rect& bounds) override;
    Size measure_size() const override;
    Size measure_size(int avail_width) const override;

private:
    std::string m_text;
    std::string m_font_family = "";
    Color m_color;
    bool m_has_custom_color = false;
    int m_font_size = -1;
    bool m_bold = false;
    bool m_italic = false;
    int m_heading_level = 0; // 0 = normal, 1 = h1, 2 = h2, 3 = h3
    bool m_caption = false;
    bool m_muted = false;
    TextAlignment m_align = TextAlignment::Left;
    TextVerticalAlignment m_vertical_align = TextVerticalAlignment::Center;
    bool m_has_custom_vertical_align = false;
    bool m_multiline = false;
    WrapMode m_wrap_mode = WrapMode::None;
    int m_max_lines = -1; // <= 0 means unlimited
    bool m_ellipsize = true;
    EllipsizeMode m_ellipsize_mode = EllipsizeMode::End;
    float m_line_spacing = 1.0f;

    struct _PangoLayout* ensure_layout(cairo_t* cr, int target_w, int target_h, int& out_text_w, int& out_text_h) const;

    mutable struct _PangoLayout* m_cached_layout = nullptr;
    mutable bool m_layout_dirty = true;
    mutable int m_cached_target_w = -2;
    mutable int m_cached_target_h = -2;
    mutable int m_cached_text_w = 0;
    mutable int m_cached_text_h = 0;
    mutable std::string m_cached_font_desc;
};

class TextViewBuilder : public ViewBuilder<TextViewBuilder, TextView> {
public:
    TextViewBuilder() : ViewBuilder<TextViewBuilder, TextView>() {}
    explicit TextViewBuilder(std::string text) : ViewBuilder<TextViewBuilder, TextView>() {
        m_view->set_text(std::move(text));
    }

    static std::shared_ptr<TextViewBuilder> create() {
        return std::make_shared<TextViewBuilder>();
    }

    static std::shared_ptr<TextViewBuilder> create(std::string text) {
        return std::make_shared<TextViewBuilder>(std::move(text));
    }

    std::shared_ptr<TextViewBuilder> text(std::string text) {
        m_view->set_text(std::move(text));
        return shared_from_this();
    }

    std::shared_ptr<TextViewBuilder> textColor(const Color& col) {
        m_view->set_text_color(col);
        return shared_from_this();
    }

    std::shared_ptr<TextViewBuilder> fontFamily(std::string family) {
        m_view->set_font_family(std::move(family));
        return shared_from_this();
    }

    std::shared_ptr<TextViewBuilder> textSize(int size) {
        m_view->set_text_size(size);
        return shared_from_this();
    }

    std::shared_ptr<TextViewBuilder> h1() {
        m_view->set_heading_level(1);
        return shared_from_this();
    }

    std::shared_ptr<TextViewBuilder> h2() {
        m_view->set_heading_level(2);
        return shared_from_this();
    }

    std::shared_ptr<TextViewBuilder> h3() {
        m_view->set_heading_level(3);
        return shared_from_this();
    }

    std::shared_ptr<TextViewBuilder> caption(bool c = true) {
        m_view->set_caption(c);
        return shared_from_this();
    }

    std::shared_ptr<TextViewBuilder> muted(bool m = true) {
        m_view->set_muted(m);
        return shared_from_this();
    }

    std::shared_ptr<TextViewBuilder> bold(bool b = true) {
        m_view->set_bold(b);
        return shared_from_this();
    }

    std::shared_ptr<TextViewBuilder> textAlignment(TextAlignment a) {
        m_view->set_text_alignment(a);
        return shared_from_this();
    }

    std::shared_ptr<TextViewBuilder> center(bool c = true) {
        m_view->set_center_aligned(c);
        return shared_from_this();
    }

    std::shared_ptr<TextViewBuilder> right() {
        m_view->set_text_alignment(TextAlignment::Right);
        return shared_from_this();
    }

    std::shared_ptr<TextViewBuilder> justify() {
        m_view->set_text_alignment(TextAlignment::Justify);
        return shared_from_this();
    }

    std::shared_ptr<TextViewBuilder> ellipsize(bool e = true) {
        m_view->set_ellipsize(e);
        return shared_from_this();
    }

    std::shared_ptr<TextViewBuilder> ellipsizeMode(EllipsizeMode mode) {
        m_view->set_ellipsize_mode(mode);
        return shared_from_this();
    }

    std::shared_ptr<TextViewBuilder> multiline(bool m = true) {
        m_view->set_multiline(m);
        return shared_from_this();
    }

    std::shared_ptr<TextViewBuilder> wrap(bool w = true) {
        m_view->set_wrap(w);
        return shared_from_this();
    }

    std::shared_ptr<TextViewBuilder> wrapMode(WrapMode mode) {
        m_view->set_wrap_mode(mode);
        return shared_from_this();
    }

    std::shared_ptr<TextViewBuilder> maxLines(int lines) {
        m_view->set_max_lines(lines);
        return shared_from_this();
    }

    std::shared_ptr<TextViewBuilder> verticalAlignment(TextVerticalAlignment a) {
        m_view->set_vertical_alignment(a);
        return shared_from_this();
    }

    std::shared_ptr<TextViewBuilder> lineSpacing(float s) {
        m_view->set_line_spacing(s);
        return shared_from_this();
    }
};

} // namespace miqu
