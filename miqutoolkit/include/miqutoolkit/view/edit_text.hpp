#pragma once

#include "miqutoolkit/view/view.hpp"
#include "miqutoolkit/view/view_builder.hpp"
#include <string>
#include <functional>

struct _PangoLayout;
struct _cairo_surface;

namespace miqu {

class EditText : public View {
public:
    EditText() { m_focusable = true; }
    ~EditText() override;

    void set_text(std::string text);
    const std::string& get_text() const { return m_text; }

    void set_hint(std::string hint) { m_hint = std::move(hint); m_layout_dirty = true; }
    const std::string& get_hint() const { return m_hint; }

    void set_on_text_changed_listener(std::function<void(std::shared_ptr<EditText>, const std::string&)> cb) {
        m_on_text_changed = std::move(cb);
    }

    void set_on_submit_listener(std::function<void(const std::string&)> cb) {
        m_on_submit = std::move(cb);
    }

    void set_password_mode(bool enable) {
        if (m_password_mode != enable) { m_password_mode = enable; m_layout_dirty = true; }
    }
    bool is_password_mode() const { return m_password_mode; }

    void set_editable(bool editable);
    bool is_editable() const { return m_editable; }

    void set_multiline(bool enable) {
        if (m_multiline != enable) { m_multiline = enable; m_layout_dirty = true; }
    }
    bool is_multiline() const { return m_multiline; }

    void set_focused(bool focus);
    bool is_focused() const { return m_focused; }

    void set_draw_background(bool draw) { m_draw_background = draw; }
    bool is_draw_background() const { return m_draw_background; }

    void set_text_color(const Color& col) { m_text_color = col; m_has_custom_text_color = true; }
    void set_hint_color(const Color& col) { m_hint_color = col; m_has_custom_hint_color = true; }
    void set_background_color(const Color& col) { m_bg_color = col; m_has_custom_bg_color = true; }

    void set_font_family(std::string family) {
        if (m_font_family != family) { m_font_family = std::move(family); m_layout_dirty = true; }
    }
    const std::string& get_font_family() const { return m_font_family; }

    void set_font_size(int size) {
        if (m_font_size != size) { m_font_size = size; m_layout_dirty = true; }
    }
    int get_font_size() const { return m_font_size; }

    const Color& get_text_color() const { return m_text_color; }
    const Color& get_hint_color() const { return m_hint_color; }
    const Color& get_background_color() const { return m_bg_color; }

    void clear();

    void set_window(Window* win) override;
    void on_focus_changed(bool focused) override;

    void draw(cairo_t* cr, const Rect& bounds) override;
    Size measure_size() const override;
    Size measure_size(int avail_width) const override;

    bool on_key(const KeyPressEvent& event) override;
    bool captures_text_input() const override {
        return is_visible() && is_enabled() && m_editable && (has_focus() || is_focused());
    }
    bool on_mouse_button(int lx, int ly, MouseButton button, bool pressed, const Rect& bounds) override;

    void request_scroll_to_cursor() { m_needs_scroll_to_cursor = true; }

private:
    std::string m_text;
    std::string m_hint = "";
    std::string m_font_family = "";
    int m_font_size = -1;
    int m_cursor_pos = 0;
    bool m_focused = false;
    bool m_draw_background = true;
    bool m_password_mode = false;
    bool m_editable = true;
    bool m_multiline = false;
    bool m_needs_scroll_to_cursor = false;

    Color m_text_color;
    Color m_hint_color;
    Color m_bg_color;
    bool m_has_custom_text_color = false;
    bool m_has_custom_hint_color = false;
    bool m_has_custom_bg_color = false;

    struct _PangoLayout* ensure_layout(cairo_t* cr, int avail_text_w, int& out_text_w, int& out_text_h) const;
    void invalidate_tiles() const;

    struct TextTile {
        int index = 0;
        int height = 0;
        struct _cairo_surface* surface = nullptr;
        uint64_t last_used = 0;
    };

    mutable struct _PangoLayout* m_cached_layout = nullptr;
    mutable bool m_layout_dirty = true;
    mutable int m_cached_avail_w = -2;
    mutable int m_cached_text_w = 0;
    mutable int m_cached_text_h = 0;
    mutable std::string m_cached_font_desc;
    mutable bool m_cached_is_hint = false;
    mutable bool m_cached_multiline = false;
    mutable bool m_cached_password = false;

    mutable std::vector<TextTile> m_tiles;
    mutable bool m_tiles_dirty = true;
    mutable int m_tile_avail_w = -1;
    mutable Color m_tile_color;
    mutable uint64_t m_tile_lru_counter = 0;

    std::function<void(std::shared_ptr<EditText>, const std::string&)> m_on_text_changed;
    std::function<void(const std::string&)> m_on_submit;
};

class EditTextBuilder : public ViewBuilder<EditTextBuilder, EditText> {
public:
    using ViewBuilder<EditTextBuilder, EditText>::ViewBuilder;

    static std::shared_ptr<EditTextBuilder> create() {
        return std::make_shared<EditTextBuilder>();
    }

    std::shared_ptr<EditTextBuilder> hint(std::string h) {
        m_view->set_hint(std::move(h));
        return shared_from_this();
    }

    std::shared_ptr<EditTextBuilder> text(std::string t) {
        m_view->set_text(std::move(t));
        return shared_from_this();
    }

    std::shared_ptr<EditTextBuilder> textColor(const Color& col) {
        m_view->set_text_color(col);
        return shared_from_this();
    }

    std::shared_ptr<EditTextBuilder> hintColor(const Color& col) {
        m_view->set_hint_color(col);
        return shared_from_this();
    }

    std::shared_ptr<EditTextBuilder> backgroundColor(const Color& col) {
        m_view->set_background_color(col);
        return shared_from_this();
    }

    std::shared_ptr<EditTextBuilder> onTextChanged(std::function<void(std::shared_ptr<EditText>, const std::string&)> cb) {
        m_view->set_on_text_changed_listener(std::move(cb));
        return shared_from_this();
    }

    std::shared_ptr<EditTextBuilder> focused(bool f = true) {
        m_view->set_focused(f);
        return shared_from_this();
    }

    std::shared_ptr<EditTextBuilder> drawBackground(bool d = true) {
        m_view->set_draw_background(d);
        return shared_from_this();
    }

    std::shared_ptr<EditTextBuilder> passwordMode(bool enable = true) {
        m_view->set_password_mode(enable);
        return shared_from_this();
    }

    std::shared_ptr<EditTextBuilder> editable(bool ed = true) {
        m_view->set_editable(ed);
        return shared_from_this();
    }

    std::shared_ptr<EditTextBuilder> multiline(bool enable = true) {
        m_view->set_multiline(enable);
        return shared_from_this();
    }

    std::shared_ptr<EditTextBuilder> fontFamily(std::string family) {
        m_view->set_font_family(std::move(family));
        return shared_from_this();
    }

    std::shared_ptr<EditTextBuilder> fontSize(int size) {
        m_view->set_font_size(size);
        return shared_from_this();
    }

    std::shared_ptr<EditTextBuilder> onSubmit(std::function<void(const std::string&)> cb) {
        m_view->set_on_submit_listener(std::move(cb));
        return shared_from_this();
    }
};

} // namespace miqu
