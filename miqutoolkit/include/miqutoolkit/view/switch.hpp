#pragma once

#include "miqutoolkit/view/view.hpp"
#include "miqutoolkit/view/view_builder.hpp"
#include <functional>

namespace miqu {

class Switch : public View {
public:
    Switch();
    ~Switch() override = default;

    void set_checked(bool checked);
    bool is_checked() const { return m_checked; }

    void toggle() {
        set_checked(!m_checked);
        if (m_on_checked_changed) {
            m_on_checked_changed(m_checked);
        }
    }

    void set_on_checked_changed_listener(std::function<void(bool is_checked)> cb) {
        m_on_checked_changed = std::move(cb);
    }

    void set_track_active_color(const Color& col) { m_track_active_color = col; m_has_custom_track_active = true; }
    void set_track_inactive_color(const Color& col) { m_track_inactive_color = col; m_has_custom_track_inactive = true; }
    void set_thumb_active_color(const Color& col) { m_thumb_active_color = col; m_has_custom_thumb_active = true; }
    void set_thumb_inactive_color(const Color& col) { m_thumb_inactive_color = col; m_has_custom_thumb_inactive = true; }

    void draw(cairo_t* cr, const Rect& bounds) override;
    void draw_focus_ring(cairo_t* cr, const Rect& bounds) override;
    Size measure_size() const override;

    bool on_mouse_move(int lx, int ly, const Rect& bounds) override;
    bool on_mouse_button(int lx, int ly, MouseButton button, bool pressed, const Rect& bounds) override;
    bool on_key(const KeyPressEvent& event) override;

private:
    bool m_checked = false;
    bool m_hovered = false;
    bool m_pressed = false;

    bool m_has_custom_track_active = false;
    Color m_track_active_color;

    bool m_has_custom_track_inactive = false;
    Color m_track_inactive_color;

    bool m_has_custom_thumb_active = false;
    Color m_thumb_active_color;

    bool m_has_custom_thumb_inactive = false;
    Color m_thumb_inactive_color;

    std::function<void(bool)> m_on_checked_changed;
};

class SwitchBuilder : public ViewBuilder<SwitchBuilder, Switch> {
public:
    using ViewBuilder<SwitchBuilder, Switch>::ViewBuilder;

    static std::shared_ptr<SwitchBuilder> create() {
        return std::make_shared<SwitchBuilder>();
    }

    std::shared_ptr<SwitchBuilder> checked(bool c) {
        m_view->set_checked(c);
        return shared_from_this();
    }

    std::shared_ptr<SwitchBuilder> onCheckedChanged(std::function<void(bool)> cb) {
        m_view->set_on_checked_changed_listener(std::move(cb));
        return shared_from_this();
    }

    std::shared_ptr<SwitchBuilder> trackActiveColor(const Color& c) {
        m_view->set_track_active_color(c);
        return shared_from_this();
    }

    std::shared_ptr<SwitchBuilder> trackInactiveColor(const Color& c) {
        m_view->set_track_inactive_color(c);
        return shared_from_this();
    }

    std::shared_ptr<SwitchBuilder> thumbActiveColor(const Color& c) {
        m_view->set_thumb_active_color(c);
        return shared_from_this();
    }

    std::shared_ptr<SwitchBuilder> thumbInactiveColor(const Color& c) {
        m_view->set_thumb_inactive_color(c);
        return shared_from_this();
    }
};

} // namespace miqu
