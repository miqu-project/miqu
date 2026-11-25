#include "miqutoolkit/core/file_opener.hpp"
#include "miqutoolkit/view/popup_menu.hpp"
#include "miqutoolkit/view/view.hpp"

#include <algorithm>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_set>

namespace fs = std::filesystem;

namespace miqu {

namespace {

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string run_capture(const std::vector<std::string>& argv, int* out_exit_code = nullptr) {
    if (argv.empty()) return "";
    int pipefd[2];
    if (pipe(pipefd) != 0) return "";

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return "";
    }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        close(pipefd[1]);

        std::vector<char*> c_args;
        c_args.reserve(argv.size() + 1);
        for (const auto& a : argv) c_args.push_back(const_cast<char*>(a.c_str()));
        c_args.push_back(nullptr);
        execvp(c_args[0], c_args.data());
        _exit(127);
    }

    close(pipefd[1]);
    std::string output;
    char buf[512];
    ssize_t n;
    while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
        output.append(buf, static_cast<size_t>(n));
    }
    close(pipefd[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    if (out_exit_code) {
        *out_exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
    return output;
}

std::vector<fs::path> desktop_search_dirs() {
    std::vector<fs::path> dirs;
    if (const char* data_home = std::getenv("XDG_DATA_HOME")) {
        if (*data_home) dirs.emplace_back(fs::path(data_home) / "applications");
    } else if (const char* home = std::getenv("HOME")) {
        dirs.emplace_back(fs::path(home) / ".local/share/applications");
    }
    dirs.emplace_back("/usr/local/share/applications");
    dirs.emplace_back("/usr/share/applications");
    return dirs;
}

fs::path find_desktop_file(const std::string& desktop_id) {
    if (desktop_id.empty()) return {};
    for (const auto& dir : desktop_search_dirs()) {
        fs::path candidate = dir / desktop_id;
        std::error_code ec;
        if (fs::exists(candidate, ec) && fs::is_regular_file(candidate, ec)) {
            return candidate;
        }
    }
    return {};
}

bool parse_desktop_file(const fs::path& desktop_path, const std::string& desktop_id, AppHandler& out) {
    std::ifstream in(desktop_path);
    if (!in.is_open()) return false;

    out.desktop_id = desktop_id;
    out.desktop_path = desktop_path.string();

    bool in_desktop_entry = false;
    bool no_display = false;
    bool hidden = false;
    std::string line;
    while (std::getline(in, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;
        if (t.front() == '[' && t.back() == ']') {
            in_desktop_entry = (t == "[Desktop Entry]");
            continue;
        }
        if (!in_desktop_entry) continue;

        size_t eq = t.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(t.substr(0, eq));
        std::string val = trim(t.substr(eq + 1));

        if (key == "Name" && out.name.empty()) {
            out.name = val;
        } else if (key == "Icon" && out.icon.empty()) {
            out.icon = val;
        } else if (key == "Exec" && out.exec.empty()) {
            out.exec = val;
        } else if (key == "Terminal") {
            out.terminal = (val == "true" || val == "1");
        } else if (key == "NoDisplay") {
            no_display = (val == "true" || val == "1");
        } else if (key == "Hidden") {
            hidden = (val == "true" || val == "1");
        }
    }

    if (hidden || no_display || out.exec.empty()) return false;
    if (out.name.empty()) out.name = desktop_id;
    return true;
}

std::vector<std::string> split_exec_args(const std::string& exec_str) {
    std::vector<std::string> args;
    std::string cur;
    bool in_single = false;
    bool in_double = false;
    bool escape = false;

    for (char c : exec_str) {
        if (escape) {
            cur.push_back(c);
            escape = false;
            continue;
        }
        if (c == '\\' && !in_single) {
            escape = true;
            continue;
        }
        if (c == '\'' && !in_double) {
            in_single = !in_single;
            continue;
        }
        if (c == '"' && !in_single) {
            in_double = !in_double;
            continue;
        }
        if ((c == ' ' || c == '\t') && !in_single && !in_double) {
            if (!cur.empty()) {
                args.push_back(std::move(cur));
                cur.clear();
            }
            continue;
        }
        cur.push_back(c);
    }
    if (!cur.empty()) args.push_back(std::move(cur));
    return args;
}

std::string find_terminal_emulator() {
    if (const char* term = std::getenv("TERMINAL")) {
        if (*term) return term;
    }
    static constexpr const char* candidates[] = {
        "miquterm", "kitty", "alacritty", "foot", "wezterm", "gnome-terminal", "xterm"
    };
    const char* path_env = std::getenv("PATH");
    if (!path_env) return "xterm";

    std::stringstream ss(path_env);
    std::string dir;
    std::vector<std::string> path_dirs;
    while (std::getline(ss, dir, ':')) {
        if (!dir.empty()) path_dirs.push_back(dir);
    }

    for (const char* candidate : candidates) {
        for (const auto& pdir : path_dirs) {
            fs::path full = fs::path(pdir) / candidate;
            if (access(full.c_str(), X_OK) == 0) {
                return candidate;
            }
        }
    }
    return "xterm";
}

} // namespace

bool FileOpener::spawn_detached(const std::vector<std::string>& argv) {
    if (argv.empty()) return false;

    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        setsid();
        // Double-fork so the launched app is reparented to PID 1 (no zombies)
        pid_t pid2 = fork();
        if (pid2 < 0) _exit(127);
        if (pid2 > 0) _exit(0);

        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            if (devnull > 2) close(devnull);
        }

        std::vector<char*> c_args;
        c_args.reserve(argv.size() + 1);
        for (const auto& a : argv) c_args.push_back(const_cast<char*>(a.c_str()));
        c_args.push_back(nullptr);
        execvp(c_args[0], c_args.data());
        _exit(127);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

std::string FileOpener::query_mime_type(const fs::path& path) {
    if (path.empty()) return "";
    int code = 0;
    std::string out = trim(run_capture({"xdg-mime", "query", "filetype", path.string()}, &code));
    if (code != 0) return "";
    if (out == "inode/x-empty" || out == "application/x-zerosize") return "text/plain";
    return out;
}

std::string FileOpener::query_default_desktop_id(const std::string& mime) {
    if (mime.empty()) return "";
    int code = 0;
    std::string out = trim(run_capture({"xdg-mime", "query", "default", mime}, &code));
    if (code != 0) return "";
    return out;
}

std::vector<AppHandler> FileOpener::query_handlers(const fs::path& path, std::string* out_mime) {
    std::vector<AppHandler> handlers;
    std::string mime = query_mime_type(path);
    if (out_mime) *out_mime = mime;
    if (mime.empty()) return handlers;

    std::string default_id = query_default_desktop_id(mime);
    std::vector<std::string> candidate_ids;
    std::unordered_set<std::string> seen;

    auto add_id = [&](const std::string& id) {
        std::string t = trim(id);
        if (t.size() > 8 && t.substr(t.size() - 8) == ".desktop" && seen.insert(t).second) {
            candidate_ids.push_back(t);
        }
    };

    if (!default_id.empty()) add_id(default_id);

    // Query gio mime <mime> for registered & recommended .desktop handlers
    int gio_code = 0;
    std::string gio_out = run_capture({"gio", "mime", mime}, &gio_code);
    if (gio_code == 0 && !gio_out.empty()) {
        std::istringstream iss(gio_out);
        std::string line;
        while (std::getline(iss, line)) {
            std::string t = trim(line);
            if (t.rfind("Default application", 0) == 0) {
                size_t colon = t.find(':');
                if (colon != std::string::npos) {
                    std::string def = trim(t.substr(colon + 1));
                    if (def != "(none)") add_id(def);
                }
            } else if (t.size() > 8 && t.substr(t.size() - 8) == ".desktop") {
                add_id(t);
            }
        }
    }

    // Ensure miqunotepad is offered for text files if installed
    if (mime.rfind("text/", 0) == 0 || mime == "application/x-shellscript" ||
        mime == "application/json" || mime == "application/xml") {
        add_id("miqunotepad.desktop");
    }

    for (const auto& id : candidate_ids) {
        fs::path dpath = find_desktop_file(id);
        if (dpath.empty()) continue;
        AppHandler handler;
        if (parse_desktop_file(dpath, id, handler)) {
            handler.is_default = (!default_id.empty() && id == default_id);
            handlers.push_back(std::move(handler));
        }
    }

    return handlers;
}

bool FileOpener::is_executable_file(const fs::path& path) {
    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) return false;
    if (path.extension() == ".desktop") return false;
    return access(path.c_str(), X_OK) == 0;
}

bool FileOpener::launch_default(const fs::path& path) {
    if (path.empty()) return false;

    if (path.extension() == ".desktop") {
        AppHandler handler;
        if (parse_desktop_file(path, path.filename().string(), handler)) {
            std::vector<std::string> args = split_exec_args(handler.exec);
            std::vector<std::string> clean_args;
            for (const auto& a : args) {
                if (a == "%f" || a == "%F" || a == "%u" || a == "%U" ||
                    a == "%i" || a == "%c" || a == "%k") {
                    continue;
                }
                clean_args.push_back(a);
            }
            if (!clean_args.empty()) {
                if (handler.terminal) {
                    std::vector<std::string> term_args = {find_terminal_emulator(), "-e"};
                    term_args.insert(term_args.end(), clean_args.begin(), clean_args.end());
                    return spawn_detached(term_args);
                }
                return spawn_detached(clean_args);
            }
        }
        return spawn_detached({"gio", "launch", path.string()});
    }

    std::string mime = query_mime_type(path);
    std::string default_id = query_default_desktop_id(mime);
    if (default_id.empty()) {
        // No default handler set: caller should show Open With picker
        return false;
    }

    fs::path dpath = find_desktop_file(default_id);
    if (!dpath.empty()) {
        AppHandler handler;
        if (parse_desktop_file(dpath, default_id, handler)) {
            return launch_handler(handler, path);
        }
    }

    return launch_xdg_open(path);
}

bool FileOpener::launch_handler(const AppHandler& handler, const fs::path& path) {
    if (handler.exec.empty() || path.empty()) return false;

    std::vector<std::string> raw_args = split_exec_args(handler.exec);
    if (raw_args.empty()) return false;

    std::vector<std::string> cmd_args;
    bool inserted_file = false;
    for (const auto& a : raw_args) {
        if (a == "%f" || a == "%F" || a == "%u" || a == "%U") {
            cmd_args.push_back(path.string());
            inserted_file = true;
        } else if (a == "%i" || a == "%c" || a == "%k") {
            continue;
        } else {
            cmd_args.push_back(a);
        }
    }
    if (!inserted_file) {
        cmd_args.push_back(path.string());
    }

    if (handler.terminal) {
        std::vector<std::string> term_args = {find_terminal_emulator(), "-e"};
        term_args.insert(term_args.end(), cmd_args.begin(), cmd_args.end());
        return spawn_detached(term_args);
    }

    return spawn_detached(cmd_args);
}

bool FileOpener::launch_executable(const fs::path& path, bool in_terminal) {
    if (path.empty()) return false;
    if (in_terminal) {
        return spawn_detached({find_terminal_emulator(), "-e", path.string()});
    }
    return spawn_detached({path.string()});
}

bool FileOpener::launch_xdg_open(const fs::path& path) {
    if (path.empty()) return false;
    return spawn_detached({"xdg-open", path.string()});
}

bool FileOpener::set_default_handler(const std::string& mime, const std::string& desktop_id) {
    if (mime.empty() || desktop_id.empty()) return false;
    int code = 0;
    run_capture({"xdg-mime", "default", desktop_id, mime}, &code);
    return code == 0;
}

std::shared_ptr<PopupMenu> FileOpener::build_open_with_menu(const fs::path& path,
                                                            const std::string& exclude_desktop_id) {
    std::string mime;
    auto handlers = query_handlers(path, &mime);
    if (!exclude_desktop_id.empty()) {
        handlers.erase(
            std::remove_if(handlers.begin(), handlers.end(),
                           [&](const AppHandler& h) { return h.desktop_id == exclude_desktop_id; }),
            handlers.end());
    }
    bool is_exec = is_executable_file(path);

    auto menu = PopupMenuBuilder::create()
        ->minWidth(230)
        ->maxVisibleItems(10)
        ->build();

    std::string header = mime.empty() ? "Open with" : ("Open with (" + mime + ")");
    menu->add_section_header(header);

    if (is_exec) {
        menu->add_item("Run", "system-run", [path]() {
            launch_executable(path, false);
        });
        menu->add_item("Run in Terminal", "utilities-terminal", [path]() {
            launch_executable(path, true);
        });
        if (!handlers.empty()) {
            menu->add_separator();
        }
    }

    for (const auto& h : handlers) {
        std::string label = h.is_default ? (h.name + " (Default)") : h.name;
        menu->add_item(label, h.icon, [h, path]() {
            launch_handler(h, path);
        });
    }

    if (handlers.empty() && !is_exec) {
        menu->add_item("System Default (xdg-open)", "application-x-executable", [path]() {
            launch_xdg_open(path);
        });
    }

    if (!mime.empty()) {
        bool has_non_default = false;
        for (const auto& h : handlers) {
            if (!h.is_default) {
                has_non_default = true;
                break;
            }
        }
        if (has_non_default) {
            menu->add_separator();
            menu->add_section_header("Set Default for " + mime);
            for (const auto& h : handlers) {
                if (h.is_default) continue;
                menu->add_item("Always use " + h.name, h.icon, [mime, h]() {
                    set_default_handler(mime, h.desktop_id);
                });
            }
        }
    }

    return menu;
}

void FileOpener::open_or_show_menu(const fs::path& path,
                                   const std::shared_ptr<View>& anchor,
                                   std::shared_ptr<PopupMenu>& out_menu) {
    if (path.empty()) return;
    if (is_executable_file(path) || !launch_default(path)) {
        if (!anchor) return;
        out_menu = build_open_with_menu(path);
        out_menu->show_as_dropdown(anchor, PopupGravity::BottomStart, 0, 4);
    }
}

} // namespace miqu
