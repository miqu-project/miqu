#pragma once

#include "mpd_client.hpp"
#include "mpd_types.hpp"
#include "player_card_view.hpp"
#include "visualizer_view.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace miqumusic {

enum class MusicCategory {
    Queue,
    Artists,
    Albums,
    Playlists,
    Database,
    Visualizer
};

struct MusicNavLevel {
    std::string title;
    std::string id;
};

class MusicMillerView : public miqu::LinearLayout {
public:
    explicit MusicMillerView(std::shared_ptr<MpdClient> client);
    ~MusicMillerView() override = default;

    void init();
    void sync_theme();
    void tick(double delta_time);

    bool on_mouse_move(int lx, int ly, const miqu::Rect& bounds) override;
    bool on_scroll(double delta) override;

    void update_playback(const MpdStatus& status, const Song& song);
    void update_queue(const std::vector<Song>& queue);
    void refresh_library();

    // Navigation
    bool navigate_up();
    bool navigate_down();
    bool navigate_enter();
    bool navigate_back();
    void navigate_to_depth(size_t depth);

    // Expanded / Immersion mode (F11)
    bool is_preview_expanded() const { return m_preview_expanded; }
    void set_preview_expanded(bool expanded);

    // Breadcrumbs & State
    bool can_navigate_back() const;
    const std::vector<MusicNavLevel>& get_path() const { return m_path; }
    std::string get_current_leaf_title() const;
    bool is_searching() const { return !m_search_query.empty(); }
    const std::string& get_search_query() const { return m_search_query; }
    void set_search_query(const std::string& query);

    void set_on_state_changed(std::function<void()> cb) { m_on_state_changed = std::move(cb); }

    std::shared_ptr<PlayerCardView> get_player_card() const { return m_player_card; }

private:
    void setup_columns();
    void rebuild_columns();
    void update_pillar_styles();
    void rebuild_parent_column();
    void rebuild_current_column();
    void update_right_pillar();
    void handle_parent_item_click(size_t idx);

    std::shared_ptr<miqu::View> create_parent_hero_box(const std::shared_ptr<miqu::Config>& config);
    std::shared_ptr<miqu::View> create_row_item(const std::string& title, const std::string& icon = "", bool has_children = true);
    std::shared_ptr<miqu::View> create_song_row_item(size_t index, const Song& song, bool is_current);

    std::shared_ptr<MpdClient> m_client;

    // Pillars
    std::shared_ptr<miqu::LinearLayout> m_parent_pillar;
    std::shared_ptr<miqu::LinearLayout> m_current_pillar;
    std::shared_ptr<miqu::LinearLayout> m_right_pillar;

    std::shared_ptr<miqu::View> m_parent_hero;
    std::shared_ptr<miqu::ListView> m_parent_list;
    std::shared_ptr<miqu::ListView> m_current_list;

    std::shared_ptr<PlayerCardView> m_player_card;
    std::shared_ptr<VisualizerView> m_full_visualizer;

    // Navigation State
    MusicCategory m_active_category = MusicCategory::Queue;
    std::vector<MusicNavLevel> m_path;
    std::vector<int> m_history_indices;
    int m_current_index = 0;

    std::string m_selected_artist;
    std::string m_selected_album;
    std::string m_selected_playlist;

    // Data Caches
    std::vector<Song> m_cached_queue;
    std::vector<Song> m_cached_all_songs;
    std::vector<std::string> m_cached_artists;
    std::vector<std::string> m_cached_albums;
    std::vector<std::string> m_cached_playlists;
    std::vector<Song> m_current_display_songs;

    std::string m_search_query;
    std::vector<Song> m_search_results;

    bool m_preview_expanded = false;
    int m_last_mouse_x = -1;
    int m_last_mouse_y = -1;

    MpdStatus m_last_status;
    Song m_last_song;

    std::function<void()> m_on_state_changed;
};

} // namespace miqumusic
