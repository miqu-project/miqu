#pragma once

#include "mpd_client.hpp"
#include "mpd_types.hpp"
#include "music_miller_view.hpp"
#include "bottom_player_bar.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>

namespace miqumusic {

class MusicApp {
public:
    MusicApp(std::shared_ptr<miqu::AppEngine> engine, std::string host, unsigned int port);
    ~MusicApp();

    bool init();
    int run();
    void sync_theme();
    void quit();

private:
    void setup_ui();
    void setup_mpd_callbacks();
    void update_header();
    void start_ticker();
    void stop_ticker();

    void handle_key_press(const miqu::KeyPressEvent& ev);

    std::shared_ptr<miqu::AppEngine> m_engine;
    std::shared_ptr<miqu::Window> m_window;
    std::shared_ptr<MpdClient> m_client;
    std::string m_host;
    unsigned int m_port = 0;

    std::shared_ptr<miqu::LinearLayout> m_root_layout;
    std::shared_ptr<miqu::Toolbar> m_toolbar;
    std::shared_ptr<miqu::LinearLayout> m_ribbon;
    std::shared_ptr<miqu::CardView> m_back_btn;
    std::shared_ptr<miqu::TextView> m_back_label;
    std::shared_ptr<miqu::LinearLayout> m_breadcrumb_container;
    std::shared_ptr<miqu::SearchView> m_search_view;
    std::shared_ptr<MusicMillerView> m_miller_view;
    std::shared_ptr<BottomPlayerBar> m_player_bar;

    std::mutex m_status_mutex;
    MpdStatus m_status;
    Song m_current_song;

    std::atomic<bool> m_running{false};
    std::thread m_ticker_thread;
};

} // namespace miqumusic
