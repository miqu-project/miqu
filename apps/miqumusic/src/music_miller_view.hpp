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
    void rebuild_category_list();
    void rebuild_middle_column();
    void update_right_pillar();

    std::shared_ptr<View> create_category_item(MusicCategory cat, const std::string& title, const std::string& icon, bool selected);
    std::shared_ptr<View> create_song_list_item(size_t index, const Song& song, bool is_current);
    std::shared_ptr<View> create_text_list_item(const std::string& title, const std::string& subtitle, bool selected);

    std::shared_ptr<MpdClient> m_client;

    // Pillars
    std::shared_ptr<miqu::LinearLayout> m_left_pillar;
    std::shared_ptr<miqu::LinearLayout> m_middle_pillar;
    std::shared_ptr<miqu::LinearLayout> m_right_pillar;

    std::shared_ptr<miqu::ListView> m_category_list;
    std::shared_ptr<miqu::ListView> m_middle_list;

    std::shared_ptr<PlayerCardView> m_player_card;
    std::shared_ptr<VisualizerView> m_full_visualizer;

    // Navigation State
    MusicCategory m_active_category = MusicCategory::Queue;
    std::vector<MusicNavLevel> m_path;
    std::vector<int> m_history_indices;
    int m_middle_selected_index = 0;

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
    MpdStatus m_last_status;
    Song m_last_song;

    std::function<void()> m_on_state_changed;
};

} // namespace miqumusic
