#include "player_card_view.hpp"
#include "art_loader.hpp"
#include <iostream>

using namespace miqu;

constexpr int MATCH_PARENT = static_cast<int>(LayoutDimension::MatchParent);
constexpr int WRAP_CONTENT = static_cast<int>(LayoutDimension::WrapContent);

namespace miqumusic {

PlayerCardView::PlayerCardView(std::shared_ptr<MpdClient> client)
    : LinearLayout(Orientation::Vertical), m_client(std::move(client)) {
    setup_ui();
}

void PlayerCardView::setup_ui() {
    auto config = Config::get();

    set_gravity(Gravity::CenterHorizontal);
    set_divider_spacing(8);
    set_padding(20, 12, 20, 16);

    // 1. Album Artwork Container
    m_art_image = ImageViewBuilder::create()
        ->fitMode(FitMode::Cover)
        ->cornerRadius(14)
        ->build();
    m_art_image->set_layout_params(LayoutParams(220, 220));

    m_art_card = CardViewBuilder::create()
        ->cornerRadius(14)
        ->backgroundColor(config->colors.surface_variant.with_alpha(0.5f))
        ->elevation(0)
        ->addView(m_art_image)
        ->build();
    m_art_card->set_focusable(false);

    LayoutParams art_lp(220, 220, Gravity::CenterHorizontal);
    add_view(m_art_card, art_lp);

    // 2. Track Title, Artist & Album
    m_title_label = TextViewBuilder::create()
        ->text("No Track Playing")
        ->textSize(config->metrics.font_size + 4)
        ->bold(true)
        ->textColor(config->colors.on_surface)
        ->textAlignment(TextAlignment::Center)
        ->ellipsize(true)
        ->build();
    add_view(m_title_label, LayoutParams(MATCH_PARENT, WRAP_CONTENT, Gravity::CenterHorizontal));

    m_artist_label = TextViewBuilder::create()
        ->text("Connect to MPD to start listening")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.primary)
        ->textAlignment(TextAlignment::Center)
        ->ellipsize(true)
        ->build();
    add_view(m_artist_label, LayoutParams(MATCH_PARENT, WRAP_CONTENT, Gravity::CenterHorizontal));

    m_album_label = TextViewBuilder::create()
        ->text("")
        ->textSize(config->metrics.font_size - 1)
        ->textColor(config->colors.on_surface_variant)
        ->textAlignment(TextAlignment::Center)
        ->ellipsize(true)
        ->build();
    add_view(m_album_label, LayoutParams(MATCH_PARENT, WRAP_CONTENT, Gravity::CenterHorizontal));

    // Audio format pill badge
    m_format_badge = TextViewBuilder::create()
        ->text("FLAC • 24-bit / 96kHz")
        ->textSize(config->metrics.font_size - 2)
        ->textColor(config->colors.primary)
        ->textAlignment(TextAlignment::Center)
        ->build();
    m_format_badge->set_visibility(Visibility::Gone);

    LayoutParams badge_lp(WRAP_CONTENT, WRAP_CONTENT, Gravity::CenterHorizontal);
    add_view(m_format_badge, badge_lp);

    // 3. Progress Scrubber Row (Elapsed - Slider - Total)
    auto scrubber_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    scrubber_row->set_gravity(Gravity::CenterVertical);
    scrubber_row->set_divider_spacing(8);

    m_elapsed_label = TextViewBuilder::create()
        ->text("0:00")
        ->textSize(config->metrics.font_size - 1)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    scrubber_row->add_view(m_elapsed_label, LayoutParams(42, WRAP_CONTENT, Gravity::CenterVertical));

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
    scrubber_row->add_view(m_seek_slider, LayoutParams(MATCH_PARENT, 20, 1.0f));

    m_total_label = TextViewBuilder::create()
        ->text("0:00")
        ->textSize(config->metrics.font_size - 1)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    scrubber_row->add_view(m_total_label, LayoutParams(42, WRAP_CONTENT, Gravity::CenterVertical));

    add_view(scrubber_row, LayoutParams(MATCH_PARENT, 26));

    // 4. Media Transport Controls Row (Shuffle | Prev | Play/Pause | Next | Repeat)
    auto transport_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    transport_row->set_gravity(Gravity::Center);
    transport_row->set_divider_spacing(14);

    m_btn_shuffle = ButtonBuilder::create()
        ->text("🔀")
        ->flat(true)
        ->cornerRadius(6)
        ->padding(6, 4)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    m_btn_shuffle->set_focusable(false);
    m_btn_shuffle->set_on_click_listener([this]() {
        if (m_client) {
            m_client->set_random(!m_last_status.random);
        }
    });
    transport_row->add_view(m_btn_shuffle, LayoutParams(34, 34, Gravity::CenterVertical));

    m_btn_prev = ButtonBuilder::create()
        ->text("⏮")
        ->flat(true)
        ->cornerRadius(6)
        ->padding(6, 4)
        ->textSize(config->metrics.font_size + 2)
        ->textColor(config->colors.on_surface)
        ->build();
    m_btn_prev->set_focusable(false);
    m_btn_prev->set_on_click_listener([this]() {
        if (m_client) {
            m_client->previous();
        }
    });
    transport_row->add_view(m_btn_prev, LayoutParams(34, 34, Gravity::CenterVertical));

    m_btn_play_pause = ButtonBuilder::create()
        ->text("▶")
        ->flat(true)
        ->cornerRadius(22)
        ->padding(10, 6)
        ->textSize(config->metrics.font_size + 4)
        ->textColor(config->colors.primary)
        ->build();
    m_btn_play_pause->set_focusable(false);
    m_btn_play_pause->set_background_color(config->colors.primary.with_alpha(0.18f));
    m_btn_play_pause->set_on_click_listener([this]() {
        if (m_client) {
            m_client->toggle_pause();
        }
    });
    transport_row->add_view(m_btn_play_pause, LayoutParams(44, 44, Gravity::CenterVertical));

    m_btn_next = ButtonBuilder::create()
        ->text("⏭")
        ->flat(true)
        ->cornerRadius(6)
        ->padding(6, 4)
        ->textSize(config->metrics.font_size + 2)
        ->textColor(config->colors.on_surface)
        ->build();
    m_btn_next->set_focusable(false);
    m_btn_next->set_on_click_listener([this]() {
        if (m_client) {
            m_client->next();
        }
    });
    transport_row->add_view(m_btn_next, LayoutParams(34, 34, Gravity::CenterVertical));

    m_btn_repeat = ButtonBuilder::create()
        ->text("🔁")
        ->flat(true)
        ->cornerRadius(6)
        ->padding(6, 4)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    m_btn_repeat->set_focusable(false);
    m_btn_repeat->set_on_click_listener([this]() {
        if (m_client) {
            m_client->set_repeat(!m_last_status.repeat);
        }
    });
    transport_row->add_view(m_btn_repeat, LayoutParams(34, 34, Gravity::CenterVertical));

    add_view(transport_row, LayoutParams(MATCH_PARENT, 48));

    // 5. Volume Slider Row
    auto vol_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    vol_row->set_gravity(Gravity::CenterVertical);
    vol_row->set_divider_spacing(8);

    auto vol_icon = TextViewBuilder::create()
        ->text("󰕾")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    vol_row->add_view(vol_icon, LayoutParams(20, WRAP_CONTENT, Gravity::CenterVertical));

    m_vol_slider = SliderBuilder::create()
        ->progress(0.8f)
        ->progressColor(config->colors.primary)
        ->onValueChanged([this](float val, bool from_user) {
            if (from_user && m_client) {
                m_client->set_volume(static_cast<int>(val * 100.0f));
            }
        })
        ->build();
    m_vol_slider->set_focusable(false);
    vol_row->add_view(m_vol_slider, LayoutParams(MATCH_PARENT, 18, 1.0f));

    add_view(vol_row, LayoutParams(MATCH_PARENT, 24));

    // 6. Mini Audio Spectrum Visualizer
    m_visualizer = std::make_shared<VisualizerView>();
    add_view(m_visualizer, LayoutParams(MATCH_PARENT, 38));
}

void PlayerCardView::update_state(const MpdStatus& status, const Song& song) {
    m_last_status = status;
    m_last_song = song;

    if (m_title_label) {
        m_title_label->set_text(song.display_title());
    }
    if (m_artist_label) {
        m_artist_label->set_text(song.display_artist());
    }
    if (m_album_label) {
        m_album_label->set_text(song.album.empty() ? "" : (song.date.empty() ? song.album : song.album + " (" + song.date + ")"));
    }

    if (m_format_badge) {
        if (!status.audio_format.empty()) {
            m_format_badge->set_text(status.audio_format);
            m_format_badge->set_visibility(Visibility::Visible);
        } else {
            m_format_badge->set_visibility(Visibility::Gone);
        }
    }

    if (m_elapsed_label) {
        m_elapsed_label->set_text(status.formatted_elapsed());
    }
    if (m_total_label) {
        m_total_label->set_text(status.formatted_total());
    }

    if (m_seek_slider && !m_user_seeking.load()) {
        m_seek_slider->set_value(status.elapsed_fraction);
    }

    if (m_btn_play_pause) {
        bool is_playing = (status.state == PlaybackState::Playing);
        m_btn_play_pause->set_text(is_playing ? "⏸" : "▶");
    }

    if (m_vol_slider && status.volume >= 0) {
        m_vol_slider->set_value(static_cast<float>(status.volume) / 100.0f);
    }

    auto config = Config::get();
    if (m_btn_shuffle) {
        m_btn_shuffle->set_text_color(status.random ? config->colors.primary : config->colors.on_surface_variant);
    }
    if (m_btn_repeat) {
        m_btn_repeat->set_text_color(status.repeat ? config->colors.primary : config->colors.on_surface_variant);
    }

    if (m_visualizer) {
        m_visualizer->set_playing(status.state == PlaybackState::Playing);
    }

    update_cover_art(song);
    request_redraw();
}

void PlayerCardView::update_cover_art(const Song& song) {
    if (song.uri == m_last_art_uri && !m_last_art_uri.empty()) {
        return;
    }
    m_last_art_uri = song.uri;

    if (!m_client || song.uri.empty()) {
        if (m_art_image) m_art_image->set_image_resource("");
        return;
    }

    std::string art_path = ArtLoader::get_art_for_song(song, *m_client);
    if (m_art_image) {
        m_art_image->set_image_resource(art_path);
    }
}

void PlayerCardView::tick(double delta_time) {
    if (m_visualizer) {
        m_visualizer->tick(delta_time);
    }
}

void PlayerCardView::sync_theme() {
    auto config = Config::get();
    if (m_title_label) m_title_label->set_text_color(config->colors.on_surface);
    if (m_artist_label) m_artist_label->set_text_color(config->colors.primary);
    if (m_album_label) m_album_label->set_text_color(config->colors.on_surface_variant);
    if (m_elapsed_label) m_elapsed_label->set_text_color(config->colors.on_surface_variant);
    if (m_total_label) m_total_label->set_text_color(config->colors.on_surface_variant);

    if (m_seek_slider) m_seek_slider->set_progress_color(config->colors.primary);
    if (m_vol_slider) m_vol_slider->set_progress_color(config->colors.primary);
    if (m_btn_play_pause) {
        m_btn_play_pause->set_text_color(config->colors.primary);
        m_btn_play_pause->set_background_color(config->colors.primary.with_alpha(0.18f));
    }
    if (m_art_card) {
        m_art_card->set_background_color(config->colors.surface_variant.with_alpha(0.5f));
    }
    request_redraw();
}

} // namespace miqumusic
