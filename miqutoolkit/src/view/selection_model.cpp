#include "miqutoolkit/view/selection_model.hpp"

namespace miqu {

SelectionModel::SelectionModel()
    : m_mode(SelectionMode::Single) {}

SelectionModel::SelectionModel(SelectionMode mode)
    : m_mode(mode) {}

void SelectionModel::set_mode(SelectionMode mode) {
    if (m_mode == mode) return;
    m_mode = mode;
    if (m_mode == SelectionMode::Single) {
        clear_marks();
    } else if (m_mode == SelectionMode::None) {
        reset();
    }
}

void SelectionModel::set_cursor(int index, bool notify) {
    if (m_cursor == index) return;
    m_cursor = index;
    if (m_anchor < 0) {
        m_anchor = index;
    }
    if (notify) {
        notify_cursor();
        if (m_marks.empty()) {
            notify_selection();
        }
    }
}

bool SelectionModel::is_selected(int index) const {
    if (m_mode == SelectionMode::None || index < 0) return false;
    if (m_mode == SelectionMode::Multiple && !m_marks.empty()) {
        return m_marks.find(index) != m_marks.end();
    }
    return index == m_cursor && m_cursor >= 0;
}

size_t SelectionModel::selection_count() const {
    if (m_mode == SelectionMode::None) return 0;
    if (m_mode == SelectionMode::Multiple && !m_marks.empty()) {
        return m_marks.size();
    }
    return (m_cursor >= 0) ? 1 : 0;
}

std::vector<int> SelectionModel::effective_indices() const {
    if (m_mode == SelectionMode::None) return {};
    if (m_mode == SelectionMode::Multiple && !m_marks.empty()) {
        std::vector<int> res(m_marks.begin(), m_marks.end());
        std::sort(res.begin(), res.end());
        return res;
    }
    if (m_cursor >= 0) {
        return {m_cursor};
    }
    return {};
}

void SelectionModel::select_single(int index) {
    m_cursor = index;
    m_anchor = index;
    m_marks.clear();
    notify_cursor();
    notify_selection();
}

void SelectionModel::select(int index) {
    if (m_mode != SelectionMode::Multiple || index < 0) return;
    if (m_marks.insert(index).second) {
        notify_selection();
    }
}

void SelectionModel::deselect(int index) {
    if (m_marks.erase(index) > 0) {
        notify_selection();
    }
}

void SelectionModel::toggle(int index) {
    if (m_mode != SelectionMode::Multiple || index < 0) return;
    auto it = m_marks.find(index);
    if (it != m_marks.end()) {
        m_marks.erase(it);
    } else {
        m_marks.insert(index);
    }
    notify_selection();
}

void SelectionModel::select_range(int from, int to) {
    if (m_mode != SelectionMode::Multiple) return;
    m_marks.clear();
    int s = std::min(from, to);
    int e = std::max(from, to);
    for (int i = s; i <= e; ++i) {
        m_marks.insert(i);
    }
    notify_selection();
}

void SelectionModel::select_all(size_t total_count) {
    if (m_mode != SelectionMode::Multiple || total_count == 0) return;
    m_marks.clear();
    for (size_t i = 0; i < total_count; ++i) {
        m_marks.insert(static_cast<int>(i));
    }
    notify_selection();
}

void SelectionModel::set_marks(const std::unordered_set<int>& indices) {
    m_marks = indices;
    notify_selection();
}

void SelectionModel::clear_marks() {
    if (!m_marks.empty()) {
        m_marks.clear();
        m_anchor = m_cursor;
        notify_selection();
    }
}

void SelectionModel::reset(bool notify) {
    m_cursor = -1;
    m_anchor = -1;
    m_marks.clear();
    if (notify) {
        notify_cursor();
        notify_selection();
    }
}

bool SelectionModel::handle_click(int index, bool shift, bool ctrl, size_t total_count) {
    if (m_mode == SelectionMode::None || index < 0 || index >= static_cast<int>(total_count)) {
        return false;
    }
    if (m_mode == SelectionMode::Single) {
        select_single(index);
        return true;
    }

    if (shift) {
        int from = (m_anchor >= 0 ? m_anchor : (m_cursor >= 0 ? m_cursor : index));
        int to = index;
        int s = std::min(from, to);
        int e = std::max(from, to);
        m_marks.clear();
        for (int i = s; i <= e; ++i) {
            m_marks.insert(i);
        }
        m_cursor = index;
        m_anchor = from;
        notify_cursor();
        notify_selection();
        return true;
    }

    if (ctrl) {
        if (m_marks.empty() && m_cursor >= 0 && m_cursor != index) {
            m_marks.insert(m_cursor);
        }
        auto it = m_marks.find(index);
        if (it != m_marks.end()) {
            m_marks.erase(it);
        } else {
            m_marks.insert(index);
        }
        m_cursor = index;
        m_anchor = index;
        notify_cursor();
        notify_selection();
        return true;
    }

    // Normal click without modifiers: exclusive single selection
    select_single(index);
    return true;
}

bool SelectionModel::handle_context_click(int index) {
    if (m_mode == SelectionMode::None || index < 0) return false;
    if (m_mode == SelectionMode::Multiple && !m_marks.empty()) {
        if (m_marks.find(index) != m_marks.end()) {
            // Right-click inside existing multi-selection: preserve marks, update cursor
            m_cursor = index;
            notify_cursor();
            return true;
        }
    }
    select_single(index);
    return true;
}

bool SelectionModel::handle_space(size_t total_count, bool step) {
    if (m_mode != SelectionMode::Multiple) return false;
    if (m_cursor < 0 || m_cursor >= static_cast<int>(total_count)) return false;

    auto it = m_marks.find(m_cursor);
    if (it != m_marks.end()) {
        m_marks.erase(it);
    } else {
        m_marks.insert(m_cursor);
    }

    if (step && m_cursor < static_cast<int>(total_count) - 1) {
        m_cursor++;
    }
    m_anchor = m_cursor;
    notify_cursor();
    notify_selection();
    return true;
}

bool SelectionModel::handle_nav(int target_cursor, bool shift) {
    if (m_mode == SelectionMode::None) return false;
    int old_cursor = m_cursor;
    m_cursor = target_cursor;

    if (m_mode == SelectionMode::Multiple) {
        if (shift) {
            int from = (m_anchor >= 0 ? m_anchor : (old_cursor >= 0 ? old_cursor : target_cursor));
            int s = std::min(from, target_cursor);
            int e = std::max(from, target_cursor);
            m_marks.clear();
            for (int i = s; i <= e; ++i) {
                m_marks.insert(i);
            }
            m_anchor = from;
            notify_cursor();
            notify_selection();
            return true;
        } else {
            m_anchor = target_cursor;
            notify_cursor();
            if (m_marks.empty()) {
                notify_selection();
            }
            return true;
        }
    } else {
        m_anchor = target_cursor;
        notify_cursor();
        notify_selection();
        return true;
    }
}

bool SelectionModel::handle_escape() {
    if (m_mode == SelectionMode::Multiple && !m_marks.empty()) {
        clear_marks();
        return true;
    }
    return false;
}

bool SelectionModel::handle_select_all(size_t total_count) {
    if (m_mode != SelectionMode::Multiple || total_count == 0) return false;
    select_all(total_count);
    return true;
}

size_t SelectionModel::add_selection_changed_listener(SelectionChangedCallback cb) {
    size_t id = m_next_listener_id++;
    m_selection_listeners.push_back({id, std::move(cb)});
    return id;
}

void SelectionModel::remove_selection_changed_listener(size_t id) {
    m_selection_listeners.erase(
        std::remove_if(m_selection_listeners.begin(), m_selection_listeners.end(),
                       [id](const SelectionListenerEntry& e) { return e.id == id; }),
        m_selection_listeners.end());
}

size_t SelectionModel::add_cursor_changed_listener(CursorChangedCallback cb) {
    size_t id = m_next_listener_id++;
    m_cursor_listeners.push_back({id, std::move(cb)});
    return id;
}

void SelectionModel::remove_cursor_changed_listener(size_t id) {
    m_cursor_listeners.erase(
        std::remove_if(m_cursor_listeners.begin(), m_cursor_listeners.end(),
                       [id](const CursorListenerEntry& e) { return e.id == id; }),
        m_cursor_listeners.end());
}

void SelectionModel::set_on_selection_changed(SelectionChangedCallback cb) {
    m_selection_listeners.clear();
    if (cb) {
        m_selection_listeners.push_back({m_next_listener_id++, std::move(cb)});
    }
}

void SelectionModel::set_on_cursor_changed(CursorChangedCallback cb) {
    m_cursor_listeners.clear();
    if (cb) {
        m_cursor_listeners.push_back({m_next_listener_id++, std::move(cb)});
    }
}

void SelectionModel::notify_selection() {
    auto listeners = m_selection_listeners;
    for (const auto& entry : listeners) {
        if (entry.cb) {
            entry.cb(m_marks);
        }
    }
}

void SelectionModel::notify_cursor() {
    auto listeners = m_cursor_listeners;
    for (const auto& entry : listeners) {
        if (entry.cb) {
            entry.cb(m_cursor);
        }
    }
}

} // namespace miqu
