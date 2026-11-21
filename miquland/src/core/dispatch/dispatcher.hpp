#pragma once

#include <string>
#include <vector>
#include <sys/types.h>

namespace miquland {

class Server;

class Dispatcher {
public:
    explicit Dispatcher(Server* server);
    ~Dispatcher();

    // Dispatches a compositor action (e.g. "close", "toggle_fullscreen", "ws_1", etc., or spawns external command)
    bool execute_action(const std::string& action);

    // Spawns an external command or detached process safely
    void spawn_command(const char* cmd);

    // Gracefully terminates all tracked spawned child processes on session shutdown
    void terminate_spawned_processes();

    // Returns tracked process IDs
    const std::vector<pid_t>& get_spawned_pids() const { return m_spawned_pids; }

private:
    Server* m_server = nullptr;
    std::vector<pid_t> m_spawned_pids;
};

} // namespace miquland
