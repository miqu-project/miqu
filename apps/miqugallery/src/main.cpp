#include "gallery_app.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;
using namespace miqu;

int main(int argc, char* argv[]) {
    bool auto_exit = false;
    std::string image_path;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "miqugallery — Full-Quality Wayland Image Viewer\n\n"
                      << "Usage: miqugallery [OPTIONS] [IMAGE_PATH]\n\n"
                      << "Options:\n"
                      << "  --auto-exit             Initialize UI and exit immediately\n"
                      << "  -h, --help              Display this help message and exit\n\n"
                      << "Keybindings:\n"
                      << "  [i]                     Toggle metadata overlay HUD\n";
            return 0;
        } else if (arg == "--auto-exit") {
            auto_exit = true;
        } else if (!arg.empty() && arg[0] != '-') {
            image_path = arg;
        }
    }

    std::string user_conf = FsUtils::get_user_config_file("miqugallery");
    std::string target_conf = fs::exists(user_conf) ? user_conf : FsUtils::get_system_config_file("miqugallery");
    Config::get()->load_from_file(target_conf);

    auto engine = AppEngine::create();
    if (!engine) {
        std::cerr << "[miqugallery] Failed to initialize AppEngine.\n";
        return 1;
    }

    engine->setup_config_watcher();

    GalleryApp app(engine, image_path);
    if (!app.init()) {
        std::cerr << "[miqugallery] Failed to initialize GalleryApp.\n";
        return 1;
    }

    engine->add_theme_change_listener([&app]() {
        app.sync_theme();
    });

    if (auto_exit) {
        return 0;
    }

    return engine->enter_loop();
}
