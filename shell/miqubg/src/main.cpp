#include "wallpaper_manager.hpp"
#include "ipc.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <csignal>
#include <iomanip>

using namespace miqubg;

static std::shared_ptr<miqu::AppEngine> g_engine;
static void sig_handler(int) {
    if (g_engine) {
        g_engine->quit(0);
    }
}

static std::string mode_to_string(miqu::FitMode mode) {
    switch (mode) {
        case miqu::FitMode::Contain: return "contain";
        case miqu::FitMode::Fill: return "stretch";
        case miqu::FitMode::Center: return "center";
        case miqu::FitMode::Tile: return "tile";
        case miqu::FitMode::Cover:
        default:
            return "cover";
    }
}

static std::string color_to_hex(const miqu::Color& c) {
    int r = static_cast<int>(c.r * 255.0f);
    int g = static_cast<int>(c.g * 255.0f);
    int b = static_cast<int>(c.b * 255.0f);
    std::ostringstream oss;
    oss << "#" << std::hex << std::setfill('0')
        << std::setw(2) << r
        << std::setw(2) << g
        << std::setw(2) << b;
    return oss.str();
}

static void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options...] [image_path]\n\n"
              << "Options:\n"
              << "  -i, --image <path>    Path to the wallpaper image (falls back to system asset wallpaper)\n"
              << "  -d, --default         Reset to system default asset wallpaper\n"
              << "  -o, --output <name>   Target specific output name (e.g. HDMI-A-1, eDP-1, or * for all)\n"
              << "  -m, --mode <mode>     Scaling mode: fill/cover, fit, stretch, center, tile (default: fill)\n"
              << "  -c, --color <#hex>    Solid background color in hex (e.g. #1e1e2e, default: #000000)\n"
              << "  -r, --reload          Reload wallpapers across all outputs\n"
              << "  -v, --version         Show version number and quit\n"
              << "  -h, --help            Show this help message\n\n"
              << "Scaling Modes:\n"
              << "  fill / cover   Scale to fill output, cropping if necessary (default)\n"
              << "  fit / contain  Scale to fit inside output, letterboxing with background color\n"
              << "  stretch        Stretch image to fill output (ignores aspect ratio)\n"
              << "  center         Center image at 1:1 scale without scaling\n"
              << "  tile           Tile the image repeatedly across output\n\n"
              << "Examples:\n"
              << "  " << prog << "\n"
              << "  " << prog << " -i /path/to/wallpaper.jpg\n"
              << "  " << prog << " /path/to/wallpaper.jpg\n"
              << "  " << prog << " --default\n"
              << "  " << prog << " -i /path/to/wallpaper.jpg -m fit -c '#1e1e2e'\n"
              << "  " << prog << " -i default.png -o HDMI-A-1 -i monitor.png -m stretch\n";
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, sig_handler);
    std::signal(SIGTERM, sig_handler);

    OutputConfig default_cfg;
    default_cfg.mode = miqu::FitMode::Cover;
    default_cfg.bg_color = miqu::Color::rgb(0.0f, 0.0f, 0.0f);

    std::map<std::string, OutputConfig> output_cfgs;
    std::string current_output = ""; // empty means modifying default_cfg
    bool reload_requested = false;
    bool explicit_image_set = false;
    bool default_requested = false;
    bool mode_set = false;
    bool color_set = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            std::cout << "miqubg version 1.1.0\n";
            return 0;
        } else if (arg == "-r" || arg == "--reload") {
            reload_requested = true;
        } else if (arg == "-d" || arg == "--default") {
            default_requested = true;
            std::string def_path = WallpaperManager::resolve_default_wallpaper();
            if (current_output.empty()) {
                default_cfg.image_path = def_path;
            } else {
                output_cfgs[current_output].image_path = def_path;
            }
        } else if (arg == "-i" || arg == "--image") {
            if (i + 1 < argc) {
                std::string path = argv[++i];
                if (current_output.empty()) {
                    default_cfg.image_path = path;
                } else {
                    output_cfgs[current_output].image_path = path;
                }
                explicit_image_set = true;
            } else {
                std::cerr << "Error: " << arg << " requires a file path\n";
                return 1;
            }
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                current_output = argv[++i];
                if (current_output == "*") {
                    current_output = "";
                } else if (output_cfgs.find(current_output) == output_cfgs.end()) {
                    output_cfgs[current_output] = default_cfg;
                }
            } else {
                std::cerr << "Error: " << arg << " requires an output name\n";
                return 1;
            }
        } else if (arg == "-m" || arg == "--mode") {
            if (i + 1 < argc) {
                miqu::FitMode mode = WallpaperManager::parse_mode(argv[++i]);
                if (current_output.empty()) {
                    default_cfg.mode = mode;
                } else {
                    output_cfgs[current_output].mode = mode;
                }
                mode_set = true;
            } else {
                std::cerr << "Error: " << arg << " requires a mode name\n";
                return 1;
            }
        } else if (arg == "-c" || arg == "--color") {
            if (i + 1 < argc) {
                miqu::Color col = miqu::Color::from_hex(argv[++i], miqu::Color::rgb(0.0f, 0.0f, 0.0f));
                if (current_output.empty()) {
                    default_cfg.bg_color = col;
                } else {
                    output_cfgs[current_output].bg_color = col;
                }
                color_set = true;
            } else {
                std::cerr << "Error: " << arg << " requires a color string (e.g. #000000)\n";
                return 1;
            }
        } else if (!arg.empty() && arg[0] != '-') {
            // Positional image path shorthand
            if (current_output.empty()) {
                default_cfg.image_path = arg;
            } else {
                output_cfgs[current_output].image_path = arg;
            }
            explicit_image_set = true;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    // If no wallpaper was explicitly specified, fall back to default asset wallpaper
    if (default_cfg.image_path.empty()) {
        default_cfg.image_path = WallpaperManager::resolve_default_wallpaper();
    }
    for (auto& pair : output_cfgs) {
        if (pair.second.image_path.empty()) {
            pair.second.image_path = default_cfg.image_path;
        }
    }

    // 1. Check if an active daemon is running to handle this request via IPC
    if (reload_requested) {
        std::string resp;
        if (IPC::send_command("RELOAD", resp)) {
            std::cout << "[miqubg] Daemon reloaded.\n";
            return 0;
        }
    }

    bool has_explicit_action = explicit_image_set || default_requested || !output_cfgs.empty() || mode_set || color_set;

    if (has_explicit_action) {
        std::string resp;
        bool sent_any = false;

        if (!output_cfgs.empty()) {
            for (const auto& pair : output_cfgs) {
                std::string cmd = "SET " + pair.first + " " + mode_to_string(pair.second.mode) + " "
                                  + color_to_hex(pair.second.bg_color) + " " + pair.second.image_path;
                if (IPC::send_command(cmd, resp)) {
                    sent_any = true;
                }
            }
        } else {
            std::string img = default_cfg.image_path;
            // If only mode or color was updated without image or default reset, preserve running daemon's image
            if (!explicit_image_set && !default_requested) {
                std::string get_resp;
                if (IPC::send_command("GET *", get_resp) && get_resp.rfind("OK ", 0) == 0) {
                    std::istringstream iss(get_resp.substr(3));
                    std::string cur_m, cur_c, cur_img;
                    if (iss >> cur_m >> cur_c) {
                        std::getline(iss, cur_img);
                        size_t f = cur_img.find_first_not_of(" \t");
                        if (f != std::string::npos) {
                            cur_img = cur_img.substr(f);
                            if (!cur_img.empty()) {
                                img = cur_img;
                            }
                        }
                    }
                }
            }

            std::string cmd = "SET * " + mode_to_string(default_cfg.mode) + " "
                              + color_to_hex(default_cfg.bg_color) + " " + img;
            if (IPC::send_command(cmd, resp)) {
                sent_any = true;
            }
        }

        if (sent_any) {
            std::cout << "[miqubg] Wallpaper updated via running daemon.\n";
            return 0;
        }
    } else {
        // No explicit wallpaper, mode, or output changes requested.
        // Check if an existing daemon is already running so we avoid trying to bind a duplicate daemon.
        std::string ping_resp;
        if (IPC::send_command("PING", ping_resp)) {
            std::cout << "[miqubg] Daemon is already running.\n"
                      << "  Use 'miqubg -i <path>' to set a new wallpaper,\n"
                      << "  'miqubg --default' to restore the default asset wallpaper, or\n"
                      << "  'miqubg --reload' to reload current outputs.\n";
            return 0;
        }
    }

    // 2. Start as the Persistent Daemon
    auto engine = miqu::AppEngine::create();
    if (!engine) {
        std::cerr << "[miqubg] Failed to create AppEngine. Is a Wayland compositor running?\n";
        return 1;
    }

    g_engine = engine;
    engine->set_quit_on_last_window_closed(false);

    // Perform an initial roundtrip to ensure registry globals (outputs) are announced
    wl_display_roundtrip(engine->get_display());

    WallpaperManager manager;
    manager.set_default_config(default_cfg);
    for (const auto& pair : output_cfgs) {
        manager.set_output_config(pair.first, pair.second);
    }

    manager.init(engine.get());

    // Initialize IPC Server
    IPCServer server([&manager, &engine](const std::string& cmd) -> std::string {
        if (cmd == "PING") {
            return "PONG";
        } else if (cmd.rfind("SET ", 0) == 0) {
            std::istringstream iss(cmd.substr(4));
            std::string output_name, mode_str, color_hex, image_path;
            if (iss >> output_name >> mode_str >> color_hex) {
                std::getline(iss, image_path);
                // Trim leading whitespace from image_path
                size_t first = image_path.find_first_not_of(" \t");
                if (first != std::string::npos) {
                    image_path = image_path.substr(first);
                } else {
                    image_path = "";
                }

                OutputConfig cfg;
                cfg.image_path = image_path;
                cfg.mode = WallpaperManager::parse_mode(mode_str);
                cfg.bg_color = miqu::Color::from_hex(color_hex, miqu::Color::rgb(0.0f, 0.0f, 0.0f));

                engine->post([&manager, output_name, cfg]() {
                    manager.update_wallpaper(output_name, cfg);
                });
                return "OK";
            }
            return "ERR Invalid SET arguments";
        } else if (cmd == "RELOAD") {
            engine->post([&manager]() {
                manager.sync_outputs();
            });
            return "OK";
        } else if (cmd.rfind("GET", 0) == 0) {
            std::string output_name = "*";
            if (cmd.length() > 3) {
                std::istringstream iss(cmd.substr(3));
                iss >> output_name;
                if (output_name.empty()) output_name = "*";
            }
            OutputConfig cfg = manager.get_current_config(output_name);
            return "OK " + mode_to_string(cfg.mode) + " " + color_to_hex(cfg.bg_color) + " " + cfg.image_path;
        }
        return "ERR Unknown command";
    });

    if (!server.start()) {
        std::cerr << "[miqubg] Warning: Could not bind IPC socket. Running standalone.\n";
    }

    std::cout << "[miqubg] Daemon started successfully with wallpaper: "
              << (default_cfg.image_path.empty() ? "(solid background color)" : default_cfg.image_path)
              << "\n";

    int ret = engine->enter_loop();

    std::cout << "[miqubg] Shutting down.\n";
    server.stop();
    g_engine.reset();
    return ret;
}
