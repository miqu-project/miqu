#include "miller_view.hpp"
#include "preview_common.hpp"
#include "art_loader.hpp"
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace miqu {

constexpr int MATCH_PARENT = static_cast<int>(LayoutDimension::MatchParent);
constexpr int WRAP_CONTENT = static_cast<int>(LayoutDimension::WrapContent);

void MillerView::render_empty_preview(const std::shared_ptr<Config>& config) {
    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->gravity(Gravity::Center)
        ->padding(24, 32)
        ->spacing(8)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto icon = TextViewBuilder::create()
        ->text("󰎆")
        ->textSize(static_cast<int>(config->metrics.font_size * 2.5f))
        ->textColor(config->colors.on_surface_variant.with_alpha(0.5f))
        ->center(true)
        ->build();
    card->add_view(icon);

    auto text = TextViewBuilder::create()
        ->text("No Track Selected")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->center(true)
        ->build();
    card->add_view(text);

    auto hint = TextViewBuilder::create()
        ->text("Select an item to view preview")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.6f))
        ->center(true)
        ->build();
    card->add_view(hint);

    add_preview_card(card);
}

void MillerView::render_category_children_preview(const std::shared_ptr<MusicItem>& item, const std::shared_ptr<Config>& config) {
    if (!item) return;
    item->ensure_children();
    if (m_category_list) {
        m_category_list->clear_items();
        m_category_list->set_visibility(Visibility::Visible);
        for (const auto& child : item->children) {
            m_category_list->add_item(create_list_row_item(child, config));
        }
    }
    if (m_preview_scroll) {
        m_preview_scroll->set_visibility(Visibility::Gone);
    }
}

void MillerView::render_song_preview(const std::shared_ptr<MusicItem>& item, const std::shared_ptr<Config>& config) {
    if (!item) return;
    const auto& song = item->song_data;

    // 1. Hero Song Card
    auto song_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 16)
        ->spacing(14)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto header_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(14)
        ->gravity(Gravity::CenterVertical)
        ->build();

    // Album art or placeholder
    std::string art_path;
    if (m_client) {
        art_path = miqumusic::ArtLoader::get_art_for_song(song, *m_client);
    }

    if (!art_path.empty() && fs::exists(art_path)) {
        auto img = ImageViewBuilder::create()
            ->source(art_path)
            ->cornerRadius(get_inner_radius(config))
            ->build();
        header_row->add_view(img, LayoutParams(96, 96));
    } else {
        auto art_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->gravity(Gravity::Center)
            ->backgroundColor(config->colors.surface_variant.with_alpha(0.4f))
            ->cornerRadius(get_inner_radius(config))
            ->build();

        auto note_icon = TextViewBuilder::create()
            ->text("󰎆")
            ->textSize(static_cast<int>(config->metrics.font_size * 2.2f))
            ->textColor(config->colors.primary.with_alpha(0.6f))
            ->center(true)
            ->build();
        art_box->add_view(note_icon);
        header_row->add_view(art_box, LayoutParams(96, 96));
    }

    // Info Column
    auto info_col = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(4)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto title_view = TextViewBuilder::create()
        ->text(song.display_title())
        ->textSize(static_cast<int>(config->metrics.font_size * 1.25f))
        ->bold(true)
        ->textColor(config->colors.on_surface)
        ->ellipsize(true)
        ->build();
    info_col->add_view(title_view);

    auto artist_view = TextViewBuilder::create()
        ->text(song.display_artist())
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.primary)
        ->ellipsize(true)
        ->build();
    info_col->add_view(artist_view);

    auto album_view = TextViewBuilder::create()
        ->text(song.display_album())
        ->textSize(static_cast<int>(config->metrics.font_size * 0.9f))
        ->textColor(config->colors.on_surface_variant)
        ->ellipsize(true)
        ->build();
    info_col->add_view(album_view);

    auto dur_view = TextViewBuilder::create()
        ->text(song.formatted_duration())
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    info_col->add_view(dur_view);

    header_row->add_view(info_col, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));
    song_card->add_view(header_row);

    // Divider
    auto div = LinearLayoutBuilder::create()
        ->backgroundColor(config->colors.outline.with_alpha(0.2f))
        ->build();
    song_card->add_view(div, LayoutParams(MATCH_PARENT, 1));

    // Action Buttons
    auto action_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(10)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto play_btn = ButtonBuilder::create()
        ->text("▶ Play Now")
        ->padding(12, 6)
        ->cornerRadius(get_btn_radius(config))
        ->textSize(config->metrics.font_size)
        ->build();
    play_btn->set_focusable(true);
    play_btn->set_on_click_listener([this, song]() {
        if (!m_client) return;
        if (!song.uri.empty()) {
            m_client->add_and_play(song.uri);
        } else {
            m_client->play_pos(song.pos);
        }
    });
    action_row->add_view(play_btn, LayoutParams(WRAP_CONTENT, 32));

    auto queue_btn = ButtonBuilder::create()
        ->text("󰐊 Add to Queue")
        ->flat(true)
        ->padding(12, 6)
        ->cornerRadius(get_btn_radius(config))
        ->textSize(config->metrics.font_size)
        ->build();
    queue_btn->set_focusable(true);
    queue_btn->set_on_click_listener([this, song]() {
        if (!m_client) return;
        if (!song.uri.empty()) {
            m_client->add_to_queue(song.uri);
        }
    });
    action_row->add_view(queue_btn, LayoutParams(WRAP_CONTENT, 32));

    song_card->add_view(action_row);
    add_preview_card(song_card);

    // 2. Metadata Info Card
    auto meta_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(8)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto add_meta_row = [&meta_card, config](const std::string& key, const std::string& value) {
        if (value.empty()) return;
        auto row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->spacing(8)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto k_view = TextViewBuilder::create()
            ->text(key)
            ->textSize(static_cast<int>(config->metrics.font_size * 0.9f))
            ->textColor(config->colors.on_surface_variant)
            ->build();
        row->add_view(k_view, LayoutParams(90, WRAP_CONTENT));

        auto v_view = TextViewBuilder::create()
            ->text(value)
            ->textSize(static_cast<int>(config->metrics.font_size * 0.9f))
            ->textColor(config->colors.on_surface)
            ->ellipsize(true)
            ->build();
        row->add_view(v_view, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));

        meta_card->add_view(row);
    };

    add_meta_row("Duration", song.formatted_duration());
    if (!song.track.empty()) add_meta_row("Track", song.track);
    if (!song.genre.empty()) add_meta_row("Genre", song.genre);
    if (!song.date.empty()) add_meta_row("Date", song.date);
    if (!song.uri.empty()) add_meta_row("Location", song.uri);

    add_preview_card(meta_card);
}

void MillerView::render_visualizer_preview(const std::shared_ptr<MusicItem>& item, const std::shared_ptr<Config>& config) {
    if (!item) return;

    if (!m_visualizer_view) {
        m_visualizer_view = std::make_shared<miqumusic::VisualizerView>();
    }

    m_visualizer_view->set_mode(item->visualizer_mode);

    if (m_client) {
        auto status = m_client->get_status();
        m_visualizer_view->set_playing(status.state == miqumusic::PlaybackState::Playing);
    }

    // 1. Visualizer Display Card
    auto vis_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(12, 12)
        ->spacing(8)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    vis_card->add_view(m_visualizer_view, LayoutParams(MATCH_PARENT, 220));
    add_preview_card(vis_card);

    // 2. Visualizer Controls Card
    auto ctrl_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto header = TextViewBuilder::create()
        ->text(item->title)
        ->textSize(static_cast<int>(config->metrics.font_size * 1.15f))
        ->bold(true)
        ->textColor(config->colors.on_surface)
        ->build();
    ctrl_card->add_view(header);

    auto subtitle = TextViewBuilder::create()
        ->text(item->subtitle)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    ctrl_card->add_view(subtitle);

    auto btn_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(10)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto cycle_btn = ButtonBuilder::create()
        ->text("󰓃 Cycle Mode")
        ->padding(12, 6)
        ->cornerRadius(get_btn_radius(config))
        ->textSize(config->metrics.font_size)
        ->build();
    cycle_btn->set_focusable(true);
    cycle_btn->set_on_click_listener([this]() {
        if (m_visualizer_view) {
            m_visualizer_view->cycle_mode();
            request_redraw();
        }
    });
    btn_row->add_view(cycle_btn, LayoutParams(WRAP_CONTENT, 32));

    auto play_toggle_btn = ButtonBuilder::create()
        ->text("⏯ Toggle Playback")
        ->flat(true)
        ->padding(12, 6)
        ->cornerRadius(get_btn_radius(config))
        ->textSize(config->metrics.font_size)
        ->build();
    play_toggle_btn->set_focusable(true);
    play_toggle_btn->set_on_click_listener([this]() {
        if (m_client) {
            m_client->toggle_pause();
            auto status = m_client->get_status();
            if (m_visualizer_view) {
                m_visualizer_view->set_playing(status.state == miqumusic::PlaybackState::Playing);
            }
            request_redraw();
        }
    });
    btn_row->add_view(play_toggle_btn, LayoutParams(WRAP_CONTENT, 32));

    ctrl_card->add_view(btn_row);
    add_preview_card(ctrl_card);
}

void MillerView::render_multi_selection_preview(const std::vector<std::shared_ptr<MusicItem>>& items, const std::shared_ptr<Config>& config) {
    if (items.empty()) return;

    size_t num_songs = 0;
    size_t num_cats = 0;
    unsigned int total_duration_secs = 0;

    for (const auto& it : items) {
        if (!it) continue;
        if (it->is_song) {
            num_songs++;
            total_duration_secs += it->song_data.duration;
        } else if (it->is_category) {
            num_cats++;
        }
    }

    // 1. Overview Hero Card
    auto hero_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->gravity(Gravity::Center)
        ->padding(20, 24)
        ->spacing(8)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto hero_icon = TextViewBuilder::create()
        ->text("󰎆")
        ->textSize(static_cast<int>(config->metrics.font_size * 2.4f))
        ->textColor(config->colors.primary)
        ->center(true)
        ->build();
    hero_card->add_view(hero_icon);

    auto hero_title = TextViewBuilder::create()
        ->text(std::to_string(items.size()) + " Tracks Selected")
        ->textSize(static_cast<int>(config->metrics.font_size * 1.25f))
        ->bold(true)
        ->textColor(config->colors.on_surface)
        ->center(true)
        ->build();
    hero_card->add_view(hero_title);

    std::string count_str;
    if (num_songs > 0 && num_cats > 0) {
        count_str = std::to_string(num_songs) + " tracks, " + std::to_string(num_cats) + " categories";
    } else if (num_cats > 0) {
        count_str = std::to_string(num_cats) + (num_cats == 1 ? " category" : " categories");
    } else {
        count_str = std::to_string(num_songs) + (num_songs == 1 ? " track" : " tracks");
    }

    if (total_duration_secs > 0) {
        unsigned int mins = total_duration_secs / 60;
        unsigned int secs = total_duration_secs % 60;
        char buf[32];
        snprintf(buf, sizeof(buf), " (%u:%02u)", mins, secs);
        count_str += buf;
    }

    auto count_label = TextViewBuilder::create()
        ->text(count_str)
        ->caption(true)
        ->textColor(config->colors.on_surface_variant)
        ->center(true)
        ->build();
    hero_card->add_view(count_label);

    // Batch Action Buttons
    auto action_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(10)
        ->gravity(Gravity::Center)
        ->padding(0, 6)
        ->build();

    auto play_all_btn = ButtonBuilder::create()
        ->text("▶ Play Selection")
        ->padding(14, 6)
        ->cornerRadius(get_btn_radius(config))
        ->textSize(config->metrics.font_size)
        ->build();
    play_all_btn->set_focusable(true);
    play_all_btn->set_on_click_listener([this, items]() {
        if (!m_client) return;
        bool first = true;
        for (const auto& it : items) {
            if (it && it->is_song && !it->song_data.uri.empty()) {
                if (first) {
                    m_client->add_and_play(it->song_data.uri);
                    first = false;
                } else {
                    m_client->add_to_queue(it->song_data.uri);
                }
            }
        }
        request_redraw();
    });
    action_row->add_view(play_all_btn, LayoutParams(WRAP_CONTENT, 32));

    auto enqueue_btn = ButtonBuilder::create()
        ->text("󰒺 Enqueue Selection")
        ->flat(true)
        ->padding(12, 6)
        ->cornerRadius(get_btn_radius(config))
        ->textSize(config->metrics.font_size)
        ->build();
    enqueue_btn->set_focusable(true);
    enqueue_btn->set_on_click_listener([this, items]() {
        if (!m_client) return;
        for (const auto& it : items) {
            if (it && it->is_song && !it->song_data.uri.empty()) {
                m_client->add_to_queue(it->song_data.uri);
            }
        }
        request_redraw();
    });
    action_row->add_view(enqueue_btn, LayoutParams(WRAP_CONTENT, 32));

    hero_card->add_view(action_row);
    add_preview_card(hero_card);

    // 2. Selected Tracks List Card
    auto list_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(14, 12)
        ->spacing(6)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto list_header = TextViewBuilder::create()
        ->text("Selected Tracks")
        ->bold(true)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    list_card->add_view(list_header);

    size_t max_preview_items = 8;
    for (size_t i = 0; i < std::min(items.size(), max_preview_items); ++i) {
        const auto& it = items[i];
        if (!it) continue;

        auto row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->spacing(8)
            ->gravity(Gravity::CenterVertical)
            ->padding(4, 3)
            ->build();

        auto icon = TextViewBuilder::create()
            ->text(it->icon.empty() ? (it->is_category ? "󰉋" : "󰎆") : it->icon)
            ->textSize(static_cast<int>(config->metrics.font_size * 0.95f))
            ->textColor(it->is_category ? config->colors.primary : config->colors.on_surface_variant)
            ->build();
        row->add_view(icon, LayoutParams(20, WRAP_CONTENT));

        auto title_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(1)
            ->build();

        auto name = TextViewBuilder::create()
            ->text(it->title)
            ->textSize(static_cast<int>(config->metrics.font_size * 0.95f))
            ->textColor(config->colors.on_surface)
            ->ellipsize(true)
            ->build();
        title_box->add_view(name);

        if (!it->subtitle.empty()) {
            auto sub = TextViewBuilder::create()
                ->text(it->subtitle)
                ->caption(true)
                ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
                ->ellipsize(true)
                ->build();
            title_box->add_view(sub);
        }

        row->add_view(title_box, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));

        if (it->is_song && it->song_data.duration > 0) {
            auto dur = TextViewBuilder::create()
                ->text(it->song_data.formatted_duration())
                ->caption(true)
                ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
                ->build();
            row->add_view(dur, LayoutParams(WRAP_CONTENT, WRAP_CONTENT));
        }

        list_card->add_view(row);
    }

    if (items.size() > max_preview_items) {
        auto more_label = TextViewBuilder::create()
            ->text("+ " + std::to_string(items.size() - max_preview_items) + " more tracks...")
            ->caption(true)
            ->textColor(config->colors.primary)
            ->padding(4, 2)
            ->build();
        list_card->add_view(more_label);
    }

    add_preview_card(list_card);

    // 3. Selection Shortcuts Card
    auto hint_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(14, 12)
        ->spacing(4)
        ->backgroundColor(config->colors.surface_variant.with_alpha(0.35f))
        ->stroke(1, config->colors.outline.with_alpha(0.2f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto hint_title = TextViewBuilder::create()
        ->text("Selection Shortcuts")
        ->bold(true)
        ->textSize(static_cast<int>(config->metrics.font_size * 0.9f))
        ->textColor(config->colors.on_surface_variant)
        ->build();
    hint_card->add_view(hint_title);

    auto hint_text = TextViewBuilder::create()
        ->text("Space: Toggle track  |  Shift+↑/↓: Range\nCtrl+A: Select All  |  Esc: Cancel")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.75f))
        ->build();
    hint_card->add_view(hint_text);

    add_preview_card(hint_card);
}

} // namespace miqu
