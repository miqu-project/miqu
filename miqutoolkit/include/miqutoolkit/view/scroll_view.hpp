#pragma once

#include "miqutoolkit/view/view_group.hpp"
#include "miqutoolkit/view/view_builder.hpp"
#include <memory>

namespace miqu {

class ScrollView : public ViewGroup {
public:
    ScrollView() = default;
    explicit ScrollView(std::shared_ptr<View> content) {
        set_content_view(std::move(content));
    }

    void set_content_view(std::shared_ptr<View> view);
    std::shared_ptr<View> get_content_view() const { return m_content; }

    void set_scroll_y(double y);
    double get_scroll_y() const { return m_scroll_y; }
    double get_max_scroll() const { return m_max_scroll; }

    void set_scrollbar_visible(bool visible) { m_show_scrollbar = visible; }
    bool is_scrollbar_visible() const { return m_show_scrollbar; }

    void scroll_to_view(View* view);
    void scroll_to_rect(const Rect& rect);

    void draw(cairo_t* cr, const Rect& bounds) override;
    Size measure_size() const override;
    Size measure_size(int avail_width) const override;

    bool on_mouse_move(int lx, int ly, const Rect& bounds) override;
    bool on_mouse_button(int lx, int ly, MouseButton button, bool pressed, const Rect& bounds) override;
    bool on_scroll(double delta, uint32_t axis_source = 0) override;
    bool on_touch(const TouchEvent& event, const Rect& bounds) override;

    std::shared_ptr<View> find_view_at(int lx, int ly, Rect* out_bounds = nullptr) override;

private:
    std::shared_ptr<View> m_content;
    double m_scroll_y = 0.0;
    double m_max_scroll = 0.0;
    bool m_show_scrollbar = true;

    bool m_is_hovered = false;
    bool m_dragging_thumb = false;
    int m_drag_start_y = 0;
    double m_drag_start_scroll = 0.0;

    int32_t m_touch_id = -1;
    double m_touch_start_x = 0.0;
    double m_touch_start_y = 0.0;
    double m_touch_last_y = 0.0;
    bool m_touch_scrolling = false;
    bool m_child_touch_target = false;
};

class ScrollViewBuilder : public ViewGroupBuilder<ScrollViewBuilder, ScrollView> {
public:
    using ViewGroupBuilder<ScrollViewBuilder, ScrollView>::ViewGroupBuilder;

    static std::shared_ptr<ScrollViewBuilder> create() {
        return std::make_shared<ScrollViewBuilder>();
    }

    std::shared_ptr<ScrollViewBuilder> contentView(std::shared_ptr<View> content) {
        m_view->set_content_view(std::move(content));
        return shared_from_this();
    }

    std::shared_ptr<ScrollViewBuilder> scrollbar(bool show) {
        m_view->set_scrollbar_visible(show);
        return shared_from_this();
    }
};

} // namespace miqu
