#pragma once

#include "music_model.hpp"
#include "mpd_client.hpp"
#include "visualizer_view.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <vector>
#include <memory>
#include <functional>
#include <string>

namespace miqu {

class MillerView : public LinearLayout {
public:
    MillerView();
    ~MillerView() override = default;

    void init();
    void sync_theme();

    bool on_key(const KeyPressEvent& event) override;
    bool on_mouse_move(int lx, int ly, const Rect& bounds) override;
    bool on_mouse_button(int lx, int ly, MouseButton button, bool pressed, const Rect& bounds) override;
    bool on_scroll(double delta) override;

    void set_preview_focused(bool focused);
    void set_preview_expanded(bool expanded);
    bool is_preview_expanded() const { return m_preview_expanded; }

    // Navigation Controls
    bool navigate_up();
    bool navigate_down();
    bool navigate_enter();
    bool navigate_back();
    bool navigate_right();
    bool navigate_left();
    void navigate_to_depth(size_t depth);
    bool focus_first_preview_control();
    bool focus_next_preview_control();
    bool focus_prev_preview_control();
    bool trigger_active_action();

    // State Inspection
    const std::vector<std::shared_ptr<MusicItem>>& get_path() const { return m_path; }
    bool can_navigate_back() const { return !m_search_query.empty() || m_preview_expanded || !m_path.empty(); }
    void set_search_query(const std::string& query);
    const std::string& get_search_query() const { return m_search_query; }
    bool is_searching() const { return !m_search_query.empty(); }
    std::string get_breadcrumb_path() const;
    bool is_preview_focused() const { return m_preview_focused; }
    std::shared_ptr<MusicItem> get_current_selected_item() const;

    bool check_and_update_click_debounce();

    void set_on_state_changed(std::function<void()> cb) {
        m_on_state_changed = std::move(cb);
    }

    void set_client(std::shared_ptr<miqumusic::MpdClient> client);
    std::shared_ptr<miqumusic::MpdClient> get_client() const { return m_client; }
    void refresh_model();

private:
    void rebuild_columns();
    void update_pillar_styles();
    void update_preview_ribbon();
    void rebuild_parent_column();
    void rebuild_current_column();
    void rebuild_preview_column();
    void deferred_rebuild_preview(int delay_ms = 15);
    void add_preview_card(std::shared_ptr<View> card);

    // Column Element Builders
    std::shared_ptr<View> create_parent_hero_box(const std::shared_ptr<Config>& config);
    std::shared_ptr<View> create_empty_search_box(const std::shared_ptr<Config>& config);
    std::shared_ptr<View> create_list_row_item(const std::shared_ptr<MusicItem>& item, const std::shared_ptr<Config>& config);

    // Preview Renderers (preview_music.cpp)
    void render_empty_preview(const std::shared_ptr<Config>& config);
    void render_category_children_preview(const std::shared_ptr<MusicItem>& item, const std::shared_ptr<Config>& config);
    void render_song_preview(const std::shared_ptr<MusicItem>& item, const std::shared_ptr<Config>& config);
    void render_visualizer_preview(const std::shared_ptr<MusicItem>& item, const std::shared_ptr<Config>& config);

    const std::vector<std::shared_ptr<MusicItem>>& get_current_list() const;
    void notify_state_changed();

    void handle_parent_item_click(size_t idx);
    void execute_sibling_swap(const std::shared_ptr<MusicItem>& item, size_t index);
    void execute_shift_up(size_t index);

    uint64_t m_last_click_time_ms = 0;
    uint64_t m_last_navigate_enter_time_ms = 0;
    uint64_t m_last_navigate_back_time_ms = 0;

    std::shared_ptr<miqumusic::MpdClient> m_client;
    std::shared_ptr<miqumusic::VisualizerView> m_visualizer_view;

    std::vector<std::shared_ptr<MusicItem>> m_root_tree;

    // Navigation stack
    std::vector<std::shared_ptr<MusicItem>> m_path;
    std::vector<int> m_history_indices;
    int m_current_index = 0;

    // Focus state
    bool m_preview_focused = false;
    bool m_preview_expanded = false;
    int m_last_mouse_x = -1;
    int m_last_mouse_y = -1;

    std::function<void()> m_on_state_changed;

    // UI containers
    std::shared_ptr<LinearLayout> m_parent_pillar;
    std::shared_ptr<ListView> m_parent_list;
    std::shared_ptr<View> m_parent_hero;

    std::shared_ptr<LinearLayout> m_current_pillar;
    std::shared_ptr<ListView> m_current_list;
    std::shared_ptr<View> m_empty_search_box;

    std::shared_ptr<LinearLayout> m_preview_pillar;
    std::shared_ptr<LinearLayout> m_preview_ribbon;
    std::shared_ptr<CardView> m_preview_toggle_btn;
    std::shared_ptr<TextView> m_preview_toggle_icon;
    std::shared_ptr<TextView> m_preview_title;

    std::shared_ptr<ListView> m_category_list;
    std::shared_ptr<ScrollView> m_preview_scroll;
    std::shared_ptr<LinearLayout> m_preview_layout;

    std::string m_search_query;
    std::vector<std::shared_ptr<MusicItem>> m_search_results;
};

} // namespace miqu
