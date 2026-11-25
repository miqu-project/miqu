#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace miqu {

class PopupMenu;
class View;

struct AppHandler {
    std::string desktop_id;   // e.g. "miqunotepad.desktop"
    std::string desktop_path; // Full path to .desktop file
    std::string name;         // Display name
    std::string icon;         // Icon name or path
    std::string exec;         // Raw Exec= command
    bool terminal = false;
    bool is_default = false;
};

class FileOpener {
public:
    // Detect MIME type via xdg-mime (returns e.g. "text/plain", empty on failure).
    static std::string query_mime_type(const std::filesystem::path& path);

    // Return desktop_id of default handler for mime (empty if none set).
    static std::string query_default_desktop_id(const std::string& mime);

    // Query registered handlers for the file's MIME type (default first).
    static std::vector<AppHandler> query_handlers(const std::filesystem::path& path,
                                                  std::string* out_mime = nullptr);

    // Returns true if the path is an executable regular file (and not a .desktop file).
    static bool is_executable_file(const std::filesystem::path& path);

    // Launch using the default handler (or .desktop launcher).
    // Returns false if no default handler is configured so caller can show Open With picker.
    static bool launch_default(const std::filesystem::path& path);

    // Launch using a specific handler.
    static bool launch_handler(const AppHandler& handler, const std::filesystem::path& path);

    // Run an executable file directly (or in a terminal).
    static bool launch_executable(const std::filesystem::path& path, bool in_terminal);

    // Fallback launch via xdg-open (detached, safe argv).
    static bool launch_xdg_open(const std::filesystem::path& path);

    // Set default handler for a MIME type via xdg-mime default.
    static bool set_default_handler(const std::string& mime, const std::string& desktop_id);

    // Spawn a detached child process safely without shell interpolation.
    static bool spawn_detached(const std::vector<std::string>& argv);

    // Build an "Open With" PopupMenu for the given file path.
    static std::shared_ptr<PopupMenu> build_open_with_menu(const std::filesystem::path& path);

    // Open with default handler, or show the Open With PopupMenu if executable / no default set.
    static void open_or_show_menu(const std::filesystem::path& path,
                                  const std::shared_ptr<View>& anchor,
                                  std::shared_ptr<PopupMenu>& out_menu);
};

} // namespace miqu
