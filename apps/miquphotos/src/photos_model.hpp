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
    std::string title;           // filename (e.g. "sunset.jpg" or "Wallpapers")
    std::string subtitle;        // e.g. "48 photos" or "1920 × 1080 • 2.4 MB"
    std::string icon;            // icon (e.g. "󰋩", "󰈟", "󰉏")
    bool is_category = false;    // true for photo album/folder
    std::filesystem::path path;

    // Children and lazy provider
    std::vector<std::shared_ptr<FileItem>> children;
    std::function<void(FileItem*)> provider = nullptr;

    // Attributes
    bool is_dir = false;
    bool is_symlink = false;
    bool is_hidden = false;
    uintmax_t size_bytes = 0;
    size_t photo_count = 0;
    std::string size_formatted;
    std::string permissions_str;
    std::string mtime_formatted;
    FileCategory category = FileCategory::Image;
    std::string category_label = "Image";

    // Dimensions
    int width = 0;
    int height = 0;
    std::string dimensions_formatted;

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
};

class PhotosModel {
public:
    static std::vector<std::shared_ptr<FileItem>> build_default_tree(const std::string& start_path = "");
    static std::shared_ptr<FileItem> create_album_item(const std::filesystem::path& dir, size_t count);
    static std::shared_ptr<FileItem> create_photo_item(const std::filesystem::path& path);
    static std::vector<std::shared_ptr<FileItem>> scan_photos_in_directory(const std::filesystem::path& dir);
    static bool is_image_file(const std::filesystem::path& p);
    static size_t count_images_in_directory(const std::filesystem::path& dir);

    static std::string format_file_size(uintmax_t bytes);
    static std::string format_time(std::filesystem::file_time_type ftime);
};

} // namespace miqu
