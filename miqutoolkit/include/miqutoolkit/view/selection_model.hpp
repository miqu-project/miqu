#pragma once

#include <unordered_set>
#include <vector>
#include <functional>
#include <algorithm>
#include <cstddef>

namespace miqu {

/**
 * SelectionMode defines the selection behavior for item views.
 */
enum class SelectionMode {
    None,       // Selection disabled
    Single,     // Only one item can be selected at a time (cursor is the selected item)
    Multiple    // Multiple items can be marked/selected (hybrid Yazi/Desktop model)
};

/**
 * SelectionModel is a first-class model that encapsulates selection state,
 * active cursor focus, range anchors, and interaction semantics across views.
 */
class SelectionModel {
public:
    using SelectionChangedCallback = std::function<void(const std::unordered_set<int>&)>;
    using CursorChangedCallback = std::function<void(int)>;

    SelectionModel();
    explicit SelectionModel(SelectionMode mode);
    ~SelectionModel() = default;

    // --- Mode ---
    SelectionMode mode() const { return m_mode; }
    void set_mode(SelectionMode mode);

    // --- Cursor & Anchor ---
    // The cursor is the active item (navigated row / tile).
    int cursor() const { return m_cursor; }
    void set_cursor(int index, bool notify = true);

    // The anchor is the origin for Shift-range operations.
    int anchor() const { return m_anchor; }
    void set_anchor(int index) { m_anchor = index; }

    // --- Selection Queries ---
    // In Multiple mode:
    //   If explicit marks exist, returns true iff index is marked.
    //   If NO explicit marks exist, returns true iff index == cursor (provided cursor >= 0).
    // In Single mode:
    //   Returns true iff index == cursor (provided cursor >= 0).
    // In None mode:
    //   Always returns false.
    bool is_selected(int index) const;

    // Returns true if index is the active cursor.
    bool is_cursor(int index) const { return m_cursor == index && m_cursor >= 0; }

    // Returns true if index is explicitly marked in the multi-select set.
    bool is_marked(int index) const { return m_marks.find(index) != m_marks.end(); }

    // The set of explicitly marked indices.
    const std::unordered_set<int>& marks() const { return m_marks; }
    bool has_marks() const { return !m_marks.empty(); }

    // Total number of effectively selected items.
    // In Multiple mode with marks, returns marks.size().
    // If no marks exist and cursor >= 0, returns 1.
    // Otherwise 0.
    size_t selection_count() const;

    // Returns effective selected indices in ascending sorted order.
    std::vector<int> effective_indices() const;

    // --- Direct State Mutation ---
    // Selects a single item exclusively (clears marks, sets cursor & anchor to index).
    void select_single(int index);

    // Adds index to marked items (in Multiple mode).
    void select(int index);

    // Removes index from marked items.
    void deselect(int index);

    // Toggles index in marked items.
    void toggle(int index);

    // Replaces marked items with range [from, to] inclusive.
    void select_range(int from, int to);

    // Marks all items [0, total_count - 1].
    void select_all(size_t total_count);

    // Replaces marked set directly.
    void set_marks(const std::unordered_set<int>& indices);

    // Clears all explicit marks, keeping cursor and resetting anchor to cursor.
    void clear_marks();

    // Fully resets cursor, anchor, and marks.
    void reset(bool notify = true);

    // --- High-Level User Interaction Handlers ---

    // Left mouse click on index with modifier keys.
    // Returns true if selection or cursor changed.
    bool handle_click(int index, bool shift, bool ctrl, size_t total_count);

    // Right mouse click (context click) on index.
    // Preserves existing marks if index is already marked.
    // Otherwise selects index exclusively.
    // Returns true if state changed.
    bool handle_context_click(int index);

    // Keyboard Space:
    // Toggles mark on current cursor and optionally steps cursor to next index.
    // Returns true if handled.
    bool handle_space(size_t total_count, bool step = true);

    // Keyboard Navigation (Up, Down, Left, Right, PageUp, PageDown, Home, End):
    // target_cursor: destination index
    // shift: Shift modifier held for range selection
    // Returns true if cursor or selection changed.
    bool handle_nav(int target_cursor, bool shift);

    // Keyboard Escape:
    // Clears explicit marks if any exist, collapsing back to single cursor.
    // Returns true if marks were cleared, false if there were no marks to clear.
    bool handle_escape();

    // Keyboard Ctrl+A:
    // Selects all items [0, total_count - 1].
    // Returns true if handled.
    bool handle_select_all(size_t total_count);

    // --- Observer / Listener Support ---

    size_t add_selection_changed_listener(SelectionChangedCallback cb);
    void remove_selection_changed_listener(size_t id);

    size_t add_cursor_changed_listener(CursorChangedCallback cb);
    void remove_cursor_changed_listener(size_t id);

    // Convenience setters
    void set_on_selection_changed(SelectionChangedCallback cb);
    void set_on_cursor_changed(CursorChangedCallback cb);

private:
    void notify_selection();
    void notify_cursor();

    SelectionMode m_mode = SelectionMode::Single;
    int m_cursor = -1;
    int m_anchor = -1;
    std::unordered_set<int> m_marks;

    struct SelectionListenerEntry {
        size_t id;
        SelectionChangedCallback cb;
    };
    struct CursorListenerEntry {
        size_t id;
        CursorChangedCallback cb;
    };

    size_t m_next_listener_id = 1;
    std::vector<SelectionListenerEntry> m_selection_listeners;
    std::vector<CursorListenerEntry> m_cursor_listeners;
};

} // namespace miqu
