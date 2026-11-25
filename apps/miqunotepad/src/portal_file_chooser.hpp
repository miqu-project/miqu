#pragma once

#include <functional>
#include <string>

namespace miqu {

struct PortalPickResult {
    bool available = false;  // False if xdg-desktop-portal could not be reached
    bool accepted = false;   // True if user picked a file
    std::string path;        // Decoded local filesystem path
};

class PortalFileChooser {
public:
    // Blocking call (run on a worker thread) to open XDG Desktop Portal OpenFile dialog.
    static PortalPickResult pick_open_file(const std::string& title = "Open File",
                                           const std::string& current_dir = "");

    // Blocking call (run on a worker thread) to open XDG Desktop Portal SaveFile dialog.
    static PortalPickResult pick_save_file(const std::string& title = "Save File",
                                           const std::string& current_name = "Untitled.txt",
                                           const std::string& current_dir = "");
};

} // namespace miqu
