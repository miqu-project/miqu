#include "files_app.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;
using namespace miqu;

int main(int argc, char* argv[]) {
    bool auto_exit = false;
    std::string start_path;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "miqufiles — Fast Miller Columns File Manager for Wayland\n\n"
                      << "Usage: miqufiles [OPTIONS] [PATH]\n\n"
                      << "Arguments:\n"
                      << "  [PATH]                  Directory path to browse on startup\n\n"
                      << "Options:\n"
                      << "  --init-config           Safely generate user configuration file (~/.config/miqufiles/miqufiles.conf)\n"
                      << "  --auto-exit             Initialize UI components and exit immediately without entering event loop\n"
                      << "  -h, --help              Display this help message and exit\n\n"
                      << "Keybindings:\n"
                      << "  [Ctrl+F]                Focus search bar\n"
                      << "  [Ctrl+H / .]            Toggle show/hide hidden files\n"
                      << "  [Space]                 Toggle item selection & step down (sporadic)\n"
                      << "  [Shift+↑/↓]             Range selection\n"
                      << "  [Ctrl+A]                Select all items in folder\n"
                      << "  [Shift/Ctrl + Click]    Range / sporadic mouse selection\n"
                      << "  [Esc]                   Cancel multi-selection / back / quit at root\n"
                      << "  [Left / Backspace / h]  Navigate back to parent folder\n"
                      << "  [Right / Enter / l]     Open directory or activate file preview\n"
                      << "  [Up / Down / k / j]     Navigate up/down in active list\n"
                      << "  [Tab / Shift+Tab]       Cycle preview controls\n"
                      << "  [F11]                   Toggle full-width / standard 3-column view\n";
            return 0;
        } else if (arg == "--init-config") {
            std::string path = FsUtils::init_user_config("miqufiles");
            std::cout << "[miqufiles] Configuration initialized at: " << path << "\n";
            return 0;
        } else if (arg == "--auto-exit") {
            auto_exit = true;
        } else if (!arg.empty() && arg[0] != '-') {
            start_path = arg;
        }
    }

    // 1. Resolve canonical configuration path
    std::string user_conf = FsUtils::get_user_config_file("miqufiles");
    std::string target_conf = fs::exists(user_conf) ? user_conf : FsUtils::get_system_config_file("miqufiles");

    // 2. Load exclusive configuration file
    Config::get()->load_from_file(target_conf);

    // 3. Initialize AppEngine
    auto engine = AppEngine::create();
    if (!engine) {
        std::cerr << "[miqufiles] Failed to initialize AppEngine.\n";
        return 1;
    }

    // 4. Setup inotify config watcher for live reload
    engine->setup_config_watcher();

    // 5. Initialize application
    FilesApp app(engine, start_path);
    if (!app.init()) {
        std::cerr << "[miqufiles] Failed to initialize FilesApp.\n";
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
