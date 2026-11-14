#include "music_model.hpp"
#include "mpd_client.hpp"
#include <map>
#include <algorithm>

namespace miqu {

std::vector<std::shared_ptr<MusicItem>> MusicModel::build_default_tree(std::shared_ptr<miqumusic::MpdClient> client) {
    std::vector<std::shared_ptr<MusicItem>> tree;

    // Helper to create a song leaf item
    auto make_song_leaf = [](const miqumusic::Song& song, const std::string& prefix = "song_") {
        auto item = std::make_shared<MusicItem>();
        item->id = prefix + (!song.uri.empty() ? song.uri : std::to_string(song.id));
        item->title = song.display_title();
        item->subtitle = song.display_artist();
        item->icon = "󰎆";
        item->is_category = false;
        item->is_song = true;
        item->song_data = song;
        return item;
    };

    // 1. Queue Category
    auto queue_cat = std::make_shared<MusicItem>();
    queue_cat->id = "cat_queue";
    queue_cat->title = "Queue";
    queue_cat->icon = "󰒺";
    queue_cat->is_category = true;
    queue_cat->provider = [client, make_song_leaf](MusicItem* self) {
        self->children.clear();
        if (!client) return;
        auto queue = client->get_queue();
        for (const auto& s : queue) {
            std::string prefix = "queue_" + std::to_string(s.pos) + "_";
            self->children.push_back(make_song_leaf(s, prefix));
        }
    };
    tree.push_back(queue_cat);

    // 2. Artists Category
    auto artists_cat = std::make_shared<MusicItem>();
    artists_cat->id = "cat_artists";
    artists_cat->title = "Artists";
    artists_cat->icon = "󰠃";
    artists_cat->is_category = true;
    artists_cat->provider = [client, make_song_leaf](MusicItem* self) {
        self->children.clear();
        if (!client) return;
        auto all_songs = client->get_all_songs();
        if (all_songs.empty()) {
            all_songs = client->get_queue();
        }

        std::map<std::string, std::vector<miqumusic::Song>> artist_map;
        for (const auto& s : all_songs) {
            artist_map[s.display_artist()].push_back(s);
        }

        for (const auto& [artist_name, songs] : artist_map) {
            auto artist_item = std::make_shared<MusicItem>();
            artist_item->id = "artist_" + artist_name;
            artist_item->title = artist_name;
            artist_item->icon = "󰠃";
            artist_item->is_category = true;
            artist_item->provider = [songs, make_song_leaf](MusicItem* a_self) {
                a_self->children.clear();
                // Sub-group by albums
                std::map<std::string, std::vector<miqumusic::Song>> album_map;
                for (const auto& s : songs) {
                    album_map[s.display_album()].push_back(s);
                }
                if (album_map.size() == 1) {
                    // Only one album or single songs, add songs directly
                    for (const auto& s : songs) {
                        a_self->children.push_back(make_song_leaf(s, "artist_song_"));
                    }
                } else {
                    for (const auto& [alb_name, alb_songs] : album_map) {
                        auto alb_item = std::make_shared<MusicItem>();
                        alb_item->id = "artist_alb_" + alb_name;
                        alb_item->title = alb_name;
                        alb_item->icon = "󰀥";
                        alb_item->is_category = true;
                        alb_item->provider = [alb_songs, make_song_leaf](MusicItem* alb_self) {
                            alb_self->children.clear();
                            for (const auto& s : alb_songs) {
                                alb_self->children.push_back(make_song_leaf(s, "alb_song_"));
                            }
                        };
                        a_self->children.push_back(alb_item);
                    }
                }
            };
            self->children.push_back(artist_item);
        }
    };
    tree.push_back(artists_cat);

    // 3. Albums Category
    auto albums_cat = std::make_shared<MusicItem>();
    albums_cat->id = "cat_albums";
    albums_cat->title = "Albums";
    albums_cat->icon = "󰀥";
    albums_cat->is_category = true;
    albums_cat->provider = [client, make_song_leaf](MusicItem* self) {
        self->children.clear();
        if (!client) return;
        auto all_songs = client->get_all_songs();
        if (all_songs.empty()) {
            all_songs = client->get_queue();
        }

        std::map<std::string, std::vector<miqumusic::Song>> album_map;
        for (const auto& s : all_songs) {
            album_map[s.display_album()].push_back(s);
        }

        for (const auto& [alb_name, songs] : album_map) {
            auto alb_item = std::make_shared<MusicItem>();
            alb_item->id = "album_" + alb_name;
            alb_item->title = alb_name;
            alb_item->icon = "󰀥";
            alb_item->is_category = true;
            alb_item->provider = [songs, make_song_leaf](MusicItem* alb_self) {
                alb_self->children.clear();
                for (const auto& s : songs) {
                    alb_self->children.push_back(make_song_leaf(s, "album_song_"));
                }
            };
            self->children.push_back(alb_item);
        }
    };
    tree.push_back(albums_cat);

    // 4. Playlists Category
    auto playlists_cat = std::make_shared<MusicItem>();
    playlists_cat->id = "cat_playlists";
    playlists_cat->title = "Playlists";
    playlists_cat->icon = "󰲸";
    playlists_cat->is_category = true;
    playlists_cat->provider = [client, make_song_leaf](MusicItem* self) {
        self->children.clear();
        if (!client) return;
        auto playlists = client->get_playlists();
        for (const auto& pl : playlists) {
            auto pl_item = std::make_shared<MusicItem>();
            pl_item->id = "playlist_" + pl;
            pl_item->title = pl;
            pl_item->icon = "󰲸";
            pl_item->is_category = true;
            pl_item->provider = [client, pl, make_song_leaf](MusicItem* pl_self) {
                pl_self->children.clear();
                if (!client) return;
                auto songs = client->get_playlist_songs(pl);
                for (const auto& s : songs) {
                    pl_self->children.push_back(make_song_leaf(s, "pl_song_"));
                }
            };
            self->children.push_back(pl_item);
        }
    };
    tree.push_back(playlists_cat);

    // 5. Library Category
    auto library_cat = std::make_shared<MusicItem>();
    library_cat->id = "cat_library";
    library_cat->title = "Library";
    library_cat->icon = "󰉋";
    library_cat->is_category = true;
    library_cat->provider = [client, make_song_leaf](MusicItem* self) {
        self->children.clear();
        if (!client) return;
        auto all_songs = client->get_all_songs();
        if (all_songs.empty()) {
            all_songs = client->get_queue();
        }
        for (const auto& s : all_songs) {
            self->children.push_back(make_song_leaf(s, "lib_"));
        }
    };
    tree.push_back(library_cat);

    // 6. Visualizer Category
    auto visualizer_cat = std::make_shared<MusicItem>();
    visualizer_cat->id = "cat_visualizer";
    visualizer_cat->title = "Visualizer";
    visualizer_cat->icon = "󰓃";
    visualizer_cat->is_category = true;

    auto vis_spectrum = std::make_shared<MusicItem>();
    vis_spectrum->id = "vis_spectrum";
    vis_spectrum->title = "Frequency Spectrum";
    vis_spectrum->subtitle = "Real-time 36-band audio spectrum analyzer";
    vis_spectrum->icon = "󰓃";
    vis_spectrum->is_category = false;
    vis_spectrum->is_visualizer = true;
    vis_spectrum->visualizer_mode = miqumusic::VisualizerMode::SpectrumBars;

    auto vis_waveform = std::make_shared<MusicItem>();
    vis_waveform->id = "vis_waveform";
    vis_waveform->title = "Waveform Scope";
    vis_waveform->subtitle = "Oscilloscope waveform display";
    vis_waveform->icon = "󰓃";
    vis_waveform->is_category = false;
    vis_waveform->is_visualizer = true;
    vis_waveform->visualizer_mode = miqumusic::VisualizerMode::WaveformScope;

    auto vis_glow = std::make_shared<MusicItem>();
    vis_glow->id = "vis_glow";
    vis_glow->title = "Neon Peak Glow";
    vis_glow->subtitle = "Ambient peak frequency glow";
    vis_glow->icon = "󰓃";
    vis_glow->is_category = false;
    vis_glow->is_visualizer = true;
    vis_glow->visualizer_mode = miqumusic::VisualizerMode::GlowPeaks;

    visualizer_cat->children.push_back(vis_spectrum);
    visualizer_cat->children.push_back(vis_waveform);
    visualizer_cat->children.push_back(vis_glow);

    tree.push_back(visualizer_cat);

    return tree;
}

} // namespace miqu
