#include "wallpaper_manager.hpp"
#include "config.hpp"
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
    return Config::mode_to_string(mode);
}

static std::string color_to_hex(const miqu::Color& c) {
    return Config::color_to_hex(c);
}

static void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [command|options...] [image_path]\n\n"
              << "Commands:\n"
              << "  query, list, -q, --query    Query active wallpaper and settings across monitors (swww-style)\n"
              << "  get, -g, --get              Print active wallpaper path for scripting (e.g. pywal, lockscreens)\n"
              << "  img, set, -i, --image <path> Set wallpaper image across all or targeted monitors\n"
              << "  clear [color]               Clear wallpaper image and show solid background color\n"
              << "  default, -d, --default      Reset to system default asset wallpaper\n"
              << "  reload, -r, --reload        Reload wallpapers across all monitors in place\n"
              << "  status, ping                Check if miqubg daemon is running and show IPC socket\n"
              << "  kill, quit                  Stop the running miqubg daemon gracefully via IPC\n\n"
              << "Options:\n"
              << "  --init-config               Initialize user configuration file (~/.config/miqubg/miqubg.conf)\n"
              << "  -C, --config <path>         Load custom configuration file\n"
              << "  -o, --output <name>         Target specific output (e.g. HDMI-A-1, eDP-1, or * for all)\n"
              << "  -m, --mode <mode>           Scaling mode: fill/cover, fit, stretch, center, tile (default: fill)\n"
              << "  -c, --color <#hex>          Solid background color in hex (e.g. #1e1e2e, default: #000000)\n"
              << "  -v, --version, version      Show version number and quit\n"
              << "  -h, --help, help            Show this help message\n\n"
              << "Configuration & Persistence:\n"
              << "  miqubg reads ~/.config/miqubg/miqubg.conf (or /usr/share/miqubg/miqubg.conf).\n"
              << "  Setting a wallpaper automatically persists the active wallpaper and monitor configuration.\n\n"
              << "Scaling Modes:\n"
              << "  fill / cover    Scale to fill output, cropping if necessary (default)\n"
              << "  fit / contain   Scale to fit inside output, letterboxing with background color\n"
              << "  stretch         Stretch image to fill output (ignores aspect ratio)\n"
              << "  center          Center image at 1:1 scale without scaling\n"
              << "  tile            Tile the image repeatedly across output\n\n"
              << "Examples:\n"
              << "  " << prog << " --init-config\n"
              << "  " << prog << " query\n"
              << "  " << prog << " query -o HDMI-A-1\n"
              << "  " << prog << " get\n"
              << "  " << prog << " get -o eDP-1\n"
              << "  " << prog << " img /path/to/wallpaper.jpg\n"
              << "  " << prog << " set /path/to/wallpaper.jpg -m fit -c '#1e1e2e'\n"
              << "  " << prog << " img /path/to/wallpaper.jpg -o HDMI-A-1\n"
              << "  " << prog << " clear '#1e1e2e'\n"
              << "  " << prog << " default\n"
              << "  " << prog << " reload\n"
              << "  " << prog << " status\n"
              << "  " << prog << " kill\n";
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, sig_handler);
    std::signal(SIGTERM, sig_handler);

    OutputConfig default_cfg;
    default_cfg.mode = miqu::FitMode::Cover;
    default_cfg.bg_color = miqu::Color::rgb(0.0f, 0.0f, 0.0f);

    std::map<std::string, OutputConfig> output_cfgs;
    std::string current_output = ""; // empty means modifying default_cfg
    std::string custom_config_path = "";
    bool query_requested = false;
    bool get_requested = false;
    bool status_requested = false;
    bool kill_requested = false;
    bool reload_requested = false;
    bool explicit_image_set = false;
    bool default_requested = false;
    bool clear_requested = false;
    bool mode_set = false;
    bool color_set = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help" || arg == "help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--version" || arg == "version") {
            std::cout << "miqubg version 1.3.0\n";
            return 0;
        } else if (arg == "--init-config") {
            std::string res = Config::init_user_config();
            if (!res.empty()) {
                std::cout << "[miqubg] Configuration initialized at: " << res << "\n";
            } else {
                std::cout << "[miqubg] Failed to initialize configuration file.\n";
            }
            return 0;
        } else if (arg == "-C" || arg == "--config") {
            if (i + 1 < argc) {
                custom_config_path = argv[++i];
            } else {
                std::cerr << "Error: " << arg << " requires a file path\n";
                return 1;
            }
        } else if (arg == "query" || arg == "list" || arg == "-q" || arg == "--query") {
            query_requested = true;
        } else if (arg == "get" || arg == "-g" || arg == "--get") {
            get_requested = true;
        } else if (arg == "status" || arg == "ping") {
            status_requested = true;
        } else if (arg == "kill" || arg == "quit") {
            kill_requested = true;
        } else if (arg == "-r" || arg == "--reload" || arg == "reload") {
            reload_requested = true;
        } else if (arg == "-d" || arg == "--default" || arg == "default") {
            default_requested = true;
            std::string def_path = WallpaperManager::resolve_default_wallpaper();
            if (current_output.empty()) {
                default_cfg.image_path = def_path;
            } else {
                output_cfgs[current_output].image_path = def_path;
            }
        } else if (arg == "img" || arg == "set") {
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                std::string path = argv[++i];
                if (current_output.empty()) {
                    default_cfg.image_path = path;
                } else {
                    output_cfgs[current_output].image_path = path;
                }
                explicit_image_set = true;
            } else {
                std::cerr << "Error: '" << arg << "' requires a file path\n";
                return 1;
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
        } else if (arg == "clear") {
            clear_requested = true;
            miqu::Color col = miqu::Color::rgb(0.0f, 0.0f, 0.0f);
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                col = miqu::Color::from_hex(argv[++i], col);
            }
            if (current_output.empty()) {
                default_cfg.image_path = "";
                default_cfg.bg_color = col;
            } else {
                output_cfgs[current_output].image_path = "";
                output_cfgs[current_output].bg_color = col;
            }
            explicit_image_set = true;
            color_set = true;
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

    // If output flag was passed after image/mode/color, migrate default_cfg settings to that output
    bool cli_output_specified = !output_cfgs.empty() || !current_output.empty();
    if (!current_output.empty() && output_cfgs.find(current_output) == output_cfgs.end()) {
        output_cfgs[current_output] = default_cfg;
        default_cfg.image_path = "";
    }

    // Load configuration file according to AGENTS.md Two-Tier authority:
    // User config (~/.config/miqubg/miqubg.conf) > System baseline (/usr/share/miqubg/miqubg.conf)
    Config& bg_config = Config::get();
    bg_config.load(custom_config_path);

    // Merge configuration baseline with CLI arguments:
    // CLI arguments override file configuration.
    if (!explicit_image_set && !default_requested && !clear_requested) {
        OutputConfig file_def = bg_config.get_default_config();
        default_cfg.image_path = file_def.image_path;
        if (!mode_set) default_cfg.mode = file_def.mode;
        if (!color_set) default_cfg.bg_color = file_def.bg_color;
    }

    // Merge per-monitor configs from loaded file if not specified on CLI
    auto loaded_outputs = bg_config.get_output_configs();
    for (const auto& [name, cfg] : loaded_outputs) {
        if (output_cfgs.find(name) == output_cfgs.end()) {
            output_cfgs[name] = cfg;
        }
    }

    // 1. Handle Status / Ping check
    if (status_requested) {
        std::string resp;
        if (IPC::send_command("PING", resp) && resp == "PONG") {
            std::cout << "[miqubg] Daemon is running (socket: " << IPC::get_socket_path() << ")\n";
            return 0;
        } else {
            std::cerr << "[miqubg] Daemon is not running.\n";
            return 1;
        }
    }

    // 2. Handle Kill command
    if (kill_requested) {
        std::string resp;
        if (IPC::send_command("KILL", resp)) {
            std::cout << "[miqubg] Daemon stopped.\n";
            return 0;
        } else {
            std::cerr << "[miqubg] Error: Daemon is not running.\n";
            return 1;
        }
    }

    // 3. Handle Query command (swww-style multi-monitor listing)
    if (query_requested) {
        std::string target = current_output.empty() ? "*" : current_output;
        std::string resp;
        if (IPC::send_command("QUERY " + target, resp)) {
            if (resp == "ERR Unknown command") {
                // Fallback to GET for backward compatibility with older running daemon
                if (IPC::send_command("GET " + target, resp) && resp.rfind("OK ", 0) == 0) {
                    std::istringstream iss(resp.substr(3));
                    std::string cur_m, cur_c, cur_img;
                    if (iss >> cur_m >> cur_c) {
                        std::getline(iss, cur_img);
                        size_t f = cur_img.find_first_not_of(" \t");
                        if (f != std::string::npos) cur_img = cur_img.substr(f);
                        std::string out_disp = target == "*" ? "default" : target;
                        std::cout << out_disp << ": " << cur_img
                                  << ", mode: " << cur_m
                                  << ", color: " << cur_c << "\n";
                        return 0;
                    }
                }
            }
            if (resp.rfind("OK\n", 0) == 0) {
                std::cout << resp.substr(3) << "\n";
            } else if (resp.rfind("OK", 0) == 0) {
                std::string content = resp.substr(2);
                size_t f = content.find_first_not_of(" \t\n\r");
                if (f != std::string::npos) content = content.substr(f);
                std::cout << content << "\n";
            } else {
                std::cout << resp << "\n";
            }
            return 0;
        } else {
            std::cerr << "[miqubg] Error: Daemon is not running.\n";
            return 1;
        }
    }

    // 4. Handle Get command (raw path output for scripting)
    if (get_requested) {
        std::string target = current_output.empty() ? "*" : current_output;
        std::string resp;
        if (IPC::send_command("GET " + target, resp) && resp.rfind("OK ", 0) == 0) {
            std::istringstream iss(resp.substr(3));
            std::string cur_m, cur_c, cur_img;
            if (iss >> cur_m >> cur_c) {
                std::getline(iss, cur_img);
                size_t f = cur_img.find_first_not_of(" \t");
                if (f != std::string::npos) {
                    cur_img = cur_img.substr(f);
                } else {
                    cur_img = "";
                }
                if (cur_img.empty()) {
                    std::cout << "(solid color " << cur_c << ")\n";
                } else {
                    std::cout << cur_img << "\n";
                }
                return 0;
            }
        }
        std::cerr << "[miqubg] Error: Daemon is not running or failed to retrieve wallpaper.\n";
        return 1;
    }

    // 5. Handle Reload command
    if (reload_requested) {
        std::string resp;
        if (IPC::send_command("RELOAD", resp)) {
            std::cout << "[miqubg] Daemon reloaded.\n";
            return 0;
        } else {
            std::cerr << "[miqubg] Error: Daemon is not running.\n";
            return 1;
        }
    }

    // If no wallpaper was explicitly specified and not clear/solid-color, fall back to default asset wallpaper
    if (default_cfg.image_path.empty() && !clear_requested && !color_set) {
        default_cfg.image_path = WallpaperManager::resolve_default_wallpaper();
    }

    // 6. Handle Wallpaper Updates if daemon is already running
    bool has_explicit_action = explicit_image_set || default_requested || cli_output_specified || mode_set || color_set || clear_requested;

    if (has_explicit_action) {
        std::string resp;
        bool sent_any = false;

        if (cli_output_specified && !output_cfgs.empty()) {
            for (const auto& pair : output_cfgs) {
                std::string cmd = "SET " + pair.first + " " + mode_to_string(pair.second.mode) + " "
                                  + color_to_hex(pair.second.bg_color) + " " + pair.second.image_path;
                if (IPC::send_command(cmd, resp)) {
                    sent_any = true;
                }
            }
        } else {
            std::string img = default_cfg.image_path;
            // If only mode or color was updated without image, clear, or default reset, preserve running daemon's image
            if (!explicit_image_set && !default_requested && !clear_requested) {
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
            std::cout << "[miqubg] Wallpaper updated.\n";
            return 0;
        }
    } else {
        // No explicit wallpaper, mode, or output changes requested.
        // Check if an existing daemon is already running so we avoid trying to bind a duplicate daemon.
        std::string ping_resp;
        if (IPC::send_command("PING", ping_resp)) {
            std::cout << "[miqubg] Daemon is already running.\n"
                      << "  Use 'miqubg query' to see active wallpapers,\n"
                      << "  'miqubg img <path>' to set a new wallpaper, or\n"
                      << "  'miqubg --help' for all commands.\n";
            return 0;
        }
    }

    // If starting a fresh daemon and explicit CLI options were provided, persist them
    if (has_explicit_action) {
        if (cli_output_specified && !output_cfgs.empty()) {
            for (const auto& [name, cfg] : output_cfgs) {
                bg_config.set_output_config(name, cfg);
            }
        } else {
            bg_config.set_default_config(default_cfg);
        }
        bg_config.save();
    }

    // 7. Start as the Persistent Daemon
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

    // Setup inotify config watcher on AppEngine for dynamic live reload
    engine->setup_config_watcher();
    engine->add_theme_change_listener([&manager]() {
        if (Config::get().consume_internal_write_flag()) {
            return;
        }
        std::cout << "[miqubg] Configuration file changed on disk. Reloading...\n";
        Config::get().load();
        manager.apply_config(Config::get());
    });

    // Initialize IPC Server
    IPCServer server([&manager, &engine](const std::string& cmd) -> std::string {
        if (cmd == "PING") {
            return "PONG";
        } else if (cmd == "KILL" || cmd == "QUIT") {
            engine->post([engine]() {
                engine->quit(0);
            });
            return "OK";
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
                    manager.update_wallpaper(output_name, cfg, true);
                });
                return "OK";
            }
            return "ERR Invalid SET arguments";
        } else if (cmd.rfind("CLEAR", 0) == 0) {
            std::string output_name = "*", color_hex = "#000000";
            if (cmd.length() > 5) {
                std::istringstream iss(cmd.substr(5));
                iss >> output_name >> color_hex;
                if (output_name.empty()) output_name = "*";
                if (color_hex.empty()) color_hex = "#000000";
            }
            OutputConfig cfg;
            cfg.image_path = "";
            cfg.mode = miqu::FitMode::Cover;
            cfg.bg_color = miqu::Color::from_hex(color_hex, miqu::Color::rgb(0.0f, 0.0f, 0.0f));
            engine->post([&manager, output_name, cfg]() {
                manager.update_wallpaper(output_name, cfg, true);
            });
            return "OK";
        } else if (cmd == "RELOAD") {
            engine->post([&manager]() {
                Config::get().load();
                manager.apply_config(Config::get());
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
        } else if (cmd.rfind("QUERY", 0) == 0) {
            std::string output_name = "*";
            if (cmd.length() > 5) {
                std::istringstream iss(cmd.substr(5));
                iss >> output_name;
                if (output_name.empty()) output_name = "*";
            }
            std::ostringstream oss;
            oss << "OK\n";
            if (!output_name.empty() && output_name != "*") {
                OutputConfig cfg = manager.get_current_config(output_name);
                std::string img = cfg.image_path.empty() ? "(solid color)" : cfg.image_path;
                oss << output_name << ": " << img
                    << ", mode: " << mode_to_string(cfg.mode)
                    << ", color: " << color_to_hex(cfg.bg_color);
            } else {
                auto all = manager.get_all_active_configs();
                bool first = true;
                for (const auto& pair : all) {
                    if (!first) oss << "\n";
                    first = false;
                    std::string img = pair.second.image_path.empty() ? "(solid color)" : pair.second.image_path;
                    oss << pair.first << ": " << img
                        << ", mode: " << mode_to_string(pair.second.mode)
                        << ", color: " << color_to_hex(pair.second.bg_color);
                }
            }
            return oss.str();
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
