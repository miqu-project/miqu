#pragma once

#include "mpd_client.hpp"
#include "mpd_types.hpp"
#include "visualizer_view.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <memory>
#include <atomic>

namespace miqumusic {

class PlayerCardView : public miqu::LinearLayout {
public:
    explicit PlayerCardView(std::shared_ptr<MpdClient> client);
    ~PlayerCardView() override = default;

    void update_state(const MpdStatus& status, const Song& song);
    void tick(double delta_time);
    void sync_theme();

    void set_on_track_changed(std::function<void()> cb) { m_on_track_changed = std::move(cb); }

private:
    void setup_ui();
    void update_cover_art(const Song& song);

    std::shared_ptr<MpdClient> m_client;

    std::shared_ptr<miqu::CardView> m_art_card;
    std::shared_ptr<miqu::ImageView> m_art_image;
    std::shared_ptr<miqu::TextView> m_title_label;
    std::shared_ptr<miqu::TextView> m_artist_label;
    std::shared_ptr<miqu::TextView> m_album_label;
    std::shared_ptr<miqu::TextView> m_format_badge;

    std::shared_ptr<miqu::TextView> m_elapsed_label;
    std::shared_ptr<miqu::Slider> m_seek_slider;
    std::shared_ptr<miqu::TextView> m_total_label;

    std::shared_ptr<miqu::Button> m_btn_shuffle;
    std::shared_ptr<miqu::Button> m_btn_prev;
    std::shared_ptr<miqu::Button> m_btn_play_pause;
    std::shared_ptr<miqu::Button> m_btn_next;
    std::shared_ptr<miqu::Button> m_btn_repeat;

    std::shared_ptr<miqu::Slider> m_vol_slider;
    std::shared_ptr<VisualizerView> m_visualizer;

    std::string m_last_art_uri;
    std::atomic<bool> m_user_seeking{false};
    MpdStatus m_last_status;
    Song m_last_song;

    std::function<void()> m_on_track_changed;
};

} // namespace miqumusic
