#pragma once

#include "miqutoolkit/view/view_group.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace miqu {

enum class TabBarStyle {
    Segmented, // Rounded capsule track with sliding/pill indicator
    Underline  // Flat row with underline accent on active tab
};

class TabBar;

class TabItemView : public View {
public:
    TabItemView(TabBar* parent_bar, int index, std::string text);
    ~TabItemView() override = default;

    int get_index() const { return m_index; }
    void set_index(int index) { m_index = index; }

    const std::string& get_text() const { return m_text; }
    void set_text(const std::string& text);

    void draw(cairo_t* cr, const Rect& bounds) override;
    Size measure_size() const override;
    void draw_focus_ring(cairo_t* cr, const Rect& bounds) override;

    bool on_mouse_move(int lx, int ly, const Rect& bounds) override;
    bool on_mouse_button(int lx, int ly, MouseButton button, bool pressed, const Rect& bounds) override;
    bool on_key(const KeyPressEvent& event) override;

private:
    TabBar* m_parent_bar = nullptr;
    int m_index = 0;
    std::string m_text;
    bool m_hovered = false;
    bool m_pressed = false;
};

class TabBar : public ViewGroup {
public:
    TabBar();
    ~TabBar() override = default;

    void set_tabs(const std::vector<std::string>& tabs);
    void add_tab(const std::string& label);
    void set_tab_text(int index, const std::string& text);
    const std::vector<std::string>& get_tabs() const { return m_tabs; }
    size_t get_tab_count() const { return m_tabs.size(); }

    std::shared_ptr<TabItemView> get_tab_item(size_t index) const {
        if (index < m_children.size()) {
            return std::dynamic_pointer_cast<TabItemView>(m_children[index]);
        }
        return nullptr;
    }

    void set_selected_index(int index);
    int get_selected_index() const { return m_selected_index; }

    void set_style(TabBarStyle style) { m_style = style; }
    TabBarStyle get_style() const { return m_style; }

    void set_equal_widths(bool equal) { m_equal_widths = equal; }
    bool is_equal_widths() const { return m_equal_widths; }

    void set_font_size(int size) { m_font_size = size; }
    int get_font_size() const { return m_font_size; }

    void set_corner_radius(int radius) { m_corner_radius = radius; }
    int get_corner_radius() const { return m_corner_radius; }

    void set_bold_active(bool bold) { m_bold_active = bold; }
    bool is_bold_active() const { return m_bold_active; }

    void set_on_tab_selected_listener(std::function<void(int index)> cb) {
        m_on_tab_selected = std::move(cb);
    }

    void draw(cairo_t* cr, const Rect& bounds) override;
    Size measure_size() const override;

private:
    std::vector<std::string> m_tabs;
    int m_selected_index = 0;
    TabBarStyle m_style = TabBarStyle::Segmented;
    bool m_equal_widths = true;
    int m_font_size = -1;
    int m_corner_radius = -1;
    bool m_bold_active = false;

    std::function<void(int)> m_on_tab_selected;
};

class TabBarBuilder : public std::enable_shared_from_this<TabBarBuilder> {
public:
    TabBarBuilder();

    static std::shared_ptr<TabBarBuilder> create() {
        return std::make_shared<TabBarBuilder>();
    }

    std::shared_ptr<TabBarBuilder> tabs(const std::vector<std::string>& tabs) {
        m_view->set_tabs(tabs);
        return shared_from_this();
    }

    std::shared_ptr<TabBarBuilder> addTab(const std::string& label) {
        m_view->add_tab(label);
        return shared_from_this();
    }

    std::shared_ptr<TabBarBuilder> selectedIndex(int idx) {
        m_view->set_selected_index(idx);
        return shared_from_this();
    }

    std::shared_ptr<TabBarBuilder> style(TabBarStyle s) {
        m_view->set_style(s);
        return shared_from_this();
    }

    std::shared_ptr<TabBarBuilder> equalWidths(bool eq) {
        m_view->set_equal_widths(eq);
        return shared_from_this();
    }

    std::shared_ptr<TabBarBuilder> fontSize(int sz) {
        m_view->set_font_size(sz);
        return shared_from_this();
    }

    std::shared_ptr<TabBarBuilder> cornerRadius(int r) {
        m_view->set_corner_radius(r);
        return shared_from_this();
    }

    std::shared_ptr<TabBarBuilder> boldActive(bool b = true) {
        m_view->set_bold_active(b);
        return shared_from_this();
    }

    std::shared_ptr<TabBarBuilder> padding(const Padding& p) {
        m_view->set_padding(p);
        return shared_from_this();
    }

    std::shared_ptr<TabBarBuilder> padding(int h, int v) {
        m_view->set_padding(h, v);
        return shared_from_this();
    }

    std::shared_ptr<TabBarBuilder> margin(const Margin& m) {
        m_view->set_margin(m);
        return shared_from_this();
    }

    std::shared_ptr<TabBarBuilder> margin(int h, int v) {
        m_view->set_margin(h, v);
        return shared_from_this();
    }

    std::shared_ptr<TabBarBuilder> onTabSelected(std::function<void(int)> cb) {
        m_view->set_on_tab_selected_listener(std::move(cb));
        return shared_from_this();
    }

    std::shared_ptr<TabBar> build() {
        return m_view;
    }

private:
    std::shared_ptr<TabBar> m_view;
};

} // namespace miqu
