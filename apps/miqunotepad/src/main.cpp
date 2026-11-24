#include "notepad_app.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;
using namespace miqu;

int main(int argc, char* argv[]) {
    bool auto_exit = false;
    std::string file_to_open;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "miqunotepad — Native, Lightweight Text Editor\n\n"
                      << "Usage: miqunotepad [OPTIONS] [FILE]\n\n"
                      << "Options:\n"
                      << "  --init-config    Safely generate user configuration file (~/.config/miqunotepad/miqunotepad.conf)\n"
                      << "  --auto-exit      Initialize UI components and exit immediately without entering event loop\n"
                      << "  -h, --help       Display this help message and exit\n\n"
                      << "Keybindings:\n"
                      << "  [Ctrl+N]         New document\n"
                      << "  [Ctrl+O]         Open document prompt\n"
                      << "  [Ctrl+S]         Save document\n"
                      << "  [Ctrl+Shift+S]   Save As prompt\n"
                      << "  [Ctrl+Q]         Quit miqunotepad\n"
                      << "  [Esc]            Cancel open/save prompt\n"
                      << "  [Ctrl+W]         Delete previous word\n"
                      << "  [Ctrl+U]         Clear text\n";
            return 0;
        } else if (arg == "--init-config") {
            std::string path = FsUtils::init_user_config("miqunotepad");
            std::cout << "[miqunotepad] Configuration initialized at: " << path << "\n";
            return 0;
        } else if (arg == "--auto-exit") {
            auto_exit = true;
        } else if (!arg.empty() && arg[0] != '-') {
            file_to_open = arg;
        }
    }

    // 1. Resolve canonical configuration path (Tier 2 user authority > Tier 1 system baseline)
    std::string user_conf = FsUtils::get_user_config_file("miqunotepad");
    std::string target_conf = fs::exists(user_conf) ? user_conf : FsUtils::get_system_config_file("miqunotepad");

    // 2. Load exclusive configuration file
    Config::get()->load_from_file(target_conf);

    // 3. Initialize AppEngine
    auto engine = AppEngine::create();
    if (!engine) {
        std::cerr << "[miqunotepad] Failed to initialize AppEngine.\n";
        return 1;
    }

    // 4. Register inotify watcher on primary config and all sourced files for live reload
    engine->setup_config_watcher();

    // 5. Initialize application
    NotepadApp app(engine, file_to_open);
    if (!app.init()) {
        std::cerr << "[miqunotepad] Failed to initialize NotepadApp.\n";
        return 1;
    }

    // 6. Connect live theme changes
    engine->add_theme_change_listener([&app]() {
        app.sync_theme();
    });

    if (auto_exit) {
        return 0;
    }

    return engine->enter_loop();
}
