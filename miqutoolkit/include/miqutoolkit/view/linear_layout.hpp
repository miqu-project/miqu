#pragma once

#include "miqutoolkit/view/view_group.hpp"
#include "miqutoolkit/view/view_builder.hpp"

namespace miqu {

class LinearLayout : public ViewGroup {
public:
    LinearLayout() = default;
    explicit LinearLayout(Orientation orient) : m_orientation(orient) {}

    void set_orientation(Orientation orient) { m_orientation = orient; }
    Orientation get_orientation() const { return m_orientation; }

    void set_gravity(Gravity g) { m_gravity = g; }
    Gravity get_gravity() const { return m_gravity; }

    void set_divider_spacing(int spacing) { m_spacing = spacing; }
    int get_divider_spacing() const { return m_spacing; }

    void draw(cairo_t* cr, const Rect& bounds) override;
    Size measure_size() const override;
    Size measure_size(int avail_width) const override;

private:
    Orientation m_orientation = Orientation::Vertical;
    Gravity m_gravity = Gravity::None;
    int m_spacing = 0;
};

class LinearLayoutBuilder : public ViewGroupBuilder<LinearLayoutBuilder, LinearLayout> {
public:
    using ViewGroupBuilder<LinearLayoutBuilder, LinearLayout>::ViewGroupBuilder;

    static std::shared_ptr<LinearLayoutBuilder> create() {
        return std::make_shared<LinearLayoutBuilder>();
    }

    std::shared_ptr<LinearLayoutBuilder> orientation(Orientation o) {
        m_view->set_orientation(o);
        return shared_from_this();
    }

    std::shared_ptr<LinearLayoutBuilder> gravity(Gravity g) {
        m_view->set_gravity(g);
        return shared_from_this();
    }

    std::shared_ptr<LinearLayoutBuilder> spacing(int s) {
        m_view->set_divider_spacing(s);
        return shared_from_this();
    }
};

} // namespace miqu
