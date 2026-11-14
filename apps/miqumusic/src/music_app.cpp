#include "music_app.hpp"
#include <xkbcommon/xkbcommon-keysyms.h>
#include <iostream>
#include <chrono>

using namespace miqu;

namespace miqumusic {

MusicApp::MusicApp(std::shared_ptr<AppEngine> engine, std::string host, unsigned int port)
    : m_engine(std::move(engine)), m_host(std::move(host)), m_port(port) {
    m_client = std::make_shared<MpdClient>();
}

MusicApp::~MusicApp() {
    quit();
}

void MusicApp::quit() {
    stop_ticker();
    if (m_client) {
        m_client->stop_idle_listener();
        m_client->disconnect();
    }
    if (m_engine) {
        m_engine->quit(0);
    }
}

int MusicApp::run() {
    return m_engine ? m_engine->enter_loop() : 0;
}

bool MusicApp::init() {
    if (!m_engine) return false;

    setup_ui();

    m_window = WindowBuilder::create()
        ->title("Music")
        ->appId("miqumusic")
        ->role(WindowRole::Toplevel)
        ->preferredSize(1020, 660)
        ->closeOnEscape(false)
        ->keyboardInteractive(true)
        ->contentView(m_root_layout)
        ->onClose([this]() {
            quit();
        })
        ->onKey([this](const KeyPressEvent& ev) {
            handle_key_press(ev);
        })
        ->build();

    if (!m_window) {
        std::cerr << "[miqumusic] Failed to create main window.\n";
        return false;
    }

    m_engine->register_window(m_window);
    sync_theme();
    update_header();

    setup_mpd_callbacks();

    // Connect to MPD asynchronously on background thread
    m_engine->post([this]() {
        bool connected = m_client->connect(m_host, m_port);
        if (!connected) {
            std::cerr << "[miqumusic] Warning: Could not connect to MPD on startup. Retrying in background.\n";
        } else {
            MpdStatus st = m_client->get_status();
            Song song = m_client->get_current_song();
            std::vector<Song> q = m_client->get_queue();
            if (m_miller_view) {
                m_miller_view->refresh_library();
                m_miller_view->update_playback(st, song);
                m_miller_view->update_queue(q);
            }
        }

        m_client->start_idle_listener();
        start_ticker();
    });

    return true;
}

void MusicApp::setup_ui() {
    auto config = Config::get();

    m_root_layout = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(0)
        ->backgroundColor(Color::transparent())
        ->build();

    // 1. Top Toolbar (Centered Title "Music", MPD status subtitle, close button)
    m_toolbar = ToolbarBuilder::create()
        ->title("Music")
        ->subtitle("MPD • Connecting...")
        ->titleAlignment(TitleAlignment::Center)
        ->showDivider(false)
        ->onClose([this]() {
            quit();
        })
        ->closeTooltip("Quit Music")
        ->build();

    LayoutParams toolbar_lp;
    toolbar_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    toolbar_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    m_root_layout->add_view(m_toolbar, toolbar_lp);

    // 2. Ribbon Header (Back button + Clickable Breadcrumbs + Search bar)
    m_ribbon = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(16, 0)
        ->spacing(12)
        ->backgroundColor(Color::transparent())
        ->build();

    m_back_label = TextViewBuilder::create()
        ->text("‹ Back")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.primary)
        ->build();

    auto back_content = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(10, 3)
        ->spacing(4)
        ->addView(m_back_label)
        ->build();

    m_back_btn = CardViewBuilder::create()
        ->style(CardStyle::Filled)
        ->backgroundColor(config->colors.surface_variant.with_alpha(0.35f))
        ->elevation(0)
        ->addView(back_content)
        ->build();

    m_back_btn->set_on_click_listener([this]() {
        if (m_miller_view && m_miller_view->can_navigate_back()) {
            if (m_miller_view->is_searching()) {
                if (m_search_view) {
                    m_search_view->set_query("");
                    m_search_view->set_focused(false);
                }
                if (m_window) m_window->clear_focus();
            }
            m_miller_view->navigate_back();
        }
    });

    LayoutParams back_lp;
    back_lp.width = static_cast<int>(LayoutDimension::WrapContent);
    back_lp.height = 26;
    back_lp.gravity = Gravity::CenterVertical;
    m_ribbon->add_view(m_back_btn, back_lp);

    // Dynamic Clickable Breadcrumbs
    m_breadcrumb_container = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(2)
        ->gravity(Gravity::CenterVertical)
        ->build();

    LayoutParams breadcrumb_lp;
    breadcrumb_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    breadcrumb_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    breadcrumb_lp.weight = 1.0f;
    breadcrumb_lp.gravity = Gravity::CenterVertical;
    m_ribbon->add_view(m_breadcrumb_container, breadcrumb_lp);

    // Instant Search Bar
    m_search_view = SearchViewBuilder::create()
        ->hint("Search library...")
        ->focused(false)
        ->focusBorder(false)
        ->padding(10, 2)
        ->elevation(0)
        ->backgroundColor(config->colors.surface_variant.with_alpha(0.35f))
        ->stroke(0, Color::transparent())
        ->onQueryTextListener(
            [this](const std::string& query) {
                if (m_miller_view) {
                    m_miller_view->set_search_query(query);
                }
            },
            [this](const std::string&) {
                if (m_search_view) {
                    m_search_view->set_focused(false);
                }
                if (m_window) {
                    m_window->clear_focus();
                }
                if (m_miller_view) {
                    m_miller_view->navigate_enter();
                }
            }
        )
        ->build();
    m_search_view->set_focused(false);

    LayoutParams search_lp;
    search_lp.width = 240;
    search_lp.height = 28;
    search_lp.gravity = Gravity::CenterVertical;
    m_ribbon->add_view(m_search_view, search_lp);

    LayoutParams ribbon_lp;
    ribbon_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    ribbon_lp.height = 36;
    m_root_layout->add_view(m_ribbon, ribbon_lp);

    // 3. Middle Body: MusicMillerView (3 Columns)
    m_miller_view = std::make_shared<MusicMillerView>(m_client);
    m_miller_view->init();
    m_miller_view->set_on_state_changed([this]() {
        update_header();
    });

    LayoutParams miller_lp;
    miller_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    miller_lp.height = static_cast<int>(LayoutDimension::MatchParent);
    miller_lp.weight = 1.0f;
    m_root_layout->add_view(m_miller_view, miller_lp);
}

void MusicApp::update_header() {
    if (!m_miller_view) return;

    if (m_breadcrumb_container) {
        m_breadcrumb_container->clear_views();
        auto config = Config::get();

        auto add_separator = [this, config]() {
            auto sep = TextViewBuilder::create()
                ->text("›")
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.on_surface_variant.with_alpha(0.45f))
                ->build();
            LayoutParams sep_lp;
            sep_lp.width = static_cast<int>(LayoutDimension::WrapContent);
            sep_lp.height = static_cast<int>(LayoutDimension::WrapContent);
            sep_lp.gravity = Gravity::CenterVertical;
            m_breadcrumb_container->add_view(sep, sep_lp);
        };

        auto add_crumb = [this, config](const std::string& label, std::function<void()> on_click, bool is_current) {
            LayoutParams lp;
            lp.width = static_cast<int>(LayoutDimension::WrapContent);
            lp.height = static_cast<int>(LayoutDimension::WrapContent);
            lp.gravity = Gravity::CenterVertical;

            if (is_current) {
                auto crumb_view = TextViewBuilder::create()
                    ->text(label)
                    ->textSize(config->metrics.font_size)
                    ->textColor(config->colors.on_surface)
                    ->build();
                m_breadcrumb_container->add_view(crumb_view, lp);
            } else {
                auto crumb_btn = ButtonBuilder::create()
                    ->text(label)
                    ->flat(true)
                    ->padding(4, 2)
                    ->cornerRadius(5)
                    ->textSize(config->metrics.font_size)
                    ->textColor(config->colors.on_surface_variant)
                    ->build();
                crumb_btn->set_focusable(false);
                if (on_click) {
                    crumb_btn->set_on_click_listener(std::move(on_click));
                }
                m_breadcrumb_container->add_view(crumb_btn, lp);
            }
        };

        if (m_miller_view->is_searching()) {
            add_crumb("Music", [this]() {
                if (m_search_view) {
                    m_search_view->set_query("");
                    m_search_view->set_focused(false);
                }
                if (m_miller_view) {
                    m_miller_view->navigate_to_depth(0);
                }
            }, false);

            add_separator();
            add_crumb("Search: \"" + m_miller_view->get_search_query() + "\"", nullptr, true);
        } else {
            const auto& path = m_miller_view->get_path();
            std::string leaf_title = m_miller_view->get_current_leaf_title();

            add_crumb("Music", [this]() {
                if (m_miller_view) {
                    m_miller_view->navigate_to_depth(0);
                }
            }, path.empty() && leaf_title == "Music");

            for (size_t i = 0; i < path.size(); ++i) {
                add_separator();
                size_t target_depth = i + 1;
                add_crumb(path[i].title, [this, target_depth]() {
                    if (m_miller_view) {
                        m_miller_view->navigate_to_depth(target_depth);
                    }
                }, false);
            }

            if (!leaf_title.empty() && (path.empty() || leaf_title != path.back().title)) {
                add_separator();
                add_crumb(leaf_title, nullptr, true);
            }
        }
    }

    if (m_back_btn) {
        m_back_btn->set_visibility(m_miller_view->can_navigate_back() ? Visibility::Visible : Visibility::Gone);
    }

    if (m_search_view && !m_miller_view->is_searching() && !m_search_view->get_query().empty()) {
        m_search_view->set_query("");
    }

    if (m_window) {
        m_window->schedule_redraw();
    }
}

void MusicApp::setup_mpd_callbacks() {
    m_client->set_on_status_changed([this](const MpdStatus& st, const Song& song) {
        if (m_engine) {
            m_engine->post([this, st, song]() {
                {
                    std::lock_guard<std::mutex> lock(m_status_mutex);
                    m_status = st;
                    m_current_song = song;
                }
                if (m_miller_view) {
                    m_miller_view->update_playback(st, song);
                }
                if (m_toolbar) {
                    std::string state_str = (st.state == PlaybackState::Playing) ? "Playing" :
                                            ((st.state == PlaybackState::Paused) ? "Paused" : "Stopped");
                    m_toolbar->set_subtitle("MPD • " + state_str);
                }
                if (m_window) m_window->schedule_redraw();
            });
        }
    });

    m_client->set_on_queue_changed([this](const std::vector<Song>& queue) {
        if (m_engine) {
            m_engine->post([this, queue]() {
                if (m_miller_view) {
                    m_miller_view->update_queue(queue);
                }
                if (m_window) m_window->schedule_redraw();
            });
        }
    });

    m_client->set_on_connection_changed([this](bool connected) {
        if (m_engine) {
            m_engine->post([this, connected]() {
                if (m_toolbar) {
                    m_toolbar->set_subtitle(connected ? "MPD • Connected" : "MPD • Offline");
                }
                if (connected && m_miller_view) {
                    m_miller_view->refresh_library();
                }
                if (m_window) m_window->schedule_redraw();
            });
        }
    });
}

void MusicApp::start_ticker() {
    stop_ticker();
    m_running.store(true);

    m_ticker_thread = std::thread([this]() {
        auto last_time = std::chrono::steady_clock::now();
        while (m_running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30 fps
            auto now = std::chrono::steady_clock::now();
            double delta = std::chrono::duration<double>(now - last_time).count();
            last_time = now;

            if (m_engine && m_miller_view) {
                m_engine->post([this, delta]() {
                    if (m_miller_view) {
                        m_miller_view->tick(delta);
                    }
                    if (m_window) {
                        m_window->schedule_redraw();
                    }
                });
            }
        }
    });
}

void MusicApp::stop_ticker() {
    m_running.store(false);
    if (m_ticker_thread.joinable()) {
        m_ticker_thread.join();
    }
}

void MusicApp::handle_key_press(const KeyPressEvent& event) {
    if (!event.pressed || !m_miller_view) return;

    // 1. Global: Ctrl+F to focus search bar
    if (event.has_ctrl() && (event.keysym == XKB_KEY_f || event.keysym == XKB_KEY_F)) {
        if (m_search_view) {
            m_search_view->set_focused(true);
            if (m_window) m_window->schedule_redraw();
            return;
        }
    }

    // 2. F11: Toggle full-width immersion mode
    if (event.keysym == XKB_KEY_F11) {
        m_miller_view->set_preview_expanded(!m_miller_view->is_preview_expanded());
        return;
    }

    // 3. Search Mode active
    bool is_search_active = m_search_view && (m_search_view->is_focused() || m_search_view->has_focus() ||
                            (m_search_view->get_edit_text() && m_search_view->get_edit_text()->has_focus()));
    if (is_search_active) {
        if (event.keysym == XKB_KEY_Escape) {
            m_search_view->set_focused(false);
            m_search_view->set_query("");
            m_miller_view->set_search_query("");
            if (m_window) m_window->clear_focus();
            return;
        }
        if (event.keysym == XKB_KEY_Up) {
            m_miller_view->navigate_up();
            return;
        }
        if (event.keysym == XKB_KEY_Down) {
            m_miller_view->navigate_down();
            return;
        }
        if (event.keysym == XKB_KEY_Return || event.keysym == XKB_KEY_KP_Enter) {
            m_search_view->set_focused(false);
            if (m_window) m_window->clear_focus();
            m_miller_view->navigate_enter();
            return;
        }
        return;
    }

    // 4. Space: Toggle Play / Pause
    if (event.keysym == XKB_KEY_space) {
        if (m_client) {
            m_client->toggle_pause();
        }
        return;
    }

    // 5. Media & Navigation Keys
    switch (event.keysym) {
        case XKB_KEY_Up:
            m_miller_view->navigate_up();
            break;

        case XKB_KEY_Down:
            m_miller_view->navigate_down();
            break;

        case XKB_KEY_Right:
        case XKB_KEY_Return:
        case XKB_KEY_KP_Enter:
            m_miller_view->navigate_enter();
            break;

        case XKB_KEY_Left:
        case XKB_KEY_BackSpace:
            m_miller_view->navigate_back();
            break;

        case XKB_KEY_Escape:
            if (!m_miller_view->navigate_back()) {
                quit();
            }
            break;

        default:
            break;
    }
}

void MusicApp::sync_theme() {
    auto config = Config::get();
    m_root_layout->set_background_color(Color::transparent());

    if (m_ribbon) {
        m_ribbon->set_background_color(Color::transparent());
    }
    if (m_back_label) {
        m_back_label->set_text_color(config->colors.primary);
        m_back_label->set_text_size(config->metrics.font_size);
    }
    if (m_back_btn) {
        m_back_btn->set_card_background_color(config->colors.surface_variant.with_alpha(0.35f));
    }
    if (m_search_view) {
        m_search_view->set_background_color(config->colors.surface_variant.with_alpha(0.35f));
        m_search_view->set_stroke(0, Color::transparent());
    }

    if (m_miller_view) {
        m_miller_view->sync_theme();
    }

    update_header();
    if (m_window) {
        m_window->schedule_redraw();
    }
}

} // namespace miqumusic
