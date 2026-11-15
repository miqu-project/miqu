#pragma once

#include "miqutoolkit/view/view.hpp"
#include "miqutoolkit/view/view_builder.hpp"
#include "miqutoolkit/view/selection_model.hpp"
#include <vector>
#include <memory>
#include <functional>
#include <unordered_set>

namespace miqu {

class ListView : public View {
public:
    ListView();
    ~ListView() override;

    void set_item_height(int h) { m_item_h = std::max(16, h); }
    int get_item_height() const { return m_item_h; }

    void set_spacing(int spacing) { m_spacing = std::max(0, spacing); }
    int get_spacing() const { return m_spacing; }

    void set_item_corner_radius(int radius) { m_corner_radius = radius; }
    int get_item_corner_radius() const { return m_corner_radius; }

    void set_show_dividers(bool show) { m_show_dividers = show; }
    bool get_show_dividers() const { return m_show_dividers; }

    void set_scrollbar_enabled(bool enabled) { m_scrollbar_enabled = enabled; request_redraw(); }
    bool is_scrollbar_enabled() const { return m_scrollbar_enabled; }

    // Item management
    void set_items(std::vector<std::shared_ptr<View>> items);
    void add_item(std::shared_ptr<View> item);
    void clear_items();
    size_t get_item_count() const { return m_items.size(); }
    std::shared_ptr<View> get_item_at(size_t index) const;

    // Selection Model
    void set_selection_model(std::shared_ptr<SelectionModel> model);
    std::shared_ptr<SelectionModel> get_selection_model() const { return m_selection_model; }

    void set_selectable(bool selectable);
    bool is_selectable() const { return m_selectable; }

    void set_selection_mode(SelectionMode mode);
    SelectionMode get_selection_mode() const;
    bool is_multi_select() const;

    const std::unordered_set<int>& get_selected_indices() const;
    bool is_index_selected(int index) const;
    size_t get_selected_count() const;

    void set_selected_indices(const std::unordered_set<int>& indices);
    void add_selected_index(int index);
    void remove_selected_index(int index);
    void toggle_selected_index(int index);
    void select_range(int from, int to);
    void select_all();
    void clear_selection();

    int get_anchor_index() const;
    void set_anchor_index(int index);

    void set_selected_index(int index);
    int get_selected_index() const;
    std::shared_ptr<View> get_selected_item() const;
    std::vector<std::shared_ptr<View>> get_selected_items() const;

    void set_on_selection_changed(std::function<void(const std::unordered_set<int>&)> cb) {
        m_on_selection_changed = std::move(cb);
    }
    void set_on_cursor_changed(std::function<void(int)> cb) {
        m_on_cursor_changed = std::move(cb);
    }

    void set_keyboard_navigation_enabled(bool enabled) { m_keyboard_navigation_enabled = enabled; }
    bool is_keyboard_navigation_enabled() const { return m_keyboard_navigation_enabled; }

    // Event listeners
    void set_on_item_click_listener(std::function<void(size_t index, std::shared_ptr<View> item)> cb) {
        m_on_item_click = std::move(cb);
    }
    void set_on_item_context_click_listener(std::function<void(size_t index, std::shared_ptr<View> item, int x, int y)> cb) {
        m_on_item_context_click = std::move(cb);
    }
    void set_on_empty_context_click_listener(std::function<void(int x, int y)> cb) {
        m_on_empty_context_click = std::move(cb);
    }

    void draw(cairo_t* cr, const Rect& bounds) override;
    bool on_key(const KeyPressEvent& event) override;
    bool on_mouse_move(int lx, int ly, const Rect& bounds) override;
    bool on_mouse_button(int lx, int ly, MouseButton button, bool pressed, const Rect& bounds) override;
    bool on_scroll(double delta) override;
    bool on_touch(const TouchEvent& event, const Rect& bounds) override;

private:
    void setup_selection_model_listeners();
    void teardown_selection_model_listeners();
    void ensure_visible(int viewport_height);
    int item_at(int lx, int ly, const Rect& bounds) const;

    int m_item_h = 44;
    int m_spacing = 2;
    int m_corner_radius = 6;
    bool m_show_dividers = false;
    bool m_scrollbar_enabled = true;

    bool m_selectable = true;
    bool m_keyboard_navigation_enabled = true;
    std::shared_ptr<SelectionModel> m_selection_model;
    size_t m_selection_listener_id = 0;
    size_t m_cursor_listener_id = 0;

    int m_hovered_index = -1;
    std::function<void(const std::unordered_set<int>&)> m_on_selection_changed;
    std::function<void(int)> m_on_cursor_changed;

    int32_t m_touch_id = -1;
    double m_touch_start_x = 0.0;
    double m_touch_start_y = 0.0;
    double m_touch_last_y = 0.0;
    bool m_touch_scrolling = false;
    double m_scroll_y = 0.0;
    mutable int m_last_height = 0;
    bool m_scroll_to_selected = false;

    std::vector<std::shared_ptr<View>> m_items;
    std::function<void(size_t, std::shared_ptr<View>)> m_on_item_click;
    std::function<void(size_t, std::shared_ptr<View>, int, int)> m_on_item_context_click;
    std::function<void(int, int)> m_on_empty_context_click;
};

class ListViewBuilder : public ViewBuilder<ListViewBuilder, ListView> {
public:
    using ViewBuilder<ListViewBuilder, ListView>::ViewBuilder;

    static std::shared_ptr<ListViewBuilder> create() {
        return std::make_shared<ListViewBuilder>();
    }

    std::shared_ptr<ListViewBuilder> itemHeight(int h) {
        m_view->set_item_height(h);
        return shared_from_this();
    }

    std::shared_ptr<ListViewBuilder> spacing(int spacing) {
        m_view->set_spacing(spacing);
        return shared_from_this();
    }

    std::shared_ptr<ListViewBuilder> itemCornerRadius(int r) {
        m_view->set_item_corner_radius(r);
        return shared_from_this();
    }

    std::shared_ptr<ListViewBuilder> showDividers(bool show) {
        m_view->set_show_dividers(show);
        return shared_from_this();
    }

    std::shared_ptr<ListViewBuilder> scrollbar(bool enabled) {
        m_view->set_scrollbar_enabled(enabled);
        return shared_from_this();
    }

    std::shared_ptr<ListViewBuilder> selectable(bool selectable) {
        m_view->set_selectable(selectable);
        return shared_from_this();
    }

    std::shared_ptr<ListViewBuilder> selectionModel(std::shared_ptr<SelectionModel> model) {
        m_view->set_selection_model(std::move(model));
        return shared_from_this();
    }

    std::shared_ptr<ListViewBuilder> selectionMode(SelectionMode mode) {
        m_view->set_selection_mode(mode);
        return shared_from_this();
    }

    std::shared_ptr<ListViewBuilder> multiSelect(bool enabled = true) {
        m_view->set_selection_mode(enabled ? SelectionMode::Multiple : SelectionMode::Single);
        return shared_from_this();
    }

    std::shared_ptr<ListViewBuilder> keyboardNavigation(bool enabled) {
        m_view->set_keyboard_navigation_enabled(enabled);
        return shared_from_this();
    }

    std::shared_ptr<ListViewBuilder> items(std::vector<std::shared_ptr<View>> items) {
        m_view->set_items(std::move(items));
        return shared_from_this();
    }

    std::shared_ptr<ListViewBuilder> onSelectionChanged(std::function<void(const std::unordered_set<int>&)> cb) {
        m_view->set_on_selection_changed(std::move(cb));
        return shared_from_this();
    }

    std::shared_ptr<ListViewBuilder> onCursorChanged(std::function<void(int)> cb) {
        m_view->set_on_cursor_changed(std::move(cb));
        return shared_from_this();
    }

    std::shared_ptr<ListViewBuilder> onItemClick(std::function<void(size_t, std::shared_ptr<View>)> cb) {
        m_view->set_on_item_click_listener(std::move(cb));
        return shared_from_this();
    }

    std::shared_ptr<ListViewBuilder> onItemContextClick(std::function<void(size_t, std::shared_ptr<View>, int, int)> cb) {
        m_view->set_on_item_context_click_listener(std::move(cb));
        return shared_from_this();
    }

    std::shared_ptr<ListViewBuilder> onEmptyContextClick(std::function<void(int, int)> cb) {
        m_view->set_on_empty_context_click_listener(std::move(cb));
        return shared_from_this();
    }
};

} // namespace miqu
