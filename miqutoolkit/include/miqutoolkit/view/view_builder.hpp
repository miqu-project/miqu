#pragma once

#include "miqutoolkit/view/view.hpp"
#include "miqutoolkit/view/view_group.hpp"
#include <memory>
#include <string>
#include <functional>

namespace miqu {

class PopupMenu;

/**
 * @brief Foundational CRTP builder providing universal configuration of all
 * base View attributes (layout params, geometry, padding, margins, visibility,
 * focus, tooltips, click & context listeners).
 */
template <typename Derived, typename ViewType>
class ViewBuilder : public std::enable_shared_from_this<Derived> {
public:
    ViewBuilder() : m_view(std::make_shared<ViewType>()) {}
    explicit ViewBuilder(std::shared_ptr<ViewType> v) : m_view(std::move(v)) {}
    virtual ~ViewBuilder() = default;

    // --- Layout & Dimensions ---
    std::shared_ptr<Derived> layoutParams(const LayoutParams& params) {
        m_view->set_layout_params(params);
        return this->shared_from_this();
    }

    std::shared_ptr<Derived> layoutParams(int w, int h) {
        m_view->set_layout_params(LayoutParams(w, h));
        return this->shared_from_this();
    }

    std::shared_ptr<Derived> layoutParams(int w, int h, float weight) {
        m_view->set_layout_params(LayoutParams(w, h, weight));
        return this->shared_from_this();
    }

    std::shared_ptr<Derived> layoutParams(int w, int h, Gravity g) {
        m_view->set_layout_params(LayoutParams(w, h, g));
        return this->shared_from_this();
    }

    std::shared_ptr<Derived> width(int w) {
        auto params = m_view->get_layout_params();
        params.width = w;
        m_view->set_layout_params(params);
        return this->shared_from_this();
    }

    std::shared_ptr<Derived> height(int h) {
        auto params = m_view->get_layout_params();
        params.height = h;
        m_view->set_layout_params(params);
        return this->shared_from_this();
    }

    std::shared_ptr<Derived> weight(float wt) {
        auto params = m_view->get_layout_params();
        params.weight = wt;
        m_view->set_layout_params(params);
        return this->shared_from_this();
    }

    std::shared_ptr<Derived> gravity(Gravity g) {
        auto params = m_view->get_layout_params();
        params.gravity = g;
        m_view->set_layout_params(params);
        return this->shared_from_this();
    }

    std::shared_ptr<Derived> bounds(const Rect& r) {
        m_view->set_bounds(r);
        return this->shared_from_this();
    }

    // --- Padding (Inner Spacing) ---
    std::shared_ptr<Derived> padding(const Padding& p) {
        m_view->set_padding(p);
        return this->shared_from_this();
    }

    std::shared_ptr<Derived> padding(int uniform) {
        m_view->set_padding(uniform);
        return this->shared_from_this();
    }

    std::shared_ptr<Derived> padding(int h, int v) {
        m_view->set_padding(h, v);
        return this->shared_from_this();
    }

    std::shared_ptr<Derived> padding(int l, int t, int r, int b) {
        m_view->set_padding(l, t, r, b);
        return this->shared_from_this();
    }

    // --- Margin (Outer Spacing) ---
    std::shared_ptr<Derived> margin(const Margin& m) {
        m_view->set_margin(m);
        return this->shared_from_this();
    }

    std::shared_ptr<Derived> margin(int uniform) {
        m_view->set_margin(uniform);
        return this->shared_from_this();
    }

    std::shared_ptr<Derived> margin(int h, int v) {
        m_view->set_margin(h, v);
        return this->shared_from_this();
    }

    std::shared_ptr<Derived> margin(int l, int t, int r, int b) {
        m_view->set_margin(l, t, r, b);
        return this->shared_from_this();
    }

    // --- Visibility & State ---
    std::shared_ptr<Derived> visibility(Visibility v) {
        m_view->set_visibility(v);
        return this->shared_from_this();
    }

    std::shared_ptr<Derived> visible(bool v = true) {
        m_view->set_visibility(v ? Visibility::Visible : Visibility::Gone);
        return this->shared_from_this();
    }

    std::shared_ptr<Derived> gone() {
        m_view->set_visibility(Visibility::Gone);
        return this->shared_from_this();
    }

    std::shared_ptr<Derived> invisible() {
        m_view->set_visibility(Visibility::Invisible);
        return this->shared_from_this();
    }

    std::shared_ptr<Derived> enabled(bool e = true) {
        m_view->set_enabled(e);
        return this->shared_from_this();
    }

    std::shared_ptr<Derived> focusable(bool f = true) {
        m_view->set_focusable(f);
        return this->shared_from_this();
    }

    std::shared_ptr<Derived> drawFocusRing(bool draw = true) {
        m_view->set_draw_focus_ring(draw);
        return this->shared_from_this();
    }

    std::shared_ptr<Derived> focusRingRadius(int radius) {
        m_view->set_focus_ring_radius(radius);
        return this->shared_from_this();
    }

    // --- Tooltips ---
    std::shared_ptr<Derived> tooltip(std::string text) {
        m_view->set_tooltip(std::move(text));
        return this->shared_from_this();
    }

    std::shared_ptr<Derived> tooltipShortcut(std::string shortcut) {
        m_view->set_tooltip_shortcut(std::move(shortcut));
        return this->shared_from_this();
    }

    // --- Event & Interaction Callbacks ---
    std::shared_ptr<Derived> onClick(std::function<void()> listener) {
        m_view->set_on_click_listener(std::move(listener));
        return this->shared_from_this();
    }

    std::shared_ptr<Derived> onContextClick(std::function<void(int global_x, int global_y)> listener) {
        m_view->set_on_context_click_listener(std::move(listener));
        return this->shared_from_this();
    }

    std::shared_ptr<Derived> contextMenu(std::shared_ptr<PopupMenu> menu) {
        m_view->set_context_menu(std::move(menu));
        return this->shared_from_this();
    }

    std::shared_ptr<Derived> contextMenuBuilder(std::function<std::shared_ptr<PopupMenu>(int x, int y)> builder) {
        m_view->set_context_menu_builder(std::move(builder));
        return this->shared_from_this();
    }

    // --- Final Build ---
    std::shared_ptr<ViewType> build() {
        return m_view;
    }

protected:
    std::shared_ptr<ViewType> m_view;
};

/**
 * @brief Foundational CRTP builder for container ViewGroups, providing universal
 * child hierarchy additions, background styling, strokes, and corner rounding.
 */
template <typename Derived, typename ViewGroupType>
class ViewGroupBuilder : public ViewBuilder<Derived, ViewGroupType> {
public:
    using ViewBuilder<Derived, ViewGroupType>::ViewBuilder;

    std::shared_ptr<Derived> addView(std::shared_ptr<View> child) {
        this->m_view->add_view(std::move(child));
        return this->shared_from_this();
    }

    std::shared_ptr<Derived> addView(std::shared_ptr<View> child, const LayoutParams& params) {
        this->m_view->add_view(std::move(child), params);
        return this->shared_from_this();
    }

    std::shared_ptr<Derived> backgroundColor(const Color& color) {
        this->m_view->set_background_color(color);
        return this->shared_from_this();
    }

    std::shared_ptr<Derived> stroke(int width, const Color& color) {
        this->m_view->set_stroke(width, color);
        return this->shared_from_this();
    }

    std::shared_ptr<Derived> cornerRadius(int radius) {
        this->m_view->set_corner_radius(radius);
        return this->shared_from_this();
    }

    std::shared_ptr<Derived> focusScope(bool scope = true) {
        this->m_view->set_focus_scope(scope);
        return this->shared_from_this();
    }
};

} // namespace miqu
