#pragma once

#include "miqutoolkit/view/view_group.hpp"
#include "miqutoolkit/view/view_builder.hpp"
#include "miqutoolkit/view/text_view.hpp"
#include "miqutoolkit/view/edit_text.hpp"
#include "miqutoolkit/view/image_view.hpp"
#include <string>
#include <functional>

namespace miqu {

class SearchView : public ViewGroup {
public:
    SearchView();

    void set_title(std::string title);
    const std::string& get_title() const { return m_title; }

    void set_hint(std::string hint);
    const std::string& get_hint() const;

    void set_query(std::string query);
    const std::string& get_query() const;

    void set_focused(bool focus);
    bool is_focused() const;

    void set_focus_border(bool enable) {
        m_focus_border = enable;
        set_draw_focus_ring(enable);
    }
    bool has_focus_border() const { return m_focus_border; }

    void set_radius(int radius) {
        m_corner_radius = radius;
        set_focus_ring_radius(radius);
    }
    int get_radius() const { return m_corner_radius; }

    void set_elevation(double elevation) { m_elevation = std::max(0.0, elevation); }
    double get_elevation() const { return m_elevation; }

    void set_background_color(const Color& bg) { m_bg_color = bg; m_has_custom_bg = true; }
    void set_stroke(int width, const Color& stroke) { m_stroke_width = width; m_stroke_color = stroke; }

    void set_on_query_text_listener(std::function<void(const std::string&)> on_change,
                                    std::function<void(const std::string&)> on_submit = nullptr);

    void draw(cairo_t* cr, const Rect& bounds) override;
    void draw_focus_ring(cairo_t* cr, const Rect& bounds) override;
    Size measure_size() const override;

    bool on_key(const KeyPressEvent& event) override;
    bool captures_text_input() const override {
        return is_visible() && is_enabled() && (has_focus() || is_focused() || (m_edit_text && m_edit_text->captures_text_input()));
    }
    bool on_mouse_button(int lx, int ly, MouseButton button, bool pressed, const Rect& bounds) override;
    bool on_mouse_move(int lx, int ly, const Rect& bounds) override;
    void on_focus_changed(bool focused) override;

    void set_focusable(bool focusable) override;
    void collect_focusable_views(std::vector<View*>& out) override;

    std::shared_ptr<EditText> get_edit_text() const { return m_edit_text; }
    std::shared_ptr<TextView> get_title_view() const { return m_title_view; }

private:
    std::string m_title;
    std::shared_ptr<TextView> m_title_view;
    std::shared_ptr<EditText> m_edit_text;

    bool m_focus_border = true;
    int m_corner_radius = -1;
    double m_elevation = 0.0;
    int m_stroke_width = 0;
    Color m_stroke_color = Color::transparent();
    Color m_bg_color;
    bool m_has_custom_bg = false;

    std::function<void(const std::string&)> m_on_text_change;
    std::function<void(const std::string&)> m_on_text_submit;
};

class SearchViewBuilder : public ViewGroupBuilder<SearchViewBuilder, SearchView> {
public:
    using ViewGroupBuilder<SearchViewBuilder, SearchView>::ViewGroupBuilder;

    static std::shared_ptr<SearchViewBuilder> create() {
        return std::make_shared<SearchViewBuilder>();
    }

    std::shared_ptr<SearchViewBuilder> title(std::string t) {
        m_view->set_title(std::move(t));
        return shared_from_this();
    }

    std::shared_ptr<SearchViewBuilder> hint(std::string h) {
        m_view->set_hint(std::move(h));
        return shared_from_this();
    }

    std::shared_ptr<SearchViewBuilder> query(std::string q) {
        m_view->set_query(std::move(q));
        return shared_from_this();
    }

    std::shared_ptr<SearchViewBuilder> focused(bool f = true) {
        m_view->set_focused(f);
        return shared_from_this();
    }

    std::shared_ptr<SearchViewBuilder> focusBorder(bool enable) {
        m_view->set_focus_border(enable);
        return shared_from_this();
    }

    std::shared_ptr<SearchViewBuilder> cornerRadius(int radius) {
        m_view->set_radius(radius);
        return shared_from_this();
    }

    std::shared_ptr<SearchViewBuilder> elevation(double elev) {
        m_view->set_elevation(elev);
        return shared_from_this();
    }

    std::shared_ptr<SearchViewBuilder> onQueryTextListener(std::function<void(const std::string&)> on_change,
                                                          std::function<void(const std::string&)> on_submit = nullptr) {
        m_view->set_on_query_text_listener(std::move(on_change), std::move(on_submit));
        return shared_from_this();
    }
};

} // namespace miqu
