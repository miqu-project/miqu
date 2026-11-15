#pragma once

#include "miqutoolkit/view/view_builder.hpp"
#include <algorithm>

namespace miqu {

class DividerView : public View {
public:
    DividerView(Orientation orientation = Orientation::Horizontal, int thickness = 1)
        : m_orientation(orientation), m_thickness(std::max(1, thickness)) {}

    void set_orientation(Orientation o) { m_orientation = o; }
    Orientation get_orientation() const { return m_orientation; }

    void set_thickness(int t) { m_thickness = std::max(1, t); }
    int get_thickness() const { return m_thickness; }

    void set_color(const Color& color) { m_color = color; m_has_custom_color = true; }

    void draw(cairo_t* cr, const Rect& bounds) override;
    Size measure_size() const override;

private:
    Orientation m_orientation = Orientation::Horizontal;
    int m_thickness = 1;
    bool m_has_custom_color = false;
    Color m_color;
};

class DividerViewBuilder : public ViewBuilder<DividerViewBuilder, DividerView> {
public:
    DividerViewBuilder() : ViewBuilder<DividerViewBuilder, DividerView>() {}
    explicit DividerViewBuilder(Orientation o, int thickness = 1) : ViewBuilder<DividerViewBuilder, DividerView>() {
        m_view->set_orientation(o);
        m_view->set_thickness(thickness);
    }

    static std::shared_ptr<DividerViewBuilder> create() {
        return std::make_shared<DividerViewBuilder>();
    }

    static std::shared_ptr<DividerViewBuilder> create(Orientation o, int thickness = 1) {
        return std::make_shared<DividerViewBuilder>(o, thickness);
    }

    std::shared_ptr<DividerViewBuilder> orientation(Orientation o) {
        m_view->set_orientation(o);
        return shared_from_this();
    }

    std::shared_ptr<DividerViewBuilder> horizontal(int thickness = 1) {
        m_view->set_orientation(Orientation::Horizontal);
        m_view->set_thickness(thickness);
        return shared_from_this();
    }

    std::shared_ptr<DividerViewBuilder> vertical(int thickness = 1) {
        m_view->set_orientation(Orientation::Vertical);
        m_view->set_thickness(thickness);
        return shared_from_this();
    }

    std::shared_ptr<DividerViewBuilder> thickness(int t) {
        m_view->set_thickness(t);
        return shared_from_this();
    }

    std::shared_ptr<DividerViewBuilder> color(const Color& c) {
        m_view->set_color(c);
        return shared_from_this();
    }
};

} // namespace miqu
