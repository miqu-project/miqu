#pragma once

#include "miqutoolkit/view/view.hpp"
#include "miqutoolkit/view/view_builder.hpp"
#include "miqutoolkit/view/kinetic_scroller.hpp"
#include "miqutoolkit/view/selection_model.hpp"
#include <vector>
#include <memory>
#include <functional>
#include <unordered_set>

namespace miqu {

enum class StretchMode {
    None,
    SpacingWidth,
    ColumnWidth,
};

class GridView : public View {
public:
    GridView();
    ~GridView() override;

    void set_num_columns(int cols) { m_cols = std::max(1, cols); m_auto_fit = false; }
    void set_auto_fit(int min_column_width = 100) { m_min_col_w = std::max(20, min_column_width); m_auto_fit = true; }
    void set_cell_size(int w, int h) { m_cell_w = w; m_cell_h = h; }
    void set_cell_height(int h) { m_cell_h = h; }
    void set_horizontal_spacing(int sx) { m_space_x = sx; }
    void set_vertical_spacing(int sy) { m_space_y = sy; }
    void set_stretch_mode(StretchMode mode) { m_stretch_mode = mode; }

    using ItemProviderCallback = std::function<std::shared_ptr<View>(size_t index)>;

    // Item management
    void set_items(std::vector<std::shared_ptr<View>> items);
    void add_item(std::shared_ptr<View> item);
    void set_item_provider(size_t total_count, ItemProviderCallback provider);
    void clear_items();
    size_t get_item_count() const {
        if (m_item_provider) return m_virtual_count;
        return m_items.size();
    }
    std::shared_ptr<View> get_item_at(size_t index) const;

    // Selection model
    void set_selection_model(std::shared_ptr<SelectionModel> model);
    std::shared_ptr<SelectionModel> get_selection_model() const { return m_selection_model; }

    void set_selection_mode(SelectionMode mode);
    SelectionMode get_selection_mode() const;
    bool is_multi_select() const;

    void set_selection_border(bool enable) { m_selection_border = enable; }
    bool has_selection_border() const { return m_selection_border; }

    void set_keyboard_navigation_enabled(bool enabled) { m_keyboard_navigation_enabled = enabled; }
    bool is_keyboard_navigation_enabled() const { return m_keyboard_navigation_enabled; }

    // Cursor & items
    void set_selected_index(int index);
    int get_selected_index() const;
    std::shared_ptr<View> get_selected_item() const;
    std::vector<std::shared_ptr<View>> get_selected_items() const;

    // Selection queries & manipulation
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

    // Event listeners
    void set_on_item_click_listener(std::function<void(size_t index, std::shared_ptr<View> item)> cb) {
        m_on_item_click = std::move(cb);
    }
    void set_on_selection_changed(std::function<void(const std::unordered_set<int>&)> cb) {
        m_on_selection_changed = std::move(cb);
    }
    void set_on_cursor_changed(std::function<void(int)> cb) {
        m_on_cursor_changed = std::move(cb);
    }
    void set_on_item_context_click_listener(std::function<void(size_t index, std::shared_ptr<View> item, int lx, int ly)> cb) {
        m_on_item_context_click = std::move(cb);
    }
    void set_on_empty_context_click_listener(std::function<void(int lx, int ly)> cb) {
        m_on_empty_context_click = std::move(cb);
    }

    void draw(cairo_t* cr, const Rect& bounds) override;
    bool on_key(const KeyPressEvent& event) override;
    bool on_mouse_move(int lx, int ly, const Rect& bounds) override;
    bool on_mouse_button(int lx, int ly, MouseButton button, bool pressed, const Rect& bounds) override;
    bool on_scroll(double delta, uint32_t axis_source = 0) override;
    bool on_scroll_stop(uint32_t axis_source = 0) override;
    bool on_touch(const TouchEvent& event, const Rect& bounds) override;

private:
    void setup_selection_model_listeners();
    void teardown_selection_model_listeners();
    int compute_columns(int bounds_w, int& out_cell_w) const;
    void ensure_visible(int viewport_height, int cols, int row_stride);
    int item_at(int lx, int ly, const Rect& bounds) const;

    bool m_auto_fit = true;
    int m_min_col_w = 100;
    int m_cols = 5;
    int m_cell_w = 100;
    int m_cell_h = 100;
    int m_space_x = 10;
    int m_space_y = 10;
    StretchMode m_stretch_mode = StretchMode::ColumnWidth;

    std::shared_ptr<SelectionModel> m_selection_model;
    size_t m_selection_listener_id = 0;
    size_t m_cursor_listener_id = 0;
    bool m_keyboard_navigation_enabled = true;

    int m_hovered_index = -1;
    int32_t m_touch_id = -1;
    double m_touch_start_x = 0.0;
    double m_touch_start_y = 0.0;
    double m_touch_last_y = 0.0;
    bool m_touch_scrolling = false;
    double m_scroll_y = 0.0;
    KineticScroller m_kinetic;
    mutable int m_last_width = 0;
    mutable int m_last_height = 0;
    mutable int m_effective_cols = 5;
    mutable int m_effective_cell_w = 100;

    bool m_selection_border = true;
    std::vector<std::shared_ptr<View>> m_items;
    size_t m_virtual_count = 0;
    ItemProviderCallback m_item_provider;

    std::function<void(size_t, std::shared_ptr<View>)> m_on_item_click;
    std::function<void(const std::unordered_set<int>&)> m_on_selection_changed;
    std::function<void(int)> m_on_cursor_changed;
    std::function<void(size_t, std::shared_ptr<View>, int, int)> m_on_item_context_click;
    std::function<void(int, int)> m_on_empty_context_click;
};

class GridViewBuilder : public ViewBuilder<GridViewBuilder, GridView> {
public:
    using ViewBuilder<GridViewBuilder, GridView>::ViewBuilder;

    static std::shared_ptr<GridViewBuilder> create() {
        return std::make_shared<GridViewBuilder>();
    }

    std::shared_ptr<GridViewBuilder> itemProvider(size_t total_count, GridView::ItemProviderCallback provider) {
        m_view->set_item_provider(total_count, std::move(provider));
        return shared_from_this();
    }

    std::shared_ptr<GridViewBuilder> numColumns(int cols) {
        m_view->set_num_columns(cols);
        return shared_from_this();
    }

    std::shared_ptr<GridViewBuilder> autoFit(int min_column_width = 100) {
        m_view->set_auto_fit(min_column_width);
        return shared_from_this();
    }

    std::shared_ptr<GridViewBuilder> cellSize(int w, int h) {
        m_view->set_cell_size(w, h);
        return shared_from_this();
    }

    std::shared_ptr<GridViewBuilder> cellHeight(int h) {
        m_view->set_cell_height(h);
        return shared_from_this();
    }

    std::shared_ptr<GridViewBuilder> spacing(int sx, int sy) {
        m_view->set_horizontal_spacing(sx);
        m_view->set_vertical_spacing(sy);
        return shared_from_this();
    }

    std::shared_ptr<GridViewBuilder> stretchMode(StretchMode mode) {
        m_view->set_stretch_mode(mode);
        return shared_from_this();
    }

    std::shared_ptr<GridViewBuilder> items(std::vector<std::shared_ptr<View>> items) {
        m_view->set_items(std::move(items));
        return shared_from_this();
    }

    std::shared_ptr<GridViewBuilder> selectionModel(std::shared_ptr<SelectionModel> model) {
        m_view->set_selection_model(std::move(model));
        return shared_from_this();
    }

    std::shared_ptr<GridViewBuilder> selectionMode(SelectionMode mode) {
        m_view->set_selection_mode(mode);
        return shared_from_this();
    }

    std::shared_ptr<GridViewBuilder> multiSelect(bool enabled = true) {
        m_view->set_selection_mode(enabled ? SelectionMode::Multiple : SelectionMode::Single);
        return shared_from_this();
    }

    std::shared_ptr<GridViewBuilder> selectionBorder(bool enable) {
        m_view->set_selection_border(enable);
        return shared_from_this();
    }

    std::shared_ptr<GridViewBuilder> keyboardNavigation(bool enable) {
        m_view->set_keyboard_navigation_enabled(enable);
        return shared_from_this();
    }

    std::shared_ptr<GridViewBuilder> onItemClick(std::function<void(size_t, std::shared_ptr<View>)> cb) {
        m_view->set_on_item_click_listener(std::move(cb));
        return shared_from_this();
    }

    std::shared_ptr<GridViewBuilder> onSelectionChanged(std::function<void(const std::unordered_set<int>&)> cb) {
        m_view->set_on_selection_changed(std::move(cb));
        return shared_from_this();
    }

    std::shared_ptr<GridViewBuilder> onCursorChanged(std::function<void(int)> cb) {
        m_view->set_on_cursor_changed(std::move(cb));
        return shared_from_this();
    }

    std::shared_ptr<GridViewBuilder> onItemContextClick(std::function<void(size_t, std::shared_ptr<View>, int, int)> cb) {
        m_view->set_on_item_context_click_listener(std::move(cb));
        return shared_from_this();
    }

    std::shared_ptr<GridViewBuilder> onEmptyContextClick(std::function<void(int, int)> cb) {
        m_view->set_on_empty_context_click_listener(std::move(cb));
        return shared_from_this();
    }
};

} // namespace miqu
