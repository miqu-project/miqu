#pragma once

#include "mpd_types.hpp"
#include "visualizer_view.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace miqumusic {
class MpdClient;
}

namespace miqu {

class MusicItem {
public:
    std::string id;
    std::string title;
    std::string subtitle;
    std::string icon;
    bool is_category = false;
    std::vector<std::shared_ptr<MusicItem>> children;
    std::function<void(MusicItem*)> provider = nullptr;

    // Song leaf data
    bool is_song = false;
    miqumusic::Song song_data;

    // Visualizer leaf data
    bool is_visualizer = false;
    miqumusic::VisualizerMode visualizer_mode = miqumusic::VisualizerMode::SpectrumBars;

    // Actions
    bool has_action = false;
    std::string action_label;
    std::string last_action_message;

    void ensure_children() {
        if (provider) {
            auto p = std::move(provider);
            provider = nullptr;
            p(this);
        }
    }
};

class MusicModel {
public:
    static std::vector<std::shared_ptr<MusicItem>> build_default_tree(std::shared_ptr<miqumusic::MpdClient> client);
};

} // namespace miqu
