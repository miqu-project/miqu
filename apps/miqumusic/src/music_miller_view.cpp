#include "music_miller_view.hpp"
#include <algorithm>
#include <set>
#include <iostream>
#include <filesystem>

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
    rebuild_columns();
}

void MusicMillerView::setup_columns() {
    auto config = Config::get();
    clear_views();
    set_divider_spacing(0);
    set_padding(8, 4, 8, 8);

    // 1. Column 1: Parent Pillar (width 260)
    m_parent_pillar = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(4)
        ->padding(8, 8)
        ->backgroundColor(Color::transparent())
        ->build();

    m_parent_hero = create_parent_hero_box(config);
    m_parent_pillar->add_view(m_parent_hero, LayoutParams(MATCH_PARENT, MATCH_PARENT, 1.0f));

    m_parent_list = ListViewBuilder::create()
        ->itemHeight(42)
        ->spacing(4)
        ->itemCornerRadius(8)
        ->scrollbar(false)
        ->build();
    m_parent_list->set_selectable(true);
    m_parent_list->set_keyboard_navigation_enabled(false);
    m_parent_list->set_on_item_click_listener([this](size_t idx, std::shared_ptr<View>) {
        handle_parent_item_click(idx);
    });
    m_parent_list->set_visibility(Visibility::Gone);
    m_parent_pillar->add_view(m_parent_list, LayoutParams(MATCH_PARENT, MATCH_PARENT, 1.0f));

    LayoutParams left_col_lp(260, MATCH_PARENT);
    add_view(m_parent_pillar, left_col_lp);

    // 2. Column 2: Current / Middle Pillar (width 320, flex)
    m_current_pillar = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(4)
        ->padding(8, 8)
        ->backgroundColor(Color::transparent())
        ->build();

    m_current_list = ListViewBuilder::create()
        ->itemHeight(44)
        ->spacing(4)
        ->itemCornerRadius(8)
        ->scrollbar(false)
        ->build();
    m_current_list->set_selectable(true);
    m_current_list->set_keyboard_navigation_enabled(false);
    m_current_list->set_on_item_click_listener([this](size_t idx, std::shared_ptr<View>) {
        m_current_index = static_cast<int>(idx);
        navigate_enter();
    });

    LayoutParams mid_lp(MATCH_PARENT, MATCH_PARENT);
    mid_lp.weight = 1.0f;
    m_current_pillar->add_view(m_current_list, mid_lp);

    LayoutParams mid_col_lp(MATCH_PARENT, MATCH_PARENT);
    mid_col_lp.weight = 1.0f;
    add_view(m_current_pillar, mid_col_lp);

    // 3. Column 3: Right Pillar (Hero Player Card / Visualizer)
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

std::shared_ptr<View> MusicMillerView::create_parent_hero_box(const std::shared_ptr<Config>& config) {
    auto hero_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->gravity(Gravity::Center)
        ->padding(16, 24)
        ->spacing(12)
        ->build();

    // Check for miqumusic icon assets
    std::string icon_candidates[] = {
        "/home/anisur/miqu-project/miqu/apps/miqumusic/assets/miqumusic-64.png",
        "assets/miqumusic-64.png",
        "/usr/share/icons/hicolor/64x64/apps/miqumusic.png",
        "/usr/share/pixmaps/miqumusic.png"
    };

    std::string found_icon;
    for (const auto& path : icon_candidates) {
        if (std::filesystem::exists(path)) {
            found_icon = path;
            break;
        }
    }

    if (!found_icon.empty()) {
        auto icon_view = ImageViewBuilder::create()
            ->imageResource(found_icon)
            ->fitMode(FitMode::Contain)
            ->build();
        icon_view->set_layout_params(LayoutParams(64, 64, Gravity::CenterHorizontal));
        hero_box->add_view(icon_view);
    } else {
        auto icon_glyph = TextViewBuilder::create()
            ->text("󰎆")
            ->textSize(static_cast<int>(config->metrics.font_size * 2.8f))
            ->textColor(config->colors.primary.with_alpha(0.75f))
            ->textAlignment(TextAlignment::Center)
            ->build();
        hero_box->add_view(icon_glyph);
    }

    auto hero_title = TextViewBuilder::create()
        ->text("Music")
        ->textSize(static_cast<int>(config->metrics.font_size * 1.35f))
        ->bold(true)
        ->textColor(config->colors.on_surface)
        ->textAlignment(TextAlignment::Center)
        ->build();
    hero_box->add_view(hero_title);

    auto hero_hint = TextViewBuilder::create()
        ->text("↑/↓ Navigate\nEnter Select\nEsc Back\nSpace Play/Pause")
        ->textSize(config->metrics.font_size - 2)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.6f))
        ->textAlignment(TextAlignment::Center)
        ->build();
    hero_box->add_view(hero_hint);

    return hero_box;
}

void MusicMillerView::refresh_library() {
    if (!m_client) return;

    m_cached_all_songs = m_client->get_all_songs();
    m_cached_queue = m_client->get_queue();
    m_cached_playlists = m_client->get_playlists();

    std::set<std::string> artists_set;
    std::set<std::string> albums_set;
    for (const auto& song : m_cached_all_songs) {
        if (!song.artist.empty()) artists_set.insert(song.artist);
        if (!song.album.empty()) albums_set.insert(song.album);
    }
    m_cached_artists.assign(artists_set.begin(), artists_set.end());
    m_cached_albums.assign(albums_set.begin(), albums_set.end());
}

void MusicMillerView::rebuild_columns() {
    rebuild_parent_column();
    rebuild_current_column();
    update_right_pillar();
    request_redraw();
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

    std::string num_str = std::to_string(index + 1) + ".";
    if (index + 1 < 10) num_str = "0" + num_str;
    auto num_view = TextViewBuilder::create()
        ->text(num_str)
        ->textSize(config->metrics.font_size - 1)
        ->textColor(is_current ? config->colors.primary : config->colors.on_surface_variant)
        ->build();
    row->add_view(num_view, LayoutParams(30, WRAP_CONTENT, Gravity::CenterVertical));

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

void MusicMillerView::rebuild_parent_column() {
    // 1. Root Depth 0: Show empty parent hero box with app icon, name, and shortcuts!
    if (m_path.empty() && m_search_query.empty()) {
        if (m_parent_hero) m_parent_hero->set_visibility(Visibility::Visible);
        if (m_parent_list) m_parent_list->set_visibility(Visibility::Gone);
        return;
    }

    // 2. Depth >= 1: Hide hero box, display parent siblings list in Column 1
    if (m_parent_hero) m_parent_hero->set_visibility(Visibility::Gone);
    if (!m_parent_list) return;

    m_parent_list->set_visibility(Visibility::Visible);
    m_parent_list->clear_items();

    struct CatDef {
        MusicCategory cat;
        std::string title;
        std::string icon;
    } root_cats[] = {
        {MusicCategory::Queue, "Queue", "󰐊"},
        {MusicCategory::Artists, "Artists", "󰠃"},
        {MusicCategory::Albums, "Albums", "󰀥"},
        {MusicCategory::Playlists, "Playlists", "󰲸"},
        {MusicCategory::Database, "Library", "󰋊"},
        {MusicCategory::Visualizer, "Visualizer", "󰝚"}
    };

    if (m_path.size() == 1 || is_searching()) {
        // Show root categories, highlight active category
        int active_idx = -1;
        for (size_t i = 0; i < sizeof(root_cats) / sizeof(root_cats[0]); ++i) {
            bool selected = (!is_searching() && m_active_category == root_cats[i].cat);
            if (selected) active_idx = static_cast<int>(i);
            m_parent_list->add_item(create_category_item(root_cats[i].cat, root_cats[i].title, root_cats[i].icon, selected));
        }
        m_parent_list->set_selected_index(active_idx);
        m_parent_list->request_redraw();
        return;
    }

    if (m_path.size() == 2) {
        // Parent is Depth 1 (e.g. all Artists or all Albums or Playlists)
        if (m_active_category == MusicCategory::Artists) {
            int active_idx = -1;
            for (size_t i = 0; i < m_cached_artists.size(); ++i) {
                bool selected = (m_cached_artists[i] == m_selected_artist);
                if (selected) active_idx = static_cast<int>(i);
                m_parent_list->add_item(create_text_list_item(m_cached_artists[i], "Artist", selected));
            }
            m_parent_list->set_selected_index(active_idx);
            m_parent_list->request_redraw();
            return;
        }
        if (m_active_category == MusicCategory::Albums) {
            int active_idx = -1;
            for (size_t i = 0; i < m_cached_albums.size(); ++i) {
                bool selected = (m_cached_albums[i] == m_selected_album);
                if (selected) active_idx = static_cast<int>(i);
                m_parent_list->add_item(create_text_list_item(m_cached_albums[i], "Album", selected));
            }
            m_parent_list->set_selected_index(active_idx);
            m_parent_list->request_redraw();
            return;
        }
        if (m_active_category == MusicCategory::Playlists) {
            int active_idx = -1;
            for (size_t i = 0; i < m_cached_playlists.size(); ++i) {
                bool selected = (m_cached_playlists[i] == m_selected_playlist);
                if (selected) active_idx = static_cast<int>(i);
                m_parent_list->add_item(create_text_list_item(m_cached_playlists[i], "Playlist", selected));
            }
            m_parent_list->set_selected_index(active_idx);
            m_parent_list->request_redraw();
            return;
        }
    }

    if (m_path.size() == 3) {
        // Parent is Depth 2 (e.g. Artist's Albums)
        if (m_active_category == MusicCategory::Artists) {
            std::set<std::string> artist_albums;
            for (const auto& song : m_cached_all_songs) {
                if (song.artist == m_selected_artist && !song.album.empty()) {
                    artist_albums.insert(song.album);
                }
            }
            std::vector<std::string> alb_vec(artist_albums.begin(), artist_albums.end());
            int active_idx = -1;
            for (size_t i = 0; i < alb_vec.size(); ++i) {
                bool selected = (alb_vec[i] == m_selected_album);
                if (selected) active_idx = static_cast<int>(i);
                m_parent_list->add_item(create_text_list_item(alb_vec[i], m_selected_artist, selected));
            }
            m_parent_list->set_selected_index(active_idx);
            m_parent_list->request_redraw();
            return;
        }
    }
}

void MusicMillerView::handle_parent_item_click(size_t idx) {
    if (m_path.empty()) return;

    if (m_path.size() == 1) {
        // Swapping root category
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
            std::string cat_names[] = {"Queue", "Artists", "Albums", "Playlists", "Library", "Visualizer"};
            m_path.push_back({cat_names[idx], "category"});
            m_selected_artist.clear();
            m_selected_album.clear();
            m_selected_playlist.clear();
            m_current_index = 0;
            rebuild_columns();
            if (m_on_state_changed) m_on_state_changed();
        }
        return;
    }

    if (m_path.size() == 2) {
        // Swapping artist / album / playlist
        if (m_active_category == MusicCategory::Artists && idx < m_cached_artists.size()) {
            m_selected_artist = m_cached_artists[idx];
            m_path.back() = {m_selected_artist, "artist"};
            m_selected_album.clear();
            m_current_index = 0;
            rebuild_columns();
            if (m_on_state_changed) m_on_state_changed();
            return;
        }
        if (m_active_category == MusicCategory::Albums && idx < m_cached_albums.size()) {
            m_selected_album = m_cached_albums[idx];
            m_path.back() = {m_selected_album, "album"};
            m_current_index = 0;
            rebuild_columns();
            if (m_on_state_changed) m_on_state_changed();
            return;
        }
        if (m_active_category == MusicCategory::Playlists && idx < m_cached_playlists.size()) {
            m_selected_playlist = m_cached_playlists[idx];
            m_path.back() = {m_selected_playlist, "playlist"};
            m_current_index = 0;
            rebuild_columns();
            if (m_on_state_changed) m_on_state_changed();
            return;
        }
    }

    if (m_path.size() == 3) {
        if (m_active_category == MusicCategory::Artists) {
            std::set<std::string> artist_albums;
            for (const auto& song : m_cached_all_songs) {
                if (song.artist == m_selected_artist && !song.album.empty()) {
                    artist_albums.insert(song.album);
                }
            }
            std::vector<std::string> alb_vec(artist_albums.begin(), artist_albums.end());
            if (idx < alb_vec.size()) {
                m_selected_album = alb_vec[idx];
                m_path.back() = {m_selected_album, "album"};
                m_current_index = 0;
                rebuild_columns();
                if (m_on_state_changed) m_on_state_changed();
                return;
            }
        }
    }
}

void MusicMillerView::rebuild_current_column() {
    if (!m_current_list) return;
    m_current_list->clear_items();
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
            m_current_list->add_item(create_song_list_item(i, m_current_display_songs[i], is_cur));
        }
        m_current_list->set_selected_index(m_current_index);
        m_current_list->request_redraw();
        return;
    }

    // 2. Root Depth 0: Show main categories in Middle Column!
    if (m_path.empty()) {
        struct CatDef {
            MusicCategory cat;
            std::string title;
            std::string subtitle;
            std::string icon;
        } root_cats[] = {
            {MusicCategory::Queue, "Queue", "Current playback queue & history", "󰐊"},
            {MusicCategory::Artists, "Artists", "Browse library by artist", "󰠃"},
            {MusicCategory::Albums, "Albums", "Browse album discography", "󰀥"},
            {MusicCategory::Playlists, "Playlists", "Saved MPD playlists", "󰲸"},
            {MusicCategory::Database, "Library", "All music files & tags", "󰋊"},
            {MusicCategory::Visualizer, "Visualizer", "Real-time spectrum & waveform analyzer", "󰝚"}
        };

        for (size_t i = 0; i < sizeof(root_cats) / sizeof(root_cats[0]); ++i) {
            bool selected = (static_cast<int>(i) == m_current_index);
            m_current_list->add_item(create_text_list_item(root_cats[i].title, root_cats[i].subtitle, selected));
        }
        m_current_list->set_selected_index(m_current_index);
        m_current_list->request_redraw();
        return;
    }

    // 3. Depth 1: Category Contents
    if (m_active_category == MusicCategory::Queue) {
        m_current_display_songs = m_cached_queue;
        for (size_t i = 0; i < m_cached_queue.size(); ++i) {
            bool is_cur = (m_last_status.song_pos >= 0 && static_cast<int>(i) == m_last_status.song_pos);
            m_current_list->add_item(create_song_list_item(i, m_cached_queue[i], is_cur));
        }
        m_current_list->set_selected_index(m_current_index);
        m_current_list->request_redraw();
        return;
    }

    if (m_active_category == MusicCategory::Artists) {
        if (m_path.size() == 1) {
            // Show all artists
            for (size_t i = 0; i < m_cached_artists.size(); ++i) {
                m_current_list->add_item(create_text_list_item(m_cached_artists[i], "Artist", static_cast<int>(i) == m_current_index));
            }
        } else if (m_path.size() == 2) {
            // Show albums by selected artist
            std::set<std::string> artist_albums;
            for (const auto& song : m_cached_all_songs) {
                if (song.artist == m_selected_artist && !song.album.empty()) {
                    artist_albums.insert(song.album);
                }
            }
            std::vector<std::string> alb_vec(artist_albums.begin(), artist_albums.end());
            for (size_t i = 0; i < alb_vec.size(); ++i) {
                m_current_list->add_item(create_text_list_item(alb_vec[i], m_selected_artist, static_cast<int>(i) == m_current_index));
            }
        } else {
            // Show tracks in this album
            for (const auto& song : m_cached_all_songs) {
                if (song.artist == m_selected_artist && song.album == m_selected_album) {
                    m_current_display_songs.push_back(song);
                }
            }
            for (size_t i = 0; i < m_current_display_songs.size(); ++i) {
                bool is_cur = (m_last_song.uri == m_current_display_songs[i].uri && !m_last_song.uri.empty());
                m_current_list->add_item(create_song_list_item(i, m_current_display_songs[i], is_cur));
            }
        }
        m_current_list->set_selected_index(m_current_index);
        m_current_list->request_redraw();
        return;
    }

    if (m_active_category == MusicCategory::Albums) {
        if (m_path.size() == 1) {
            for (size_t i = 0; i < m_cached_albums.size(); ++i) {
                m_current_list->add_item(create_text_list_item(m_cached_albums[i], "Album", static_cast<int>(i) == m_current_index));
            }
        } else {
            for (const auto& song : m_cached_all_songs) {
                if (song.album == m_selected_album) {
                    m_current_display_songs.push_back(song);
                }
            }
            for (size_t i = 0; i < m_current_display_songs.size(); ++i) {
                bool is_cur = (m_last_song.uri == m_current_display_songs[i].uri && !m_last_song.uri.empty());
                m_current_list->add_item(create_song_list_item(i, m_current_display_songs[i], is_cur));
            }
        }
        m_current_list->set_selected_index(m_current_index);
        m_current_list->request_redraw();
        return;
    }

    if (m_active_category == MusicCategory::Playlists) {
        if (m_path.size() == 1) {
            for (size_t i = 0; i < m_cached_playlists.size(); ++i) {
                m_current_list->add_item(create_text_list_item(m_cached_playlists[i], "Saved Playlist", static_cast<int>(i) == m_current_index));
            }
        } else {
            m_current_display_songs = m_client ? m_client->get_playlist_songs(m_selected_playlist) : std::vector<Song>();
            for (size_t i = 0; i < m_current_display_songs.size(); ++i) {
                bool is_cur = (m_last_song.uri == m_current_display_songs[i].uri && !m_last_song.uri.empty());
                m_current_list->add_item(create_song_list_item(i, m_current_display_songs[i], is_cur));
            }
        }
        m_current_list->set_selected_index(m_current_index);
        m_current_list->request_redraw();
        return;
    }

    if (m_active_category == MusicCategory::Database) {
        m_current_display_songs = m_cached_all_songs;
        for (size_t i = 0; i < m_current_display_songs.size(); ++i) {
            bool is_cur = (m_last_song.uri == m_current_display_songs[i].uri && !m_last_song.uri.empty());
            m_current_list->add_item(create_song_list_item(i, m_current_display_songs[i], is_cur));
        }
        m_current_list->set_selected_index(m_current_index);
        m_current_list->request_redraw();
        return;
    }

    if (m_active_category == MusicCategory::Visualizer) {
        m_current_list->add_item(create_text_list_item("Neon Spectrum Bars", "Dynamic frequency bands with gradient glow", m_full_visualizer && m_full_visualizer->get_mode() == VisualizerMode::SpectrumBars));
        m_current_list->add_item(create_text_list_item("Waveform Oscilloscope", "Fluid real-time audio scope curve", m_full_visualizer && m_full_visualizer->get_mode() == VisualizerMode::WaveformScope));
        m_current_list->add_item(create_text_list_item("Glow Peaks Analyzer", "Frequency meters with floor ambient reflection", m_full_visualizer && m_full_visualizer->get_mode() == VisualizerMode::GlowPeaks));
        m_current_list->set_selected_index(m_current_index);
        m_current_list->request_redraw();
        return;
    }
}

void MusicMillerView::update_right_pillar() {
    if (m_active_category == MusicCategory::Visualizer && !m_path.empty()) {
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

    if (m_active_category == MusicCategory::Queue || !m_current_display_songs.empty()) {
        rebuild_current_column();
    }
}

void MusicMillerView::update_queue(const std::vector<Song>& queue) {
    m_cached_queue = queue;
    if (m_active_category == MusicCategory::Queue && !m_path.empty()) {
        rebuild_current_column();
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
    if (!m_current_list || m_current_list->get_item_count() == 0) return false;
    int cur = m_current_list->get_selected_index();
    if (cur > 0) {
        m_current_list->set_selected_index(cur - 1);
        m_current_index = cur - 1;
        m_current_list->request_redraw();
        return true;
    }
    return false;
}

bool MusicMillerView::navigate_down() {
    if (!m_current_list || m_current_list->get_item_count() == 0) return false;
    int cur = m_current_list->get_selected_index();
    if (cur + 1 < static_cast<int>(m_current_list->get_item_count())) {
        m_current_list->set_selected_index(cur + 1);
        m_current_index = cur + 1;
        m_current_list->request_redraw();
        return true;
    }
    return false;
}

bool MusicMillerView::navigate_enter() {
    if (!m_current_list || m_current_list->get_item_count() == 0) return false;
    int idx = m_current_list->get_selected_index();
    if (idx < 0) idx = 0;

    // Search Mode: play selected search result
    if (is_searching()) {
        if (idx < static_cast<int>(m_current_display_songs.size()) && m_client) {
            m_client->add_and_play(m_current_display_songs[idx].uri);
        }
        return true;
    }

    // 1. Root Depth 0: enter selected category
    if (m_path.empty()) {
        MusicCategory cats[] = {
            MusicCategory::Queue,
            MusicCategory::Artists,
            MusicCategory::Albums,
            MusicCategory::Playlists,
            MusicCategory::Database,
            MusicCategory::Visualizer
        };
        std::string cat_names[] = {"Queue", "Artists", "Albums", "Playlists", "Library", "Visualizer"};

        if (idx < static_cast<int>(sizeof(cats) / sizeof(cats[0]))) {
            m_active_category = cats[idx];
            m_path.push_back({cat_names[idx], "category"});
            m_history_indices.push_back(idx);
            m_current_index = 0;
            rebuild_columns();
            if (m_on_state_changed) m_on_state_changed();
            return true;
        }
        return false;
    }

    // 2. Queue Mode: play track at pos
    if (m_active_category == MusicCategory::Queue) {
        if (idx < static_cast<int>(m_cached_queue.size()) && m_client) {
            m_client->play_pos(static_cast<unsigned int>(idx));
        }
        return true;
    }

    // 3. Artists Mode
    if (m_active_category == MusicCategory::Artists) {
        if (m_path.size() == 1) {
            if (idx < static_cast<int>(m_cached_artists.size())) {
                m_selected_artist = m_cached_artists[idx];
                m_path.push_back({m_selected_artist, "artist"});
                m_history_indices.push_back(idx);
                m_current_index = 0;
                rebuild_columns();
                if (m_on_state_changed) m_on_state_changed();
                return true;
            }
        } else if (m_path.size() == 2) {
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
                m_current_index = 0;
                rebuild_columns();
                if (m_on_state_changed) m_on_state_changed();
                return true;
            }
        } else {
            // Play track in album
            if (idx < static_cast<int>(m_current_display_songs.size()) && m_client) {
                m_client->add_and_play(m_current_display_songs[idx].uri);
            }
            return true;
        }
    }

    // 4. Albums Mode
    if (m_active_category == MusicCategory::Albums) {
        if (m_path.size() == 1) {
            if (idx < static_cast<int>(m_cached_albums.size())) {
                m_selected_album = m_cached_albums[idx];
                m_path.push_back({m_selected_album, "album"});
                m_history_indices.push_back(idx);
                m_current_index = 0;
                rebuild_columns();
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

    // 5. Playlists Mode
    if (m_active_category == MusicCategory::Playlists) {
        if (m_path.size() == 1) {
            if (idx < static_cast<int>(m_cached_playlists.size())) {
                m_selected_playlist = m_cached_playlists[idx];
                m_path.push_back({m_selected_playlist, "playlist"});
                m_history_indices.push_back(idx);
                m_current_index = 0;
                rebuild_columns();
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

    // 6. Database Mode
    if (m_active_category == MusicCategory::Database) {
        if (idx < static_cast<int>(m_current_display_songs.size()) && m_client) {
            m_client->add_and_play(m_current_display_songs[idx].uri);
        }
        return true;
    }

    // 7. Visualizer Mode
    if (m_active_category == MusicCategory::Visualizer && m_full_visualizer) {
        if (idx == 0) m_full_visualizer->set_mode(VisualizerMode::SpectrumBars);
        else if (idx == 1) m_full_visualizer->set_mode(VisualizerMode::WaveformScope);
        else if (idx == 2) m_full_visualizer->set_mode(VisualizerMode::GlowPeaks);
        rebuild_current_column();
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

        if (m_path.empty()) {
            m_selected_artist.clear();
            m_selected_album.clear();
            m_selected_playlist.clear();
        } else if (m_path.size() == 1) {
            m_selected_artist.clear();
            m_selected_album.clear();
            m_selected_playlist.clear();
        } else if (m_path.size() == 2) {
            m_selected_album.clear();
        }

        if (!m_history_indices.empty()) {
            m_current_index = m_history_indices.back();
            m_history_indices.pop_back();
        } else {
            m_current_index = 0;
        }

        rebuild_columns();
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
        m_current_index = 0;
        rebuild_columns();
        if (m_on_state_changed) m_on_state_changed();
        return;
    }

    while (m_path.size() > depth) {
        m_path.pop_back();
        if (!m_history_indices.empty()) {
            m_current_index = m_history_indices.back();
            m_history_indices.pop_back();
        }
    }

    if (m_active_category == MusicCategory::Artists) {
        if (depth == 1) {
            m_selected_album.clear();
        }
    }

    rebuild_columns();
    if (m_on_state_changed) m_on_state_changed();
}

void MusicMillerView::set_preview_expanded(bool expanded) {
    if (m_preview_expanded != expanded) {
        m_preview_expanded = expanded;
        if (m_parent_pillar) m_parent_pillar->set_visibility(expanded ? Visibility::Gone : Visibility::Visible);
        if (m_current_pillar) m_current_pillar->set_visibility(expanded ? Visibility::Gone : Visibility::Visible);
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
    return "";
}

void MusicMillerView::set_search_query(const std::string& query) {
    if (m_search_query != query) {
        m_search_query = query;
        m_current_index = 0;
        rebuild_columns();
        if (m_on_state_changed) m_on_state_changed();
    }
}

void MusicMillerView::sync_theme() {
    auto config = Config::get();
    m_parent_hero = create_parent_hero_box(config);
    rebuild_columns();
    if (m_player_card) m_player_card->sync_theme();
    request_redraw();
}

} // namespace miqumusic
