#include "photos_model.hpp"
#include <miqutoolkit/core/fs_utils.hpp>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <unordered_set>
#include <iostream>

namespace fs = std::filesystem;

namespace miqu {

bool PhotosModel::is_image_file(const fs::path& p) {
    std::error_code ec;
    if (!fs::is_regular_file(p, ec)) {
        return false;
    }
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

    static const std::unordered_set<std::string> valid_exts = {
        ".jpg", ".jpeg", ".png", ".webp", ".avif", ".heic",
        ".gif", ".svg", ".bmp", ".tiff", ".tif", ".ico"
    };
    return valid_exts.find(ext) != valid_exts.end();
}

std::string PhotosModel::format_file_size(uintmax_t bytes) {
    constexpr double KB = 1024.0;
    constexpr double MB = KB * 1024.0;
    constexpr double GB = MB * 1024.0;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);

    if (bytes >= GB) {
        oss << (bytes / GB) << " GB";
    } else if (bytes >= MB) {
        oss << (bytes / MB) << " MB";
    } else if (bytes >= KB) {
        oss << (bytes / KB) << " KB";
    } else {
        oss << bytes << " B";
    }
    return oss.str();
}

std::string PhotosModel::format_time(fs::file_time_type ftime) {
    try {
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
        std::tm tm_buf{};
        if (localtime_r(&tt, &tm_buf)) {
            std::ostringstream oss;
            oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M");
            return oss.str();
        }
    } catch (...) {}
    return "";
}

size_t PhotosModel::count_images_in_directory(const fs::path& dir) {
    size_t count = 0;
    std::error_code ec;
    for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; ++it) {
        if (is_image_file(it->path())) {
            count++;
        }
    }
    return count;
}

std::shared_ptr<FileItem> PhotosModel::create_album_item(const fs::path& dir, size_t count) {
    auto item = std::make_shared<FileItem>();
    item->path = dir;
    item->id = dir.string();
    item->title = dir.filename().string();
    if (item->title.empty()) item->title = "Photos";
    item->subtitle = std::to_string(count) + (count == 1 ? " photo" : " photos");
    item->size_formatted = item->subtitle;
    item->photo_count = count;
    item->icon = "folder-pictures";
    item->is_category = true;
    item->is_dir = true;

    std::error_code ec;
    auto ftime = fs::last_write_time(dir, ec);
    if (!ec) item->mtime_formatted = format_time(ftime);

    item->provider = [](FileItem* self) {
        self->children = scan_photos_in_directory(self->path);
    };

    return item;
}

std::shared_ptr<FileItem> PhotosModel::create_photo_item(const fs::path& path) {
    auto item = std::make_shared<FileItem>();
    item->path = path;
    item->id = path.string();
    item->title = path.filename().string();
    item->is_category = false;
    item->is_dir = false;
    item->category = FileCategory::Image;
    item->category_label = "Image";

    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::toupper(c); });
    if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
    if (!ext.empty()) item->category_label = ext + " Image";

    item->icon = "image-x-generic";

    std::error_code ec;
    uintmax_t sz = fs::file_size(path, ec);
    if (!ec) {
        item->size_bytes = sz;
        item->size_formatted = format_file_size(sz);
    }

    auto ftime = fs::last_write_time(path, ec);
    if (!ec) {
        item->mtime_formatted = format_time(ftime);
    }

    item->subtitle = item->size_formatted;
    return item;
}

std::vector<std::shared_ptr<FileItem>> PhotosModel::scan_photos_in_directory(const fs::path& dir) {
    std::vector<std::shared_ptr<FileItem>> items;
    std::error_code ec;

    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
        return items;
    }

    for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; ++it) {
        if (is_image_file(it->path())) {
            items.push_back(create_photo_item(it->path()));
        }
    }

    // Sort: newest photos first (by mtime), then alphabetically
    std::sort(items.begin(), items.end(), [](const std::shared_ptr<FileItem>& a, const std::shared_ptr<FileItem>& b) {
        if (a->mtime_formatted != b->mtime_formatted) {
            return a->mtime_formatted > b->mtime_formatted;
        }
        return a->title < b->title;
    });

    return items;
}

std::vector<std::shared_ptr<FileItem>> PhotosModel::build_default_tree(const std::string& start_path) {
    std::vector<std::shared_ptr<FileItem>> tree;
    std::unordered_set<std::string> seen_paths;

    const char* home_env = std::getenv("HOME");
    fs::path home = home_env ? fs::path(home_env) : fs::current_path();

    auto should_skip_dir = [](const std::string& name) -> bool {
        if (name.empty()) return true;
        if (name[0] == '.') return true;
        static const std::unordered_set<std::string> skip_names = {
            "node_modules", "build", "target", "dist", "CMakeFiles",
            "venv", ".venv", "__pycache__", "obj", "bin"
        };
        return skip_names.find(name) != skip_names.end();
    };

    auto inspect_and_add = [&](const fs::path& dir) {
        std::error_code ec;
        if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) return;

        fs::path canon = fs::canonical(dir, ec);
        if (ec) canon = dir;

        // Never add user home itself as an album (direct home photos are listed directly as files)
        if (canon == home) return;

        if (seen_paths.count(canon.string())) return;

        size_t count = count_images_in_directory(canon);
        if (count > 0) {
            seen_paths.insert(canon.string());
            tree.push_back(create_album_item(canon, count));
        }
    };

    std::function<void(const fs::path&, int)> scan_subdirs_recursive =
        [&](const fs::path& base, int max_depth) {
            if (max_depth <= 0) return;
            std::error_code ec;
            for (fs::directory_iterator it(base, fs::directory_options::skip_permission_denied, ec), end;
                 !ec && it != end; ++it) {
                if (it->is_directory(ec)) {
                    std::string dname = it->path().filename().string();
                    if (!should_skip_dir(dname)) {
                        inspect_and_add(it->path());
                        scan_subdirs_recursive(it->path(), max_depth - 1);
                    }
                }
            }
        };

    // 1. If start_path was explicitly provided, scan it first
    if (!start_path.empty()) {
        std::error_code ec;
        fs::path sp = FsUtils::expand_user_path(start_path);
        if (fs::exists(sp, ec)) {
            if (fs::is_directory(sp, ec)) {
                inspect_and_add(sp);
                scan_subdirs_recursive(sp, 3);
            } else if (fs::is_regular_file(sp, ec)) {
                inspect_and_add(sp.parent_path());
                scan_subdirs_recursive(sp.parent_path(), 3);
            }
        }
    }

    // 2. High priority photo folders in user home
    fs::path pictures_dir = home / "Pictures";
    inspect_and_add(pictures_dir);
    scan_subdirs_recursive(pictures_dir, 3);

    fs::path wallpapers_dir = home / "Wallpapers";
    inspect_and_add(wallpapers_dir);
    scan_subdirs_recursive(wallpapers_dir, 2);

    fs::path dcim_dir = home / "DCIM";
    inspect_and_add(dcim_dir);
    scan_subdirs_recursive(dcim_dir, 3);

    fs::path photos_dir = home / "Photos";
    inspect_and_add(photos_dir);
    scan_subdirs_recursive(photos_dir, 3);

    fs::path downloads_dir = home / "Downloads";
    inspect_and_add(downloads_dir);
    scan_subdirs_recursive(downloads_dir, 2);

    fs::path desktop_dir = home / "Desktop";
    inspect_and_add(desktop_dir);

    // 3. Collect direct photos in user home directory (shown directly as files)
    std::vector<std::shared_ptr<FileItem>> home_photos;
    std::error_code ec_home;
    for (fs::directory_iterator it(home, fs::directory_options::skip_permission_denied, ec_home), end;
         !ec_home && it != end; ++it) {
        if (is_image_file(it->path())) {
            home_photos.push_back(create_photo_item(it->path()));
        }
    }

    // 4. Other subdirectories in home (1 level deep)
    std::error_code ec;
    for (fs::directory_iterator it(home, fs::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; ++it) {
        if (it->is_directory(ec)) {
            std::string dname = it->path().filename().string();
            if (!should_skip_dir(dname)) {
                inspect_and_add(it->path());
            }
        }
    }

    // Disambiguate duplicate folder titles
    for (size_t i = 0; i < tree.size(); ++i) {
        for (size_t j = i + 1; j < tree.size(); ++j) {
            if (tree[i]->title == tree[j]->title) {
                tree[i]->title = tree[i]->path.parent_path().filename().string() + "/" + tree[i]->title;
                tree[j]->title = tree[j]->path.parent_path().filename().string() + "/" + tree[j]->title;
            }
        }
    }

    // Sort: Pictures first, then alphabetically
    std::sort(tree.begin(), tree.end(), [&](const std::shared_ptr<FileItem>& a, const std::shared_ptr<FileItem>& b) {
        bool a_is_pics = (a->path == pictures_dir);
        bool b_is_pics = (b->path == pictures_dir);
        if (a_is_pics != b_is_pics) return a_is_pics > b_is_pics;
        return a->title < b->title;
    });

    // Sort direct home photos (newest first)
    std::sort(home_photos.begin(), home_photos.end(), [](const std::shared_ptr<FileItem>& a, const std::shared_ptr<FileItem>& b) {
        if (a->mtime_formatted != b->mtime_formatted) {
            return a->mtime_formatted > b->mtime_formatted;
        }
        return a->title < b->title;
    });

    // Append direct home photos to tree as individual files
    for (auto& p : home_photos) {
        tree.push_back(std::move(p));
    }

    // If no folders or photos found anywhere, fallback to ~/Pictures or Home
    if (tree.empty()) {
        fs::path fallback = fs::exists(pictures_dir) ? pictures_dir : home;
        tree.push_back(create_album_item(fallback, 0));
    }

    return tree;
}

} // namespace miqu
