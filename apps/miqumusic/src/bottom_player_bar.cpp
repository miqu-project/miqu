#include "bottom_player_bar.hpp"
#include "art_loader.hpp"
#include <iostream>

using namespace miqu;

namespace miqumusic {

constexpr int MATCH_PARENT = static_cast<int>(LayoutDimension::MatchParent);
constexpr int WRAP_CONTENT = static_cast<int>(LayoutDimension::WrapContent);

BottomPlayerBar::BottomPlayerBar(std::shared_ptr<MpdClient> client)
    : LinearLayout(Orientation::Horizontal), m_client(std::move(client)) {
    setup_ui();
}

void BottomPlayerBar::setup_ui() {
    auto config = Config::get();

    set_gravity(Gravity::CenterVertical);
    set_divider_spacing(16);
    set_padding(16, 6, 16, 8);
    set_background_color(config ? config->colors.surface_variant.with_alpha(0.25f) : Color::transparent());

    // 1. Left Section: 44x44 Thumbnail + Track Title & Artist
    auto left_sec = std::make_shared<LinearLayout>(Orientation::Horizontal);
    left_sec->set_gravity(Gravity::CenterVertical);
    left_sec->set_divider_spacing(10);

    m_art_image = ImageViewBuilder::create()
        ->fitMode(FitMode::Cover)
        ->cornerRadius(8)
        ->build();
    m_art_image->set_layout_params(LayoutParams(44, 44));

    m_art_card = CardViewBuilder::create()
        ->cornerRadius(8)
        ->backgroundColor(config->colors.surface_variant.with_alpha(0.5f))
        ->elevation(0)
        ->addView(m_art_image)
        ->build();
    m_art_card->set_focusable(false);
    left_sec->add_view(m_art_card, LayoutParams(44, 44, Gravity::CenterVertical));

    auto track_info_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    track_info_col->set_gravity(Gravity::CenterVertical);
    track_info_col->set_divider_spacing(2);

    m_title_label = TextViewBuilder::create()
        ->text("No Track Playing")
        ->textSize(config->metrics.font_size)
        ->bold(true)
        ->textColor(config->colors.on_surface)
        ->ellipsize(true)
        ->build();
    track_info_col->add_view(m_title_label, LayoutParams(MATCH_PARENT, WRAP_CONTENT));

    m_artist_label = TextViewBuilder::create()
        ->text("Idle")
        ->textSize(config->metrics.font_size - 2)
        ->textColor(config->colors.on_surface_variant)
        ->ellipsize(true)
        ->build();
    track_info_col->add_view(m_artist_label, LayoutParams(MATCH_PARENT, WRAP_CONTENT));

    left_sec->add_view(track_info_col, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));
    add_view(left_sec, LayoutParams(240, WRAP_CONTENT, Gravity::CenterVertical));

    // 2. Center Section: Transport Controls & Scrubber (Weight 1.0f)
    auto center_sec = std::make_shared<LinearLayout>(Orientation::Vertical);
    center_sec->set_gravity(Gravity::CenterHorizontal);
    center_sec->set_divider_spacing(2);

    // Transport buttons row
    auto btn_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    btn_row->set_gravity(Gravity::Center);
    btn_row->set_divider_spacing(12);

    m_btn_shuffle = ButtonBuilder::create()
        ->text("⇄")
        ->flat(true)
        ->padding(6, 4)
        ->cornerRadius(6)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface_variant)
        ->onClick([this]() {
            if (m_client) m_client->set_random(!m_last_status.random);
        })
        ->build();
    m_btn_shuffle->set_focusable(false);
    btn_row->add_view(m_btn_shuffle, LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity::CenterVertical));

    m_btn_prev = ButtonBuilder::create()
        ->text("⏮")
        ->flat(true)
        ->padding(6, 4)
        ->cornerRadius(6)
        ->textSize(config->metrics.font_size + 2)
        ->textColor(config->colors.on_surface)
        ->onClick([this]() {
            if (m_client) m_client->previous();
        })
        ->build();
    m_btn_prev->set_focusable(false);
    btn_row->add_view(m_btn_prev, LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity::CenterVertical));

    m_btn_play_pause = ButtonBuilder::create()
        ->text("▶")
        ->style(ButtonStyle::Primary)
        ->padding(12, 6)
        ->cornerRadius(16)
        ->textSize(config->metrics.font_size + 2)
        ->textColor(config->colors.on_primary)
        ->backgroundColor(config->colors.primary)
        ->onClick([this]() {
            if (m_client) m_client->toggle_pause();
        })
        ->build();
    m_btn_play_pause->set_focusable(false);
    btn_row->add_view(m_btn_play_pause, LayoutParams(WRAP_CONTENT, 32, Gravity::CenterVertical));

    m_btn_next = ButtonBuilder::create()
        ->text("⏭")
        ->flat(true)
        ->padding(6, 4)
        ->cornerRadius(6)
        ->textSize(config->metrics.font_size + 2)
        ->textColor(config->colors.on_surface)
        ->onClick([this]() {
            if (m_client) m_client->next();
        })
        ->build();
    m_btn_next->set_focusable(false);
    btn_row->add_view(m_btn_next, LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity::CenterVertical));

    m_btn_repeat = ButtonBuilder::create()
        ->text("↻")
        ->flat(true)
        ->padding(6, 4)
        ->cornerRadius(6)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface_variant)
        ->onClick([this]() {
            if (m_client) m_client->set_repeat(!m_last_status.repeat);
        })
        ->build();
    m_btn_repeat->set_focusable(false);
    btn_row->add_view(m_btn_repeat, LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity::CenterVertical));

    center_sec->add_view(btn_row, LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity::CenterHorizontal));

    // Scrubber row (Elapsed - Slider - Total)
    auto scrubber_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    scrubber_row->set_gravity(Gravity::CenterVertical);
    scrubber_row->set_divider_spacing(8);

    m_elapsed_label = TextViewBuilder::create()
        ->text("0:00")
        ->textSize(config->metrics.font_size - 2)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    scrubber_row->add_view(m_elapsed_label, LayoutParams(36, WRAP_CONTENT, Gravity::CenterVertical));

    m_seek_slider = SliderBuilder::create()
        ->progress(0.0f)
        ->progressColor(config->colors.primary)
        ->onValueChanged([this](float val, bool from_user) {
            if (from_user && m_client) {
                m_client->seek_fraction(val);
            }
        })
        ->build();
    m_seek_slider->set_focusable(false);
    LayoutParams slider_lp(MATCH_PARENT, 18, Gravity::CenterVertical);
    slider_lp.weight = 1.0f;
    scrubber_row->add_view(m_seek_slider, slider_lp);

    m_total_label = TextViewBuilder::create()
        ->text("0:00")
        ->textSize(config->metrics.font_size - 2)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    scrubber_row->add_view(m_total_label, LayoutParams(36, WRAP_CONTENT, Gravity::CenterVertical));

    center_sec->add_view(scrubber_row, LayoutParams(MATCH_PARENT, WRAP_CONTENT));

    LayoutParams center_lp(MATCH_PARENT, WRAP_CONTENT, Gravity::CenterVertical);
    center_lp.weight = 1.0f;
    add_view(center_sec, center_lp);

    // 3. Right Section: Volume Slider & Format Badge
    auto right_sec = std::make_shared<LinearLayout>(Orientation::Horizontal);
    right_sec->set_gravity(Gravity::CenterVertical | Gravity::Right);
    right_sec->set_divider_spacing(8);

    auto vol_icon = TextViewBuilder::create()
        ->text("󰕾")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    right_sec->add_view(vol_icon, LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity::CenterVertical));

    m_vol_slider = SliderBuilder::create()
        ->progress(0.7f)
        ->progressColor(config->colors.primary)
        ->onValueChanged([this](float val, bool from_user) {
            if (from_user && m_client) {
                m_client->set_volume(static_cast<int>(val * 100.0f));
            }
        })
        ->build();
    m_vol_slider->set_focusable(false);
    right_sec->add_view(m_vol_slider, LayoutParams(80, 18, Gravity::CenterVertical));

    m_format_badge = TextViewBuilder::create()
        ->text("MPD")
        ->textSize(config->metrics.font_size - 3)
        ->textColor(config->colors.primary)
        ->build();
    right_sec->add_view(m_format_badge, LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity::CenterVertical));

    add_view(right_sec, LayoutParams(200, WRAP_CONTENT, Gravity::CenterVertical));
}

void BottomPlayerBar::update_state(const MpdStatus& status, const Song& song) {
    m_last_status = status;
    m_last_song = song;

    if (m_title_label) {
        m_title_label->set_text(song.display_title());
    }
    if (m_artist_label) {
        m_artist_label->set_text(song.display_artist());
    }

    if (m_btn_play_pause) {
        m_btn_play_pause->set_text(status.state == PlaybackState::Playing ? "⏸" : "▶");
    }

    auto config = Config::get();
    if (m_btn_shuffle && config) {
        m_btn_shuffle->set_text_color(status.random ? config->colors.primary : config->colors.on_surface_variant);
    }
    if (m_btn_repeat && config) {
        m_btn_repeat->set_text_color(status.repeat ? config->colors.primary : config->colors.on_surface_variant);
    }

    if (m_elapsed_label) {
        m_elapsed_label->set_text(format_time(status.elapsed_seconds));
    }
    if (m_total_label) {
        m_total_label->set_text(song.formatted_duration());
    }

    if (m_seek_slider && !m_user_seeking.load()) {
        float frac = (status.total_seconds > 0) ? (static_cast<float>(status.elapsed_seconds) / status.total_seconds) : 0.0f;
        m_seek_slider->set_progress(frac);
    }

    if (m_vol_slider) {
        m_vol_slider->set_progress(status.volume / 100.0f);
    }

    if (m_format_badge) {
        if (!status.audio_format.empty()) {
            m_format_badge->set_text(status.audio_format);
            m_format_badge->set_visibility(Visibility::Visible);
        } else {
            m_format_badge->set_text("MPD");
        }
    }

    update_cover_art(song);
    request_redraw();
}

void BottomPlayerBar::update_cover_art(const Song& song) {
    if (song.uri.empty()) {
        if (m_art_image) m_art_image->set_image_resource("");
        m_last_art_uri.clear();
        return;
    }
    if (song.uri == m_last_art_uri) return;
    m_last_art_uri = song.uri;

    if (!m_client) return;
    std::string art_path = ArtLoader::get_art_for_song(song, *m_client);
    if (!art_path.empty() && m_art_image) {
        m_art_image->set_image_resource(art_path);
    } else if (m_art_image) {
        m_art_image->set_image_resource("");
    }
}

void BottomPlayerBar::sync_theme() {
    auto config = Config::get();
    if (!config) return;

    set_background_color(config->colors.surface_variant.with_alpha(0.25f));
    if (m_art_card) {
        m_art_card->set_card_background_color(config->colors.surface_variant.with_alpha(0.5f));
    }
    if (m_title_label) {
        m_title_label->set_text_color(config->colors.on_surface);
    }
    if (m_artist_label) {
        m_artist_label->set_text_color(config->colors.on_surface_variant);
    }
    if (m_btn_play_pause) {
        m_btn_play_pause->set_background_color(config->colors.primary);
        m_btn_play_pause->set_text_color(config->colors.on_primary);
    }
    if (m_seek_slider) {
        m_seek_slider->set_progress_color(config->colors.primary);
    }
    if (m_vol_slider) {
        m_vol_slider->set_progress_color(config->colors.primary);
    }
    request_redraw();
}

} // namespace miqumusic
