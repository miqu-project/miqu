#include "music_miller_view.hpp"
#include <algorithm>
#include <set>
#include <iostream>
#include <filesystem>

using namespace miqu;

namespace miqumusic {

constexpr int MATCH_PARENT = static_cast<int>(LayoutDimension::MatchParent);
constexpr int WRAP_CONTENT = static_cast<int>(LayoutDimension::WrapContent);

MusicMillerView::MusicMillerView(std::shared_ptr<MpdClient> client)
    : LinearLayout(Orientation::Horizontal), m_client(std::move(client)) {}

void MusicMillerView::init() {
    setup_columns();
    refresh_library();
    rebuild_columns();
}

void MusicMillerView::setup_columns() {
    auto config = Config::get();
    clear_views();
    set_divider_spacing(10);
    set_padding(12, 4, 12, 12);
    set_background_color(Color::transparent());

    int inner_rad = config ? config->metrics.corner_radius - 2 : 8;

    // 1. Column 1: Parent Pillar (width 280)
    m_parent_pillar = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(4)
        ->padding(10, 10)
        ->backgroundColor(Color::transparent())
        ->build();

    m_parent_hero = create_parent_hero_box(config);
    m_parent_pillar->add_view(m_parent_hero, LayoutParams(MATCH_PARENT, MATCH_PARENT, 1.0f));

    m_parent_list = ListViewBuilder::create()
        ->itemHeight(40)
        ->spacing(4)
        ->itemCornerRadius(inner_rad)
        ->scrollbar(false)
        ->build();
    m_parent_list->set_selectable(true);
    m_parent_list->set_keyboard_navigation_enabled(false);
    m_parent_list->set_on_item_click_listener([this](size_t idx, std::shared_ptr<View>) {
        handle_parent_item_click(idx);
    });
    m_parent_list->set_visibility(Visibility::Gone);
    m_parent_pillar->add_view(m_parent_list, LayoutParams(MATCH_PARENT, MATCH_PARENT, 1.0f));

    add_view(m_parent_pillar, LayoutParams(280, MATCH_PARENT));

    // 2. Column 2: Current / Center Pillar (width 300)
    m_current_pillar = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(4)
        ->padding(10, 10)
        ->backgroundColor(Color::transparent())
        ->build();

    m_current_list = ListViewBuilder::create()
        ->itemHeight(40)
        ->spacing(4)
        ->itemCornerRadius(inner_rad)
        ->scrollbar(false)
        ->build();
    m_current_list->set_selectable(true);
    m_current_list->set_keyboard_navigation_enabled(false);
    m_current_list->set_on_item_click_listener([this](size_t idx, std::shared_ptr<View>) {
        int clicked_idx = static_cast<int>(idx);
        if (clicked_idx != m_current_index) {
            m_current_index = clicked_idx;
            rebuild_columns();
            if (m_on_state_changed) m_on_state_changed();
        } else {
            navigate_enter();
        }
    });

    m_current_pillar->add_view(m_current_list, LayoutParams(MATCH_PARENT, MATCH_PARENT, 1.0f));
    add_view(m_current_pillar, LayoutParams(300, MATCH_PARENT));

    // 3. Column 3: Right Pillar (Hero Player Card / Visualizer)
    m_right_pillar = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(4)
        ->padding(6, 6)
        ->backgroundColor(Color::transparent())
        ->build();

    m_player_card = std::make_shared<PlayerCardView>(m_client);
    m_right_pillar->add_view(m_player_card, LayoutParams(MATCH_PARENT, MATCH_PARENT, 1.0f));

    m_full_visualizer = std::make_shared<VisualizerView>();
    m_full_visualizer->set_visibility(Visibility::Gone);
    m_right_pillar->add_view(m_full_visualizer, LayoutParams(MATCH_PARENT, MATCH_PARENT, 1.0f));

    add_view(m_right_pillar, LayoutParams(MATCH_PARENT, MATCH_PARENT, 1.0f));

    update_pillar_styles();
}

void MusicMillerView::update_pillar_styles() {
    auto config = Config::get();
    if (!config) return;

    int radius = config->metrics.corner_radius;
    Color active_border = config->colors.primary.with_alpha(0.55f);

    // 1. Column 1: Parent Pillar (Always transparent, 0 border)
    if (m_parent_pillar) {
        m_parent_pillar->set_corner_radius(radius);
        m_parent_pillar->set_stroke(0, Color::transparent());
        m_parent_pillar->set_background_color(Color::transparent());
    }

    // 2. Column 2: Center Pillar (Subtle active border, indicating active navigation pillar)
    if (m_current_pillar) {
        m_current_pillar->set_corner_radius(radius);
        m_current_pillar->set_stroke(1, active_border);
        m_current_pillar->set_background_color(Color::transparent());
    }

    // 3. Column 3: Right Pillar Container
    if (m_right_pillar) {
        m_right_pillar->set_corner_radius(radius);
        m_right_pillar->set_stroke(0, Color::transparent());
        m_right_pillar->set_background_color(Color::transparent());
    }
}

std::shared_ptr<View> MusicMillerView::create_parent_hero_box(const std::shared_ptr<Config>& config) {
    auto hero_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->gravity(Gravity::Center)
        ->padding(16, 24)
        ->spacing(12)
        ->build();

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
    update_pillar_styles();
    rebuild_parent_column();
    rebuild_current_column();
    update_right_pillar();
    request_redraw();
}

std::shared_ptr<View> MusicMillerView::create_row_item(const std::string& title, const std::string& icon, bool has_children) {
    auto config = Config::get();
    auto item_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(12, 0)
        ->spacing(10)
        ->gravity(Gravity::CenterVertical)
        ->backgroundColor(Color::transparent())
        ->build();

    if (!icon.empty()) {
        auto icon_view = TextViewBuilder::create()
            ->text(icon)
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface_variant)
            ->build();
        item_row->add_view(icon_view, LayoutParams(24, WRAP_CONTENT, Gravity::CenterVertical));
    }

    auto title_view = TextViewBuilder::create()
        ->text(title)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->ellipsize(true)
        ->build();
    item_row->add_view(title_view, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));

    if (has_children) {
        auto chevron = TextViewBuilder::create()
            ->text("›")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.45f))
            ->build();
        item_row->add_view(chevron, LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity::CenterVertical));
    }

    return item_row;
}

std::shared_ptr<View> MusicMillerView::create_song_row_item(size_t index, const Song& song, bool is_current) {
    auto config = Config::get();
    auto item_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(10, 0)
        ->spacing(8)
        ->gravity(Gravity::CenterVertical)
        ->backgroundColor(Color::transparent())
        ->build();

    std::string num_str = std::to_string(index + 1) + ".";
    if (index + 1 < 10) num_str = "0" + num_str;
    auto num_view = TextViewBuilder::create()
        ->text(num_str)
        ->textSize(config->metrics.font_size - 1)
        ->textColor(is_current ? config->colors.primary : config->colors.on_surface_variant)
        ->build();
    item_row->add_view(num_view, LayoutParams(28, WRAP_CONTENT, Gravity::CenterVertical));

    std::string title_str = song.title.empty() ? song.uri : song.title;
    auto title_view = TextViewBuilder::create()
        ->text(title_str)
        ->textSize(config->metrics.font_size)
        ->bold(is_current)
        ->textColor(is_current ? config->colors.primary : config->colors.on_surface)
        ->ellipsize(true)
        ->build();
    item_row->add_view(title_view, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));

    if (song.duration > 0) {
        auto dur_view = TextViewBuilder::create()
            ->text(song.formatted_duration())
            ->textSize(config->metrics.font_size - 1)
            ->textColor(config->colors.on_surface_variant)
            ->build();
        item_row->add_view(dur_view, LayoutParams(46, WRAP_CONTENT, Gravity::CenterVertical));
    }

    return item_row;
}

void MusicMillerView::rebuild_parent_column() {
    // 1. Root Depth 0: Show empty parent hero box with app icon, name, and shortcuts!
    if (m_path.empty() && m_search_query.empty()) {
        if (m_parent_hero) m_parent_hero->set_visibility(Visibility::Visible);
        if (m_parent_list) {
            m_parent_list->clear_items();
            m_parent_list->set_visibility(Visibility::Gone);
        }
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
        // Parent is Root: Show root categories, highlight active category
        int active_idx = -1;
        for (size_t i = 0; i < sizeof(root_cats) / sizeof(root_cats[0]); ++i) {
            bool selected = (!is_searching() && m_active_category == root_cats[i].cat);
            if (selected) active_idx = static_cast<int>(i);
            m_parent_list->add_item(create_row_item(root_cats[i].title, root_cats[i].icon, false));
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
                m_parent_list->add_item(create_row_item(m_cached_artists[i], "󰠃", true));
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
                m_parent_list->add_item(create_row_item(m_cached_albums[i], "󰀥", true));
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
                m_parent_list->add_item(create_row_item(m_cached_playlists[i], "󰲸", true));
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
                m_parent_list->add_item(create_row_item(alb_vec[i], "󰀥", true));
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
        std::string cat_titles[] = {"Queue", "Artists", "Albums", "Playlists", "Library", "Visualizer"};

        if (idx < sizeof(cats) / sizeof(cats[0])) {
            m_active_category = cats[idx];
            m_path.back() = {cat_titles[idx], "category"};
            m_current_index = 0;
            rebuild_columns();
            if (m_on_state_changed) m_on_state_changed();
        }
        return;
    }

    if (m_path.size() == 2) {
        if (m_active_category == MusicCategory::Artists) {
            if (idx < m_cached_artists.size()) {
                m_selected_artist = m_cached_artists[idx];
                m_path.back() = {m_selected_artist, "artist"};
                m_current_index = 0;
                rebuild_columns();
                if (m_on_state_changed) m_on_state_changed();
            }
            return;
        }
        if (m_active_category == MusicCategory::Albums) {
            if (idx < m_cached_albums.size()) {
                m_selected_album = m_cached_albums[idx];
                m_path.back() = {m_selected_album, "album"};
                m_current_index = 0;
                rebuild_columns();
                if (m_on_state_changed) m_on_state_changed();
            }
            return;
        }
        if (m_active_category == MusicCategory::Playlists) {
            if (idx < m_cached_playlists.size()) {
                m_selected_playlist = m_cached_playlists[idx];
                m_path.back() = {m_selected_playlist, "playlist"};
                m_current_index = 0;
                rebuild_columns();
                if (m_on_state_changed) m_on_state_changed();
            }
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
            }
            return;
        }
    }
}

void MusicMillerView::rebuild_current_column() {
    if (!m_current_list) return;

    m_current_list->clear_items();
    m_current_display_songs.clear();

    // 1. Search Mode
    if (is_searching()) {
        m_current_display_songs = m_search_results;
        for (size_t i = 0; i < m_search_results.size(); ++i) {
            const auto& s = m_search_results[i];
            bool is_cur = (s.id == m_last_song.id);
            m_current_list->add_item(create_song_row_item(i, s, is_cur));
        }
        if (!m_search_results.empty()) {
            m_current_index = std::clamp(m_current_index, 0, static_cast<int>(m_search_results.size()) - 1);
            m_current_list->set_selected_index(m_current_index);
        }
        m_current_list->request_redraw();
        return;
    }

    // 2. Root Depth 0: Show Root Categories list in Center Column!
    if (m_path.empty()) {
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

        for (size_t i = 0; i < sizeof(root_cats) / sizeof(root_cats[0]); ++i) {
            m_current_list->add_item(create_row_item(root_cats[i].title, root_cats[i].icon, true));
        }
        int max_idx = static_cast<int>(sizeof(root_cats) / sizeof(root_cats[0])) - 1;
        m_current_index = std::clamp(m_current_index, 0, max_idx);
        m_current_list->set_selected_index(m_current_index);
        m_current_list->request_redraw();
        return;
    }

    // 3. Queue Mode
    if (m_active_category == MusicCategory::Queue) {
        m_current_display_songs = m_cached_queue;
        for (size_t i = 0; i < m_cached_queue.size(); ++i) {
            const auto& s = m_cached_queue[i];
            bool is_cur = (s.id == m_last_song.id);
            m_current_list->add_item(create_song_row_item(i, s, is_cur));
        }
        if (!m_cached_queue.empty()) {
            m_current_index = std::clamp(m_current_index, 0, static_cast<int>(m_cached_queue.size()) - 1);
            m_current_list->set_selected_index(m_current_index);
        }
        m_current_list->request_redraw();
        return;
    }

    // 4. Artists Mode
    if (m_active_category == MusicCategory::Artists) {
        if (m_path.size() == 1) {
            // Depth 1: List of Artists
            for (size_t i = 0; i < m_cached_artists.size(); ++i) {
                m_current_list->add_item(create_row_item(m_cached_artists[i], "󰠃", true));
            }
            if (!m_cached_artists.empty()) {
                m_current_index = std::clamp(m_current_index, 0, static_cast<int>(m_cached_artists.size()) - 1);
                m_current_list->set_selected_index(m_current_index);
            }
        } else if (m_path.size() == 2) {
            // Depth 2: Albums of selected artist
            std::set<std::string> artist_albums;
            for (const auto& song : m_cached_all_songs) {
                if (song.artist == m_selected_artist && !song.album.empty()) {
                    artist_albums.insert(song.album);
                }
            }
            std::vector<std::string> alb_vec(artist_albums.begin(), artist_albums.end());
            for (size_t i = 0; i < alb_vec.size(); ++i) {
                m_current_list->add_item(create_row_item(alb_vec[i], "󰀥", true));
            }
            if (!alb_vec.empty()) {
                m_current_index = std::clamp(m_current_index, 0, static_cast<int>(alb_vec.size()) - 1);
                m_current_list->set_selected_index(m_current_index);
            }
        } else if (m_path.size() == 3) {
            // Depth 3: Songs of selected album
            for (const auto& song : m_cached_all_songs) {
                if (song.artist == m_selected_artist && song.album == m_selected_album) {
                    m_current_display_songs.push_back(song);
                }
            }
            for (size_t i = 0; i < m_current_display_songs.size(); ++i) {
                const auto& s = m_current_display_songs[i];
                bool is_cur = (s.id == m_last_song.id);
                m_current_list->add_item(create_song_row_item(i, s, is_cur));
            }
            if (!m_current_display_songs.empty()) {
                m_current_index = std::clamp(m_current_index, 0, static_cast<int>(m_current_display_songs.size()) - 1);
                m_current_list->set_selected_index(m_current_index);
            }
        }
        m_current_list->request_redraw();
        return;
    }

    // 5. Albums Mode
    if (m_active_category == MusicCategory::Albums) {
        if (m_path.size() == 1) {
            // Depth 1: List of Albums
            for (size_t i = 0; i < m_cached_albums.size(); ++i) {
                m_current_list->add_item(create_row_item(m_cached_albums[i], "󰀥", true));
            }
            if (!m_cached_albums.empty()) {
                m_current_index = std::clamp(m_current_index, 0, static_cast<int>(m_cached_albums.size()) - 1);
                m_current_list->set_selected_index(m_current_index);
            }
        } else if (m_path.size() == 2) {
            // Depth 2: Songs of selected album
            for (const auto& song : m_cached_all_songs) {
                if (song.album == m_selected_album) {
                    m_current_display_songs.push_back(song);
                }
            }
            for (size_t i = 0; i < m_current_display_songs.size(); ++i) {
                const auto& s = m_current_display_songs[i];
                bool is_cur = (s.id == m_last_song.id);
                m_current_list->add_item(create_song_row_item(i, s, is_cur));
            }
            if (!m_current_display_songs.empty()) {
                m_current_index = std::clamp(m_current_index, 0, static_cast<int>(m_current_display_songs.size()) - 1);
                m_current_list->set_selected_index(m_current_index);
            }
        }
        m_current_list->request_redraw();
        return;
    }

    // 6. Playlists Mode
    if (m_active_category == MusicCategory::Playlists) {
        if (m_path.size() == 1) {
            // Depth 1: List of Playlists
            for (size_t i = 0; i < m_cached_playlists.size(); ++i) {
                m_current_list->add_item(create_row_item(m_cached_playlists[i], "󰲸", true));
            }
            if (!m_cached_playlists.empty()) {
                m_current_index = std::clamp(m_current_index, 0, static_cast<int>(m_cached_playlists.size()) - 1);
                m_current_list->set_selected_index(m_current_index);
            }
        } else if (m_path.size() == 2) {
            // Depth 2: Songs of playlist
            if (m_client) {
                m_current_display_songs = m_client->get_playlist_songs(m_selected_playlist);
            }
            for (size_t i = 0; i < m_current_display_songs.size(); ++i) {
                const auto& s = m_current_display_songs[i];
                bool is_cur = (s.id == m_last_song.id);
                m_current_list->add_item(create_song_row_item(i, s, is_cur));
            }
            if (!m_current_display_songs.empty()) {
                m_current_index = std::clamp(m_current_index, 0, static_cast<int>(m_current_display_songs.size()) - 1);
                m_current_list->set_selected_index(m_current_index);
            }
        }
        m_current_list->request_redraw();
        return;
    }

    // 7. Library (Database) Mode
    if (m_active_category == MusicCategory::Database) {
        m_current_display_songs = m_cached_all_songs;
        for (size_t i = 0; i < m_cached_all_songs.size(); ++i) {
            const auto& s = m_cached_all_songs[i];
            bool is_cur = (s.id == m_last_song.id);
            m_current_list->add_item(create_song_row_item(i, s, is_cur));
        }
        if (!m_cached_all_songs.empty()) {
            m_current_index = std::clamp(m_current_index, 0, static_cast<int>(m_cached_all_songs.size()) - 1);
            m_current_list->set_selected_index(m_current_index);
        }
        m_current_list->request_redraw();
        return;
    }

    // 8. Visualizer Mode
    if (m_active_category == MusicCategory::Visualizer) {
        m_current_list->add_item(create_row_item("Spectrum Bars", "󰝚", false));
        m_current_list->add_item(create_row_item("Waveform Scope", "󰝚", false));
        m_current_list->add_item(create_row_item("Glow Peaks", "󰝚", false));
        m_current_index = std::clamp(m_current_index, 0, 2);
        m_current_list->set_selected_index(m_current_index);
        m_current_list->request_redraw();
        return;
    }
}

void MusicMillerView::update_right_pillar() {
    bool is_vis = (m_active_category == MusicCategory::Visualizer);
    if (m_full_visualizer) {
        m_full_visualizer->set_visibility(is_vis ? Visibility::Visible : Visibility::Gone);
    }
    if (m_player_card) {
        m_player_card->set_visibility(is_vis ? Visibility::Gone : Visibility::Visible);
    }
}

void MusicMillerView::tick(double delta_time) {
    if (m_active_category == MusicCategory::Visualizer && m_full_visualizer && m_full_visualizer->is_visible()) {
        m_full_visualizer->tick(delta_time);
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
}

void MusicMillerView::update_queue(const std::vector<Song>& queue) {
    m_cached_queue = queue;
    if (m_active_category == MusicCategory::Queue) {
        rebuild_current_column();
    }
}

bool MusicMillerView::navigate_up() {
    if (!m_current_list) return false;
    int count = static_cast<int>(m_current_list->get_item_count());
    if (count <= 0) return false;

    m_current_index = std::max(0, m_current_index - 1);
    m_current_list->set_selected_index(m_current_index);
    m_current_list->request_redraw();
    return true;
}

bool MusicMillerView::navigate_down() {
    if (!m_current_list) return false;
    int count = static_cast<int>(m_current_list->get_item_count());
    if (count <= 0) return false;

    m_current_index = std::min(count - 1, m_current_index + 1);
    m_current_list->set_selected_index(m_current_index);
    m_current_list->request_redraw();
    return true;
}

bool MusicMillerView::navigate_enter() {
    int idx = m_current_index;

    // Search Mode
    if (is_searching()) {
        if (idx < static_cast<int>(m_search_results.size()) && m_client) {
            m_client->add_and_play(m_search_results[idx].uri);
        }
        return true;
    }

    // 1. Root Depth 0: enter selected category
    if (m_path.empty()) {
        struct CatDef {
            MusicCategory cat;
            std::string title;
        } root_cats[] = {
            {MusicCategory::Queue, "Queue"},
            {MusicCategory::Artists, "Artists"},
            {MusicCategory::Albums, "Albums"},
            {MusicCategory::Playlists, "Playlists"},
            {MusicCategory::Database, "Library"},
            {MusicCategory::Visualizer, "Visualizer"}
        };

        if (idx < static_cast<int>(sizeof(root_cats) / sizeof(root_cats[0]))) {
            m_active_category = root_cats[idx].cat;
            m_path.push_back({root_cats[idx].title, "category"});
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
    if (is_searching()) {
        set_search_query("");
        return true;
    }

    if (m_preview_expanded) {
        set_preview_expanded(false);
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

bool MusicMillerView::can_navigate_back() const {
    return is_searching() || m_preview_expanded || !m_path.empty();
}

void MusicMillerView::navigate_to_depth(size_t depth) {
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

    if (m_path.size() == 1) {
        m_selected_artist.clear();
        m_selected_album.clear();
        m_selected_playlist.clear();
    } else if (m_path.size() == 2) {
        m_selected_album.clear();
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

std::string MusicMillerView::get_current_leaf_title() const {
    if (!m_path.empty()) {
        return m_path.back().title;
    }
    return "";
}

void MusicMillerView::set_search_query(const std::string& query) {
    if (m_search_query != query) {
        m_search_query = query;
        m_current_index = 0;
        m_search_results.clear();
        if (!query.empty()) {
            std::string q_lower = query;
            std::transform(q_lower.begin(), q_lower.end(), q_lower.begin(), ::tolower);
            for (const auto& song : m_cached_all_songs) {
                std::string title_l = song.title;
                std::string artist_l = song.artist;
                std::string album_l = song.album;
                std::transform(title_l.begin(), title_l.end(), title_l.begin(), ::tolower);
                std::transform(artist_l.begin(), artist_l.end(), artist_l.begin(), ::tolower);
                std::transform(album_l.begin(), album_l.end(), album_l.begin(), ::tolower);

                if (title_l.find(q_lower) != std::string::npos ||
                    artist_l.find(q_lower) != std::string::npos ||
                    album_l.find(q_lower) != std::string::npos ||
                    song.uri.find(q_lower) != std::string::npos) {
                    m_search_results.push_back(song);
                }
            }
        }
        rebuild_columns();
        if (m_on_state_changed) m_on_state_changed();
    }
}

void MusicMillerView::sync_theme() {
    update_pillar_styles();
    rebuild_columns();
    if (m_player_card) m_player_card->sync_theme();
    request_redraw();
}

bool MusicMillerView::on_mouse_move(int lx, int ly, const Rect& bounds) {
    m_last_mouse_x = lx;
    m_last_mouse_y = ly;
    return LinearLayout::on_mouse_move(lx, ly, bounds);
}

bool MusicMillerView::on_scroll(double delta) {
    if (!is_visible() || !is_enabled()) return false;
    Point pt(m_last_mouse_x, m_last_mouse_y);

    if (m_current_pillar && m_current_pillar->get_bounds().contains(pt)) {
        return m_current_list && m_current_list->is_visible() && m_current_list->on_scroll(delta);
    }
    if (m_parent_pillar && m_parent_pillar->get_bounds().contains(pt)) {
        return m_parent_list && m_parent_list->is_visible() && m_parent_list->on_scroll(delta);
    }
    return LinearLayout::on_scroll(delta);
}

} // namespace miqumusic
