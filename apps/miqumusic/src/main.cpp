#include "music_app.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;
using namespace miqu;

int main(int argc, char* argv[]) {
    bool auto_exit = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "miqumusic — Desktop Music Player & Queue Browser\n\n"
                      << "Usage: miqumusic [OPTIONS]\n\n"
                      << "Options:\n"
                      << "  --init-config    Safely generate user configuration file (~/.config/miqumusic/miqumusic.conf)\n"
                      << "  --auto-exit      Initialize UI components and exit immediately without entering event loop\n"
                      << "  -h, --help       Display this help message and exit\n\n"
                      << "Keybindings:\n"
                      << "  [Ctrl+F]                Focus search bar\n"
                      << "  [Esc]                   Unfocus / back / clear search / quit at root\n"
                      << "  [Left / Backspace]      Navigate back to parent or leave detail form\n"
                      << "  [Right / Enter]         Enter category or focus detail form\n"
                      << "  [Up / Down]             Navigate up/down in category or search lists\n"
                      << "  [Tab / Shift+Tab]       Cycle controls inside detail form\n"
                      << "  [F11]                   Toggle full-width / standard 3-column view\n";
            return 0;
        } else if (arg == "--init-config") {
            std::string path = FsUtils::init_user_config("miqumusic");
            std::cout << "[miqumusic] Configuration initialized at: " << path << "\n";
            return 0;
        } else if (arg == "--auto-exit") {
            auto_exit = true;
        }
    }

    // 1. Resolve canonical configuration path
    std::string user_conf = FsUtils::get_user_config_file("miqumusic");
    std::string target_conf = fs::exists(user_conf) ? user_conf : FsUtils::get_system_config_file("miqumusic");

    // 2. Load exclusive configuration file
    Config::get()->load_from_file(target_conf);

    // 3. Initialize AppEngine
    auto engine = AppEngine::create();
    if (!engine) {
        std::cerr << "[miqumusic] Failed to initialize AppEngine.\n";
        return 1;
    }

    // 4. Setup inotify config watcher for live reload
    engine->setup_config_watcher();

    // 5. Initialize application
    MusicApp app(engine);
    if (!app.init()) {
        std::cerr << "[miqumusic] Failed to initialize MusicApp.\n";
        return 1;
    }

    // 6. Connect dynamic live theme updates
    engine->add_theme_change_listener([&app]() {
        app.sync_theme();
    });

    if (auto_exit) {
        return 0;
    }

    return engine->enter_loop();
}
