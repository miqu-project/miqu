#include "control_panel_app.hpp"
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
            std::cout << "miqucontrolpanel — Desktop Control Panel & Centralized System Settings Manager\n\n"
                      << "Usage: miqucontrolpanel [OPTIONS]\n\n"
                      << "Options:\n"
                      << "  --init-config    Safely generate user configuration file (~/.config/miqucontrolpanel/miqucontrolpanel.conf)\n"
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
            std::string path = FsUtils::init_user_config("miqucontrolpanel");
            std::cout << "[miqucontrolpanel] Configuration initialized at: " << path << "\n";
            return 0;
        } else if (arg == "--auto-exit") {
            auto_exit = true;
        }
    }

    // 1. Resolve canonical configuration path
    std::string user_conf = FsUtils::get_user_config_file("miqucontrolpanel");
    std::string target_conf = fs::exists(user_conf) ? user_conf : FsUtils::get_system_config_file("miqucontrolpanel");

    // 2. Load exclusive configuration file
    Config::get()->load_from_file(target_conf);

    // 3. Initialize AppEngine
    auto engine = AppEngine::create();
    if (!engine) {
        std::cerr << "[miqucontrolpanel] Failed to initialize AppEngine.\n";
        return 1;
    }

    // 4. Setup inotify config watcher for live reload
    engine->setup_config_watcher();

    // 5. Initialize application
    ControlPanelApp app(engine);
    if (!app.init()) {
        std::cerr << "[miqucontrolpanel] Failed to initialize ControlPanelApp.\n";
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
