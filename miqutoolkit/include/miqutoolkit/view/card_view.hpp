#pragma once

#include "miqutoolkit/view/frame_layout.hpp"
#include "miqutoolkit/view/view_builder.hpp"

namespace miqu {

enum class CardStyle {
    Filled,    // Modern borderless surface card (default)
    Outlined,  // Card with outline border
    Elevated   // Elevated card with subtle ambient drop shadow
};

class CardView : public FrameLayout {
public:
    CardView() {
        m_corner_radius = -1; // -1 denotes default from theme metrics
        m_style = CardStyle::Filled;
        m_elevation = -1.0;   // -1.0 denotes default from theme metrics
    }

    void set_card_background_color(const Color& color) { set_background_color(color); }
    const Color& get_card_background_color() const { return get_background_color(); }

    void set_radius(int radius) { set_corner_radius(radius); }
    int get_radius() const { return get_corner_radius(); }

    void set_style(CardStyle style) { m_style = style; }
    CardStyle get_style() const { return m_style; }

    void set_borderless(bool borderless = true) {
        m_style = borderless ? CardStyle::Filled : CardStyle::Outlined;
    }
    bool is_borderless() const { return m_style != CardStyle::Outlined && m_stroke_width <= 0; }

    void set_bordered(bool bordered = true) {
        m_style = bordered ? CardStyle::Outlined : CardStyle::Filled;
    }
    bool is_bordered() const { return m_style == CardStyle::Outlined || m_stroke_width > 0; }

    void set_elevation(double elevation) { m_elevation = elevation; }
    double get_elevation() const { return m_elevation; }

    void draw(cairo_t* cr, const Rect& bounds) override;

    using ViewGroup::add_view;
    void add_view(std::shared_ptr<View> child) override {
        if (!child) return;
        if (child->get_layout_params().width == static_cast<int>(LayoutDimension::WrapContent)) {
            auto params = child->get_layout_params();
            params.width = static_cast<int>(LayoutDimension::MatchParent);
            child->set_layout_params(params);
        }
        FrameLayout::add_view(std::move(child));
    }
    void add_view(std::shared_ptr<View> child, const LayoutParams& params) override {
        if (!child) return;
        LayoutParams p = params;
        if (p.width == static_cast<int>(LayoutDimension::WrapContent)) {
            p.width = static_cast<int>(LayoutDimension::MatchParent);
        }
        FrameLayout::add_view(std::move(child), p);
    }

    static void draw_rounded_rect(cairo_t* cr, double x, double y, double w, double h, double r);
    static void draw_elevation_shadow(cairo_t* cr, double x, double y, double w, double h, double radius, double elevation);
    static void draw_elevation_shadow(cairo_t* cr, const Rect& bounds, double radius, double elevation) {
        draw_elevation_shadow(cr, bounds.x, bounds.y, bounds.width, bounds.height, radius, elevation);
    }

private:
    CardStyle m_style = CardStyle::Filled;
    double m_elevation = -1.0;
};

class CardViewBuilder : public ViewGroupBuilder<CardViewBuilder, CardView> {
public:
    using ViewGroupBuilder<CardViewBuilder, CardView>::ViewGroupBuilder;

    static std::shared_ptr<CardViewBuilder> create() {
        return std::make_shared<CardViewBuilder>();
    }

    std::shared_ptr<CardViewBuilder> style(CardStyle s) {
        m_view->set_style(s);
        return shared_from_this();
    }

    std::shared_ptr<CardViewBuilder> borderless(bool b = true) {
        m_view->set_borderless(b);
        return shared_from_this();
    }

    std::shared_ptr<CardViewBuilder> bordered(bool b = true) {
        m_view->set_bordered(b);
        return shared_from_this();
    }

    std::shared_ptr<CardViewBuilder> outlined(bool o = true) {
        m_view->set_bordered(o);
        return shared_from_this();
    }

    std::shared_ptr<CardViewBuilder> elevation(double elev) {
        m_view->set_elevation(elev);
        return shared_from_this();
    }
};

} // namespace miqu
