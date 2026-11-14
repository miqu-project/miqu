#include "music_miller_view.hpp"
#include <algorithm>
#include <set>
#include <iostream>

using namespace miqu;

constexpr int MATCH_PARENT = static_cast<int>(LayoutDimension::MatchParent);
constexpr int WRAP_CONTENT = static_cast<int>(LayoutDimension::WrapContent);

namespace miqumusic {

MusicMillerView::MusicMillerView(std::shared_ptr<MpdClient> client)
    : LinearLayout(Orientation::Horizontal), m_client(std::move(client)) {
}

void MusicMillerView::init() {
    setup_columns();
    refresh_library();
    rebuild_category_list();
    rebuild_middle_column();
    update_right_pillar();
}

void MusicMillerView::setup_columns() {
    auto config = Config::get();
    clear_views();
    set_divider_spacing(0);

    // Pillar 1: Left Pillar (Categories / Sources)
    m_left_pillar = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_left_pillar->set_padding(8, 8, 8, 8);
    m_left_pillar->set_background_color(Color::transparent());

    m_category_list = ListViewBuilder::create()
        ->itemHeight(42)
        ->spacing(4)
        ->itemCornerRadius(8)
        ->scrollbar(false)
        ->build();
    m_category_list->set_selectable(true);
    m_category_list->set_keyboard_navigation_enabled(false);
    m_category_list->set_on_item_click_listener([this](size_t idx, std::shared_ptr<View>) {
        MusicCategory cats[] = {
            MusicCategory::Queue,
            MusicCategory::Artists,
            MusicCategory::Albums,
            MusicCategory::Playlists,
            MusicCategory::Database,
            MusicCategory::Visualizer
        };
        if (idx < sizeof(cats) / sizeof(cats[0])) {
            m_active_category = cats[idx];
            m_path.clear();
            m_history_indices.clear();
            m_selected_artist.clear();
            m_selected_album.clear();
            m_selected_playlist.clear();
            m_middle_selected_index = 0;
            rebuild_category_list();
            rebuild_middle_column();
            update_right_pillar();
            if (m_on_state_changed) m_on_state_changed();
        }
    });

    LayoutParams cat_lp(MATCH_PARENT, MATCH_PARENT);
    cat_lp.weight = 1.0f;
    m_left_pillar->add_view(m_category_list, cat_lp);

    LayoutParams left_col_lp(210, MATCH_PARENT);
    add_view(m_left_pillar, left_col_lp);

    // Pillar 2: Middle Pillar (Contextual Browser)
    m_middle_pillar = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_middle_pillar->set_padding(8, 8, 8, 8);
    m_middle_pillar->set_background_color(Color::transparent());

    m_middle_list = ListViewBuilder::create()
        ->itemHeight(44)
        ->spacing(4)
        ->itemCornerRadius(8)
        ->scrollbar(false)
        ->build();
    m_middle_list->set_selectable(true);
    m_middle_list->set_keyboard_navigation_enabled(false);
    m_middle_list->set_on_item_click_listener([this](size_t idx, std::shared_ptr<View>) {
        m_middle_selected_index = static_cast<int>(idx);
        navigate_enter();
    });

    LayoutParams mid_lp(MATCH_PARENT, MATCH_PARENT);
    mid_lp.weight = 1.0f;
    m_middle_pillar->add_view(m_middle_list, mid_lp);

    LayoutParams mid_col_lp(MATCH_PARENT, MATCH_PARENT);
    mid_col_lp.weight = 1.0f;
    add_view(m_middle_pillar, mid_col_lp);

    // Pillar 3: Right Pillar (Hero Player Card / Visualizer)
    m_right_pillar = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_right_pillar->set_padding(6, 6, 6, 6);
    m_right_pillar->set_background_color(Color::transparent());

    m_player_card = std::make_shared<PlayerCardView>(m_client);
    LayoutParams player_lp(MATCH_PARENT, MATCH_PARENT);
    player_lp.weight = 1.0f;
    m_right_pillar->add_view(m_player_card, player_lp);

    m_full_visualizer = std::make_shared<VisualizerView>();
    m_full_visualizer->set_visibility(Visibility::Gone);
    LayoutParams vis_lp(MATCH_PARENT, MATCH_PARENT);
    vis_lp.weight = 1.0f;
    m_right_pillar->add_view(m_full_visualizer, vis_lp);

    LayoutParams right_col_lp(370, MATCH_PARENT);
    add_view(m_right_pillar, right_col_lp);
}

void MusicMillerView::refresh_library() {
    if (!m_client) return;

    m_cached_all_songs = m_client->get_all_songs();
    m_cached_queue = m_client->get_queue();
    m_cached_playlists = m_client->get_playlists();

    // Cache unique artists
    std::set<std::string> artists_set;
    std::set<std::string> albums_set;
    for (const auto& song : m_cached_all_songs) {
        if (!song.artist.empty()) artists_set.insert(song.artist);
        if (!song.album.empty()) albums_set.insert(song.album);
    }
    m_cached_artists.assign(artists_set.begin(), artists_set.end());
    m_cached_albums.assign(albums_set.begin(), albums_set.end());
}

void MusicMillerView::rebuild_category_list() {
    if (!m_category_list) return;
    m_category_list->clear_items();

    struct CatDef {
        MusicCategory cat;
        std::string title;
        std::string icon;
    } cats[] = {
        {MusicCategory::Queue, "Queue", "󰐊"},
        {MusicCategory::Artists, "Artists", "󰠃"},
        {MusicCategory::Albums, "Albums", "󰀥"},
        {MusicCategory::Playlists, "Playlists", "󰲸"},
        {MusicCategory::Database, "Library", "󰋊"},
        {MusicCategory::Visualizer, "Visualizer", "󰝚"}
    };

    size_t active_idx = 0;
    for (size_t i = 0; i < sizeof(cats) / sizeof(cats[0]); ++i) {
        bool selected = (m_active_category == cats[i].cat);
        if (selected) active_idx = i;
        m_category_list->add_item(create_category_item(cats[i].cat, cats[i].title, cats[i].icon, selected));
    }
    m_category_list->set_selected_index(static_cast<int>(active_idx));
    m_category_list->request_redraw();
}

std::shared_ptr<View> MusicMillerView::create_category_item(MusicCategory cat, const std::string& title, const std::string& icon, bool selected) {
    auto config = Config::get();
    auto item = std::make_shared<LinearLayout>(Orientation::Horizontal);
    item->set_gravity(Gravity::CenterVertical);
    item->set_padding(12, 6, 12, 6);
    item->set_divider_spacing(10);
    item->set_corner_radius(8);

    if (selected) {
        item->set_background_color(config->colors.primary.with_alpha(0.18f));
    } else {
        item->set_background_color(Color::transparent());
    }

    auto icon_view = TextViewBuilder::create()
        ->text(icon)
        ->textSize(config->metrics.font_size + 2)
        ->textColor(selected ? config->colors.primary : config->colors.on_surface_variant)
        ->build();
    item->add_view(icon_view, LayoutParams(24, WRAP_CONTENT, Gravity::CenterVertical));

    auto title_view = TextViewBuilder::create()
        ->text(title)
        ->textSize(config->metrics.font_size)
        ->bold(selected)
        ->textColor(selected ? config->colors.on_surface : config->colors.on_surface_variant)
        ->build();
    item->add_view(title_view, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));

    return item;
}

std::shared_ptr<View> MusicMillerView::create_song_list_item(size_t index, const Song& song, bool is_current) {
    auto config = Config::get();
    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_gravity(Gravity::CenterVertical);
    row->set_padding(10, 4, 10, 4);
    row->set_divider_spacing(8);
    row->set_corner_radius(8);

    if (is_current) {
        row->set_background_color(config->colors.primary.with_alpha(0.20f));
    } else {
        row->set_background_color(Color::transparent());
    }

    // Track position / number
    std::string num_str = std::to_string(index + 1) + ".";
    if (index + 1 < 10) num_str = "0" + num_str;
    auto num_view = TextViewBuilder::create()
        ->text(num_str)
        ->textSize(config->metrics.font_size - 1)
        ->textColor(is_current ? config->colors.primary : config->colors.on_surface_variant)
        ->build();
    row->add_view(num_view, LayoutParams(30, WRAP_CONTENT, Gravity::CenterVertical));

    // Song Title & Artist
    auto info_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    info_col->set_gravity(Gravity::CenterVertical);

    auto title_view = TextViewBuilder::create()
        ->text(song.display_title())
        ->textSize(config->metrics.font_size)
        ->bold(is_current)
        ->textColor(is_current ? config->colors.primary : config->colors.on_surface)
        ->ellipsize(true)
        ->build();
    info_col->add_view(title_view, LayoutParams(MATCH_PARENT, WRAP_CONTENT));

    if (!song.artist.empty()) {
        auto artist_view = TextViewBuilder::create()
            ->text(song.display_artist())
            ->textSize(config->metrics.font_size - 2)
            ->textColor(config->colors.on_surface_variant)
            ->ellipsize(true)
            ->build();
        info_col->add_view(artist_view, LayoutParams(MATCH_PARENT, WRAP_CONTENT));
    }

    row->add_view(info_col, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));

    // Active wave indicator or duration
    if (is_current) {
        auto wave_view = TextViewBuilder::create()
            ->text("󰝚")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.primary)
            ->build();
        row->add_view(wave_view, LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity::CenterVertical));
    }

    auto dur_view = TextViewBuilder::create()
        ->text(song.formatted_duration())
        ->textSize(config->metrics.font_size - 1)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    row->add_view(dur_view, LayoutParams(46, WRAP_CONTENT, Gravity::CenterVertical));

    return row;
}

std::shared_ptr<View> MusicMillerView::create_text_list_item(const std::string& title, const std::string& subtitle, bool selected) {
    auto config = Config::get();
    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_gravity(Gravity::CenterVertical);
    row->set_padding(12, 6, 12, 6);
    row->set_divider_spacing(8);
    row->set_corner_radius(8);

    if (selected) {
        row->set_background_color(config->colors.primary.with_alpha(0.18f));
    } else {
        row->set_background_color(Color::transparent());
    }

    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    text_col->set_gravity(Gravity::CenterVertical);

    auto title_view = TextViewBuilder::create()
        ->text(title)
        ->textSize(config->metrics.font_size)
        ->bold(selected)
        ->textColor(selected ? config->colors.primary : config->colors.on_surface)
        ->ellipsize(true)
        ->build();
    text_col->add_view(title_view, LayoutParams(MATCH_PARENT, WRAP_CONTENT));

    if (!subtitle.empty()) {
        auto sub_view = TextViewBuilder::create()
            ->text(subtitle)
            ->textSize(config->metrics.font_size - 2)
            ->textColor(config->colors.on_surface_variant)
            ->ellipsize(true)
            ->build();
        text_col->add_view(sub_view, LayoutParams(MATCH_PARENT, WRAP_CONTENT));
    }

    row->add_view(text_col, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));

    auto chevron = TextViewBuilder::create()
        ->text("›")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.5f))
        ->build();
    row->add_view(chevron, LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity::CenterVertical));

    return row;
}

void MusicMillerView::rebuild_middle_column() {
    if (!m_middle_list) return;
    m_middle_list->clear_items();
    m_current_display_songs.clear();

    // 1. Search Mode
    if (is_searching()) {
        std::string q = m_search_query;
        std::transform(q.begin(), q.end(), q.begin(), ::tolower);

        for (const auto& song : m_cached_all_songs) {
            std::string t = song.title;
            std::string a = song.artist;
            std::string alb = song.album;
            std::transform(t.begin(), t.end(), t.begin(), ::tolower);
            std::transform(a.begin(), a.end(), a.begin(), ::tolower);
            std::transform(alb.begin(), alb.end(), alb.begin(), ::tolower);

            if (t.find(q) != std::string::npos || a.find(q) != std::string::npos || alb.find(q) != std::string::npos) {
                m_current_display_songs.push_back(song);
            }
        }

        for (size_t i = 0; i < m_current_display_songs.size(); ++i) {
            bool is_cur = (m_last_song.uri == m_current_display_songs[i].uri && !m_last_song.uri.empty());
            m_middle_list->add_item(create_song_list_item(i, m_current_display_songs[i], is_cur));
        }
        m_middle_list->set_selected_index(m_middle_selected_index);
        m_middle_list->request_redraw();
        return;
    }

    // 2. Queue Category
    if (m_active_category == MusicCategory::Queue) {
        m_current_display_songs = m_cached_queue;
        for (size_t i = 0; i < m_cached_queue.size(); ++i) {
            bool is_cur = (m_last_status.song_pos >= 0 && static_cast<int>(i) == m_last_status.song_pos);
            m_middle_list->add_item(create_song_list_item(i, m_cached_queue[i], is_cur));
        }
        m_middle_list->set_selected_index(m_middle_selected_index);
        m_middle_list->request_redraw();
        return;
    }

    // 3. Artists Category
    if (m_active_category == MusicCategory::Artists) {
        if (m_selected_artist.empty()) {
            for (const auto& artist : m_cached_artists) {
                m_middle_list->add_item(create_text_list_item(artist, "Artist", false));
            }
        } else if (m_selected_album.empty()) {
            // List albums by this artist
            std::set<std::string> artist_albums;
            for (const auto& song : m_cached_all_songs) {
                if (song.artist == m_selected_artist && !song.album.empty()) {
                    artist_albums.insert(song.album);
                }
            }
            for (const auto& alb : artist_albums) {
                m_middle_list->add_item(create_text_list_item(alb, m_selected_artist, false));
            }
        } else {
            // List songs in this artist's album
            for (const auto& song : m_cached_all_songs) {
                if (song.artist == m_selected_artist && song.album == m_selected_album) {
                    m_current_display_songs.push_back(song);
                }
            }
            for (size_t i = 0; i < m_current_display_songs.size(); ++i) {
                bool is_cur = (m_last_song.uri == m_current_display_songs[i].uri && !m_last_song.uri.empty());
                m_middle_list->add_item(create_song_list_item(i, m_current_display_songs[i], is_cur));
            }
        }
        m_middle_list->set_selected_index(m_middle_selected_index);
        m_middle_list->request_redraw();
        return;
    }

    // 4. Albums Category
    if (m_active_category == MusicCategory::Albums) {
        if (m_selected_album.empty()) {
            for (const auto& alb : m_cached_albums) {
                m_middle_list->add_item(create_text_list_item(alb, "Album", false));
            }
        } else {
            for (const auto& song : m_cached_all_songs) {
                if (song.album == m_selected_album) {
                    m_current_display_songs.push_back(song);
                }
            }
            for (size_t i = 0; i < m_current_display_songs.size(); ++i) {
                bool is_cur = (m_last_song.uri == m_current_display_songs[i].uri && !m_last_song.uri.empty());
                m_middle_list->add_item(create_song_list_item(i, m_current_display_songs[i], is_cur));
            }
        }
        m_middle_list->set_selected_index(m_middle_selected_index);
        m_middle_list->request_redraw();
        return;
    }

    // 5. Playlists Category
    if (m_active_category == MusicCategory::Playlists) {
        if (m_selected_playlist.empty()) {
            for (const auto& pl : m_cached_playlists) {
                m_middle_list->add_item(create_text_list_item(pl, "Saved Playlist", false));
            }
        } else {
            m_current_display_songs = m_client ? m_client->get_playlist_songs(m_selected_playlist) : std::vector<Song>();
            for (size_t i = 0; i < m_current_display_songs.size(); ++i) {
                bool is_cur = (m_last_song.uri == m_current_display_songs[i].uri && !m_last_song.uri.empty());
                m_middle_list->add_item(create_song_list_item(i, m_current_display_songs[i], is_cur));
            }
        }
        m_middle_list->set_selected_index(m_middle_selected_index);
        m_middle_list->request_redraw();
        return;
    }

    // 6. Database / Library Category
    if (m_active_category == MusicCategory::Database) {
        m_current_display_songs = m_cached_all_songs;
        for (size_t i = 0; i < m_current_display_songs.size(); ++i) {
            bool is_cur = (m_last_song.uri == m_current_display_songs[i].uri && !m_last_song.uri.empty());
            m_middle_list->add_item(create_song_list_item(i, m_current_display_songs[i], is_cur));
        }
        m_middle_list->set_selected_index(m_middle_selected_index);
        m_middle_list->request_redraw();
        return;
    }

    // 7. Visualizer Presets
    if (m_active_category == MusicCategory::Visualizer) {
        m_middle_list->add_item(create_text_list_item("Neon Spectrum Bars", "Dynamic frequency bands with gradient glow", m_full_visualizer && m_full_visualizer->get_mode() == VisualizerMode::SpectrumBars));
        m_middle_list->add_item(create_text_list_item("Waveform Oscilloscope", "Fluid real-time audio scope curve", m_full_visualizer && m_full_visualizer->get_mode() == VisualizerMode::WaveformScope));
        m_middle_list->add_item(create_text_list_item("Glow Peaks Analyzer", "Frequency meters with floor ambient reflection", m_full_visualizer && m_full_visualizer->get_mode() == VisualizerMode::GlowPeaks));
        m_middle_list->set_selected_index(m_middle_selected_index);
        m_middle_list->request_redraw();
        return;
    }
}

void MusicMillerView::update_right_pillar() {
    if (m_active_category == MusicCategory::Visualizer) {
        if (m_player_card) m_player_card->set_visibility(Visibility::Gone);
        if (m_full_visualizer) {
            m_full_visualizer->set_visibility(Visibility::Visible);
            m_full_visualizer->set_playing(m_last_status.state == PlaybackState::Playing);
        }
    } else {
        if (m_full_visualizer) m_full_visualizer->set_visibility(Visibility::Gone);
        if (m_player_card) {
            m_player_card->set_visibility(Visibility::Visible);
            m_player_card->update_state(m_last_status, m_last_song);
        }
    }
}

void MusicMillerView::update_playback(const MpdStatus& status, const Song& song) {
    m_last_status = status;
    m_last_song = song;

    if (m_player_card) {
        m_player_card->update_state(status, song);
    }
    if (m_full_visualizer) {
        m_full_visualizer->set_playing(status.state == PlaybackState::Playing);
    }

    // Update active highlight in middle list
    if (m_active_category == MusicCategory::Queue || !m_current_display_songs.empty()) {
        rebuild_middle_column();
    }
}

void MusicMillerView::update_queue(const std::vector<Song>& queue) {
    m_cached_queue = queue;
    if (m_active_category == MusicCategory::Queue) {
        rebuild_middle_column();
    }
}

void MusicMillerView::tick(double delta_time) {
    if (m_player_card && m_player_card->is_visible()) {
        m_player_card->tick(delta_time);
    }
    if (m_full_visualizer && m_full_visualizer->is_visible()) {
        m_full_visualizer->tick(delta_time);
    }
}

bool MusicMillerView::navigate_up() {
    if (!m_middle_list || m_middle_list->get_item_count() == 0) return false;
    int cur = m_middle_list->get_selected_index();
    if (cur > 0) {
        m_middle_list->set_selected_index(cur - 1);
        m_middle_selected_index = cur - 1;
        m_middle_list->request_redraw();
        return true;
    }
    return false;
}

bool MusicMillerView::navigate_down() {
    if (!m_middle_list || m_middle_list->get_item_count() == 0) return false;
    int cur = m_middle_list->get_selected_index();
    if (cur + 1 < static_cast<int>(m_middle_list->get_item_count())) {
        m_middle_list->set_selected_index(cur + 1);
        m_middle_selected_index = cur + 1;
        m_middle_list->request_redraw();
        return true;
    }
    return false;
}

bool MusicMillerView::navigate_enter() {
    if (!m_middle_list || m_middle_list->get_item_count() == 0) return false;
    int idx = m_middle_list->get_selected_index();
    if (idx < 0) idx = 0;

    // Search Mode: play selected search result
    if (is_searching()) {
        if (idx < static_cast<int>(m_current_display_songs.size()) && m_client) {
            m_client->add_and_play(m_current_display_songs[idx].uri);
        }
        return true;
    }

    // Queue Mode: jump to track in queue
    if (m_active_category == MusicCategory::Queue) {
        if (idx < static_cast<int>(m_cached_queue.size()) && m_client) {
            m_client->play_pos(static_cast<unsigned int>(idx));
        }
        return true;
    }

    // Artists Mode
    if (m_active_category == MusicCategory::Artists) {
        if (m_selected_artist.empty()) {
            if (idx < static_cast<int>(m_cached_artists.size())) {
                m_selected_artist = m_cached_artists[idx];
                m_path.push_back({m_selected_artist, "artist"});
                m_history_indices.push_back(idx);
                m_middle_selected_index = 0;
                rebuild_middle_column();
                if (m_on_state_changed) m_on_state_changed();
                return true;
            }
        } else if (m_selected_album.empty()) {
            std::set<std::string> artist_albums;
            for (const auto& song : m_cached_all_songs) {
                if (song.artist == m_selected_artist && !song.album.empty()) {
                    artist_albums.insert(song.album);
                }
            }
            std::vector<std::string> alb_vec(artist_albums.begin(), artist_albums.end());
            if (idx < static_cast<int>(alb_vec.size())) {
                m_selected_album = alb_vec[idx];
                m_path.push_back({m_selected_album, "album"});
                m_history_indices.push_back(idx);
                m_middle_selected_index = 0;
                rebuild_middle_column();
                if (m_on_state_changed) m_on_state_changed();
                return true;
            }
        } else {
            // Play song
            if (idx < static_cast<int>(m_current_display_songs.size()) && m_client) {
                m_client->add_and_play(m_current_display_songs[idx].uri);
            }
            return true;
        }
    }

    // Albums Mode
    if (m_active_category == MusicCategory::Albums) {
        if (m_selected_album.empty()) {
            if (idx < static_cast<int>(m_cached_albums.size())) {
                m_selected_album = m_cached_albums[idx];
                m_path.push_back({m_selected_album, "album"});
                m_history_indices.push_back(idx);
                m_middle_selected_index = 0;
                rebuild_middle_column();
                if (m_on_state_changed) m_on_state_changed();
                return true;
            }
        } else {
            if (idx < static_cast<int>(m_current_display_songs.size()) && m_client) {
                m_client->add_and_play(m_current_display_songs[idx].uri);
            }
            return true;
        }
    }

    // Playlists Mode
    if (m_active_category == MusicCategory::Playlists) {
        if (m_selected_playlist.empty()) {
            if (idx < static_cast<int>(m_cached_playlists.size())) {
                m_selected_playlist = m_cached_playlists[idx];
                m_path.push_back({m_selected_playlist, "playlist"});
                m_history_indices.push_back(idx);
                m_middle_selected_index = 0;
                rebuild_middle_column();
                if (m_on_state_changed) m_on_state_changed();
                return true;
            }
        } else {
            if (idx < static_cast<int>(m_current_display_songs.size()) && m_client) {
                m_client->add_and_play(m_current_display_songs[idx].uri);
            }
            return true;
        }
    }

    // Database Mode
    if (m_active_category == MusicCategory::Database) {
        if (idx < static_cast<int>(m_current_display_songs.size()) && m_client) {
            m_client->add_and_play(m_current_display_songs[idx].uri);
        }
        return true;
    }

    // Visualizer Mode: switch preset
    if (m_active_category == MusicCategory::Visualizer && m_full_visualizer) {
        if (idx == 0) m_full_visualizer->set_mode(VisualizerMode::SpectrumBars);
        else if (idx == 1) m_full_visualizer->set_mode(VisualizerMode::WaveformScope);
        else if (idx == 2) m_full_visualizer->set_mode(VisualizerMode::GlowPeaks);
        rebuild_middle_column();
        return true;
    }

    return false;
}

bool MusicMillerView::navigate_back() {
    if (m_preview_expanded) {
        set_preview_expanded(false);
        return true;
    }

    if (is_searching()) {
        set_search_query("");
        return true;
    }

    if (!m_path.empty()) {
        m_path.pop_back();
        if (m_active_category == MusicCategory::Artists) {
            if (!m_selected_album.empty()) {
                m_selected_album.clear();
            } else if (!m_selected_artist.empty()) {
                m_selected_artist.clear();
            }
        } else if (m_active_category == MusicCategory::Albums) {
            m_selected_album.clear();
        } else if (m_active_category == MusicCategory::Playlists) {
            m_selected_playlist.clear();
        }

        if (!m_history_indices.empty()) {
            m_middle_selected_index = m_history_indices.back();
            m_history_indices.pop_back();
        } else {
            m_middle_selected_index = 0;
        }

        rebuild_middle_column();
        if (m_on_state_changed) m_on_state_changed();
        return true;
    }

    return false;
}

void MusicMillerView::navigate_to_depth(size_t depth) {
    if (m_preview_expanded) {
        set_preview_expanded(false);
    }
    if (is_searching()) {
        set_search_query("");
    }

    if (depth == 0) {
        m_path.clear();
        m_history_indices.clear();
        m_selected_artist.clear();
        m_selected_album.clear();
        m_selected_playlist.clear();
        m_middle_selected_index = 0;
        rebuild_middle_column();
        if (m_on_state_changed) m_on_state_changed();
        return;
    }

    while (m_path.size() > depth) {
        m_path.pop_back();
        if (!m_history_indices.empty()) {
            m_middle_selected_index = m_history_indices.back();
            m_history_indices.pop_back();
        }
    }

    if (m_active_category == MusicCategory::Artists) {
        if (depth == 1) {
            m_selected_album.clear();
        }
    }

    rebuild_middle_column();
    if (m_on_state_changed) m_on_state_changed();
}

void MusicMillerView::set_preview_expanded(bool expanded) {
    if (m_preview_expanded != expanded) {
        m_preview_expanded = expanded;
        if (m_left_pillar) m_left_pillar->set_visibility(expanded ? Visibility::Gone : Visibility::Visible);
        if (m_middle_pillar) m_middle_pillar->set_visibility(expanded ? Visibility::Gone : Visibility::Visible);
        request_redraw();
        if (m_on_state_changed) m_on_state_changed();
    }
}

bool MusicMillerView::can_navigate_back() const {
    return is_searching() || m_preview_expanded || !m_path.empty();
}

std::string MusicMillerView::get_current_leaf_title() const {
    if (is_searching()) {
        return "Search Results (" + std::to_string(m_current_display_songs.size()) + ")";
    }
    if (!m_path.empty()) {
        return m_path.back().title;
    }
    switch (m_active_category) {
        case MusicCategory::Queue: return "Queue";
        case MusicCategory::Artists: return "Artists";
        case MusicCategory::Albums: return "Albums";
        case MusicCategory::Playlists: return "Playlists";
        case MusicCategory::Database: return "Library";
        case MusicCategory::Visualizer: return "Visualizer";
    }
    return "Music";
}

void MusicMillerView::set_search_query(const std::string& query) {
    if (m_search_query != query) {
        m_search_query = query;
        m_middle_selected_index = 0;
        rebuild_middle_column();
        if (m_on_state_changed) m_on_state_changed();
    }
}

void MusicMillerView::sync_theme() {
    rebuild_category_list();
    rebuild_middle_column();
    if (m_player_card) m_player_card->sync_theme();
    request_redraw();
}

} // namespace miqumusic
