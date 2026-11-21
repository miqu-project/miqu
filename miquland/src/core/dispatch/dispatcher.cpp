#include "dispatcher.hpp"
#include "server.hpp"
#include "view.hpp"
#include "workspace.hpp"
#include "input/input.hpp"
#include "plugin_manager.hpp"
#include "core/common/util.hpp"

#include <spawn.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <filesystem>
#include <vector>

extern char** environ;

namespace miquland {

static std::vector<std::string> split_simple_command(const std::string& cmd_str) {
    std::vector<std::string> args;
    std::string current;
    bool in_single = false, in_double = false;
    for (size_t i = 0; i < cmd_str.size(); ++i) {
        char c = cmd_str[i];
        if (c == '\\' && i + 1 < cmd_str.size() && !in_single) {
            current += cmd_str[++i];
        } else if (c == '\'' && !in_double) {
            in_single = !in_single;
        } else if (c == '"' && !in_single) {
            in_double = !in_double;
        } else if ((c == ' ' || c == '\t') && !in_single && !in_double) {
            if (!current.empty()) {
                args.push_back(std::move(current));
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) args.push_back(std::move(current));
    return args;
}

Dispatcher::Dispatcher(Server* server)
    : m_server(server) {}

Dispatcher::~Dispatcher() {
    terminate_spawned_processes();
}

void Dispatcher::spawn_command(const char* cmd) {
    if (!cmd || !*cmd) return;
    log_info("Executing command via Dispatcher: " + std::string(cmd));

    if (m_server && m_server->get_input_manager()) {
        m_server->get_input_manager()->trigger_app_launch_cursor();
    }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addclosefrom_np(&actions, 3);

    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
    posix_spawnattr_setpgroup(&attr, 0);

    pid_t pid = -1;
    int ret = -1;

    std::string cmd_str(cmd);
    // Fast path: if command contains no shell metacharacters, execvp directly to avoid /bin/sh fork overhead
    if (cmd_str.find_first_of("|&;<>()$`\n*?") == std::string::npos) {
        auto args = split_simple_command(cmd_str);
        if (!args.empty()) {
            // Fast path: bypass shell wrapper scripts for well-known desktop browsers
            if (args[0] == "firefox" && std::filesystem::exists("/usr/lib/firefox/firefox")) {
                args[0] = "/usr/lib/firefox/firefox";
            } else if (args[0] == "brave" && std::filesystem::exists("/opt/brave-bin/brave")) {
                args[0] = "/opt/brave-bin/brave";
            }

            std::vector<char*> c_argv;
            c_argv.reserve(args.size() + 1);
            for (auto& s : args) c_argv.push_back(s.data());
            c_argv.push_back(nullptr);

            ret = posix_spawnp(&pid, c_argv[0], &actions, &attr, c_argv.data(), environ);
        }
    }

    if (ret != 0) {
        const char* sh_argv[] = {"/bin/sh", "-c", cmd, nullptr};
        ret = posix_spawnp(&pid, "/bin/sh", &actions, &attr, (char* const*)sh_argv, environ);
    }

    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attr);

    if (ret == 0 && pid > 0) {
        m_spawned_pids.push_back(pid);
    } else {
        log_error("Failed to spawn process for command: " + std::string(cmd) + " (code: " + std::to_string(ret) + ")");
    }
}

void Dispatcher::terminate_spawned_processes() {
    if (m_spawned_pids.empty()) return;
    log_info("Terminating all spawned session processes (" + std::to_string(m_spawned_pids.size()) + " tracked)");
    for (pid_t pid : m_spawned_pids) {
        if (pid > 0) {
            kill(-pid, SIGTERM);
            kill(pid, SIGTERM);
        }
    }
    m_spawned_pids.clear();
}

bool Dispatcher::execute_action(const std::string& action) {
    if (action.empty()) {
        return false;
    }

    if (m_server->get_plugin_manager() && m_server->get_plugin_manager()->execute_dispatcher(action)) {
        return true;
    }

    if (action == "close" || action == "close_window") {
        View* focused = m_server->get_focused_view();
        if (focused) focused->close();
    } else if (action == "toggle_fullscreen" || action == "fullscreen") {
        View* focused = m_server->get_focused_view();
        if (focused) {
            focused->set_fullscreen(!focused->is_fullscreen());
        }
    } else if (action == "toggle_floating" || action == "toggle_float" || action == "floating_toggle") {
        m_server->get_workspace_manager()->toggle_floating_active();
    } else if (action == "toggle_layout" || action == "layout_toggle" || action == "cycle_layout") {
        m_server->get_workspace_manager()->toggle_layout_mode();
    } else if (action == "swap_main" || action == "swap_master" || action == "swap_with_main") {
        m_server->get_workspace_manager()->swap_with_main();
    } else if (action == "toggle_split" || action == "split_toggle" || action == "togglesplit") {
        m_server->get_workspace_manager()->toggle_active_split();
    } else if (action == "focus_win_1" || action == "window_1") {
        m_server->get_workspace_manager()->focus_window_index(0);
    } else if (action == "focus_win_2" || action == "window_2") {
        m_server->get_workspace_manager()->focus_window_index(1);
    } else if (action == "toggle_focus" || action == "next_window" || action == "focus_next") {
        m_server->get_workspace_manager()->focus_next_view();
    } else if (action == "prev_window" || action == "focus_prev") {
        m_server->get_workspace_manager()->focus_prev_view();
    } else if (action == "prev_ws" || action == "prev_workspace") {
        m_server->get_workspace_manager()->prev_workspace();
    } else if (action == "next_ws" || action == "next_workspace") {
        m_server->get_workspace_manager()->next_workspace();
    } else if (action.rfind("ws_", 0) == 0) {
        try {
            size_t ws_id = std::stoul(action.substr(3));
            m_server->get_workspace_manager()->switch_to_workspace(ws_id);
        } catch (...) {}
    } else if (action.rfind("move_ws_", 0) == 0) {
        try {
            size_t ws_id = std::stoul(action.substr(8));
            View* focused = m_server->get_focused_view();
            if (focused) m_server->get_workspace_manager()->move_view_to_workspace(focused, ws_id);
        } catch (...) {}
    } else if (action.rfind("move_to_ws_", 0) == 0) {
        try {
            size_t ws_id = std::stoul(action.substr(11));
            View* focused = m_server->get_focused_view();
            if (focused) m_server->get_workspace_manager()->move_view_to_workspace(focused, ws_id);
        } catch (...) {}
    } else if (action.rfind("movetoworkspace_", 0) == 0) {
        try {
            size_t ws_id = std::stoul(action.substr(16));
            View* focused = m_server->get_focused_view();
            if (focused) m_server->get_workspace_manager()->move_view_to_workspace(focused, ws_id);
        } catch (...) {}
    } else if (action == "shutdown" || action == "graceful_shutdown") {
        m_server->graceful_shutdown(2500);
    } else if (action == "exit" || action == "quit") {
        m_server->terminate();
    } else {
        spawn_command(action.c_str());
    }
    return true;
}

} // namespace miquland
