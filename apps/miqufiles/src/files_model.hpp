#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <memory>
#include <functional>
#include <cstdint>

namespace miqu {

enum class FileCategory {
    Directory,
    Image,
    Video,
    Audio,
    Document,
    CodeText,
    Archive,
    Executable,
    Other
};

class FileItem {
public:
    std::string id;              // canonical path string
    std::string title;           // filename (e.g. "Documents" or "main.cpp")
    std::string subtitle;        // size / child count
    std::string icon;            // Nerd font icon (e.g. "󰉋", "󰈙", "󰋩", etc.)
    bool is_category = false;    // true for directory
    std::filesystem::path path;

    // Children and lazy provider
    std::vector<std::shared_ptr<FileItem>> children;
    std::function<void(FileItem*)> provider = nullptr;

    // Attributes
    bool is_dir = false;
    bool is_symlink = false;
    bool is_hidden = false;
    uintmax_t size_bytes = 0;
    size_t child_count = 0;
    std::string size_formatted;
    std::string permissions_str;
    std::string mtime_formatted;
    FileCategory category = FileCategory::Other;
    std::string category_label;
    std::string symlink_target;

    // Actions
    bool has_action = true;
    std::string action_label = "Open";
    std::string last_action_message;

    void ensure_children() {
        if (provider) {
            auto p = std::move(provider);
            provider = nullptr;
            p(this);
        }
    }

    void reload_children(bool show_hidden);
};

class FilesModel {
public:
    static std::vector<std::shared_ptr<FileItem>> build_default_tree(const std::string& start_path = "", bool show_hidden = false);
    static std::shared_ptr<FileItem> create_directory_item(const std::filesystem::path& dir,
                                                           const std::string& custom_title = "",
                                                           const std::string& custom_icon = "",
                                                           bool show_hidden = false);
    static std::shared_ptr<FileItem> create_file_item(const std::filesystem::path& path);
    static std::vector<std::shared_ptr<FileItem>> scan_directory(const std::filesystem::path& dir, bool show_hidden = false);

    static std::string format_file_size(uintmax_t bytes);
    static std::string format_permissions(std::filesystem::perms p);
    static std::string format_time(std::filesystem::file_time_type ftime);
    static FileCategory detect_category(const std::filesystem::path& path, bool is_dir);
    static std::string get_icon_for_category(FileCategory cat, const std::string& filename = "");
};

} // namespace miqu
