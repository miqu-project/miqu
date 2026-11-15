#include "files_model.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace miqu {

std::string FilesModel::format_file_size(uintmax_t bytes) {
    if (bytes < 1024) {
        return std::to_string(bytes) + " B";
    }
    const char* units[] = {"KB", "MB", "GB", "TB"};
    double size = static_cast<double>(bytes) / 1024.0;
    int unit_idx = 0;
    while (size >= 1024.0 && unit_idx < 3) {
        size /= 1024.0;
        unit_idx++;
    }
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << size << " " << units[unit_idx];
    return ss.str();
}

std::string FilesModel::format_permissions(fs::perms p) {
    std::string s;
    s.reserve(9);
    s += (p & fs::perms::owner_read) != fs::perms::none ? "r" : "-";
    s += (p & fs::perms::owner_write) != fs::perms::none ? "w" : "-";
    s += (p & fs::perms::owner_exec) != fs::perms::none ? "x" : "-";
    s += (p & fs::perms::group_read) != fs::perms::none ? "r" : "-";
    s += (p & fs::perms::group_write) != fs::perms::none ? "w" : "-";
    s += (p & fs::perms::group_exec) != fs::perms::none ? "x" : "-";
    s += (p & fs::perms::others_read) != fs::perms::none ? "r" : "-";
    s += (p & fs::perms::others_write) != fs::perms::none ? "w" : "-";
    s += (p & fs::perms::others_exec) != fs::perms::none ? "x" : "-";
    return s;
}

std::string FilesModel::format_time(fs::file_time_type ftime) {
    try {
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
        std::tm tm_buf;
        if (localtime_r(&tt, &tm_buf)) {
            char buf[64];
            if (std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm_buf)) {
                return std::string(buf);
            }
        }
    } catch (...) {}
    return "Unknown";
}

FileCategory FilesModel::detect_category(const fs::path& path, bool is_dir) {
    if (is_dir) return FileCategory::Directory;

    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".webp" ||
        ext == ".svg" || ext == ".bmp" || ext == ".gif"  || ext == ".ico"  ||
        ext == ".avif" || ext == ".tiff") {
        return FileCategory::Image;
    }

    if (ext == ".mp4" || ext == ".mkv" || ext == ".webm" || ext == ".avi" ||
        ext == ".mov" || ext == ".flv" || ext == ".wmv") {
        return FileCategory::Video;
    }

    if (ext == ".mp3" || ext == ".flac" || ext == ".wav" || ext == ".ogg" ||
        ext == ".m4a" || ext == ".aac"  || ext == ".opus") {
        return FileCategory::Audio;
    }

    if (ext == ".pdf" || ext == ".epub" || ext == ".doc" || ext == ".docx" ||
        ext == ".odt" || ext == ".xls"  || ext == ".xlsx"|| ext == ".ppt"  ||
        ext == ".pptx") {
        return FileCategory::Document;
    }

    if (ext == ".zip" || ext == ".tar" || ext == ".gz"  || ext == ".xz"  ||
        ext == ".bz2" || ext == ".7z"  || ext == ".rar" || ext == ".zst") {
        return FileCategory::Archive;
    }

    if (ext == ".txt" || ext == ".md"   || ext == ".cpp" || ext == ".hpp" ||
        ext == ".c"   || ext == ".h"    || ext == ".py"  || ext == ".rs"   ||
        ext == ".js"  || ext == ".ts"   || ext == ".json"|| ext == ".yaml" ||
        ext == ".yml" || ext == ".toml" || ext == ".sh"  || ext == ".bash" ||
        ext == ".zsh" || ext == ".conf" || ext == ".ini" || ext == ".xml"  ||
        ext == ".html"|| ext == ".css"  || ext == ".scss"|| ext == ".go"   ||
        ext == ".java"|| ext == ".lua"  || ext == ".csv" || ext == ".patch"||
        ext == ".diff"|| ext == ".service" || ext == ".desktop") {
        return FileCategory::CodeText;
    }

    std::error_code ec;
    auto perms = fs::status(path, ec).permissions();
    if (!ec && (perms & (fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec)) != fs::perms::none) {
        return FileCategory::Executable;
    }

    return FileCategory::Other;
}

std::string FilesModel::get_icon_for_category(FileCategory cat, const std::string& filename) {
    if (cat == FileCategory::Directory) {
        if (filename == "Downloads") return "folder-download";
        if (filename == "Documents") return "folder-documents";
        if (filename == "Pictures") return "folder-pictures";
        if (filename == "Music") return "folder-music";
        if (filename == "Videos") return "folder-videos";
        if (filename == "Desktop") return "user-desktop";
        if (filename == "Home" || filename == "~") return "user-home";
        if (filename == "/" || filename == "File System") return "drive-harddisk";
        if (filename == ".git") return "folder-git";
        return "folder";
    }

    std::string ext = "";
    auto dot_pos = filename.rfind('.');
    if (dot_pos != std::string::npos) {
        ext = filename.substr(dot_pos);
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
    }

    std::string lower_fname = filename;
    std::transform(lower_fname.begin(), lower_fname.end(), lower_fname.begin(), [](unsigned char c) { return std::tolower(c); });

    switch (cat) {
        case FileCategory::Image:
            return "image-x-generic";

        case FileCategory::Video:
            return "video-x-generic";

        case FileCategory::Audio:
            return "audio-x-generic";

        case FileCategory::Document:
            if (ext == ".pdf") return "application-pdf";
            if (ext == ".epub") return "application-epub+zip";
            if (ext == ".xls" || ext == ".xlsx" || ext == ".ods" || ext == ".csv") return "x-office-spreadsheet";
            if (ext == ".ppt" || ext == ".pptx" || ext == ".odp") return "x-office-presentation";
            return "x-office-document";

        case FileCategory::Archive:
            if (lower_fname.ends_with(".tar.gz") || lower_fname.ends_with(".tar.xz") ||
                lower_fname.ends_with(".tar.bz2") || lower_fname.ends_with(".tar.zst") ||
                ext == ".tar" || ext == ".tgz") {
                return "application-x-tar";
            }
            if (ext == ".zip") return "application-zip";
            if (ext == ".7z" || ext == ".rar") return "application-x-7z-compressed";
            if (ext == ".gz" || ext == ".xz" || ext == ".bz2" || ext == ".zst") return "application-x-compressed-tar";
            return "package-x-generic";

        case FileCategory::CodeText:
            if (ext == ".py") return "text-x-python";
            if (ext == ".cpp" || ext == ".cxx" || ext == ".cc") return "text-x-c++src";
            if (ext == ".hpp" || ext == ".hxx" || ext == ".hh") return "text-x-c++hdr";
            if (ext == ".c") return "text-x-csrc";
            if (ext == ".h") return "text-x-chdr";
            if (ext == ".rs") return "text-x-rust";
            if (ext == ".js" || ext == ".ts" || ext == ".mjs" || ext == ".cjs" || ext == ".tsx") return "text-x-javascript";
            if (ext == ".json") return "application-json";
            if (ext == ".html" || ext == ".htm") return "text-html";
            if (ext == ".css" || ext == ".scss" || ext == ".sass" || ext == ".less") return "text-css";
            if (ext == ".sh" || ext == ".bash" || ext == ".zsh" || ext == ".fish") return "application-x-shellscript";
            if (ext == ".lua") return "text-x-lua";
            if (ext == ".go") return "text-x-go";
            if (ext == ".java") return "text-x-java";
            if (ext == ".md" || ext == ".markdown") return "text-markdown";
            if (ext == ".txt") return "text-plain";
            if (ext == ".xml") return "application-xml";
            if (ext == ".patch" || ext == ".diff") return "text-x-patch";
            if (ext == ".service") return "system-run";
            if (ext == ".desktop") return "application-x-desktop";
            return "text-x-generic";

        case FileCategory::Executable:
            return "application-x-executable";

        case FileCategory::Other:
        default:
            return "text-x-generic";
    }
}

void FileItem::reload_children(bool show_hidden) {
    if (!is_dir || path.empty()) return;

    if (provider == nullptr) {
        children = FilesModel::scan_directory(path, show_hidden);
        child_count = children.size();
        size_formatted = std::to_string(child_count) + (child_count == 1 ? " item" : " items");
        subtitle = size_formatted;
    } else {
        auto p = path;
        provider = [p, show_hidden](FileItem* self) {
            self->children = FilesModel::scan_directory(p, show_hidden);
            self->child_count = self->children.size();
            self->size_formatted = std::to_string(self->child_count) + (self->child_count == 1 ? " item" : " items");
            self->subtitle = self->size_formatted;
        };
    }
}

std::shared_ptr<FileItem> FilesModel::create_directory_item(const fs::path& dir,
                                                           const std::string& custom_title,
                                                           const std::string& custom_icon,
                                                           bool show_hidden) {
    auto item = std::make_shared<FileItem>();
    item->path = dir;
    item->id = dir.string();
    item->is_dir = true;
    item->is_category = true;
    item->category = FileCategory::Directory;
    item->category_label = "Folder";

    std::string fname = custom_title.empty() ? dir.filename().string() : custom_title;
    if (fname.empty()) fname = dir.string();
    item->title = fname;
    item->is_hidden = (!item->title.empty() && item->title[0] == '.');
    item->icon = custom_icon.empty() ? get_icon_for_category(FileCategory::Directory, fname) : custom_icon;

    std::error_code ec;
    auto status = fs::symlink_status(dir, ec);
    if (!ec) {
        item->is_symlink = fs::is_symlink(status);
        item->permissions_str = "d" + format_permissions(status.permissions());
        auto mtime = fs::last_write_time(dir, ec);
        if (!ec) {
            item->mtime_formatted = format_time(mtime);
        }
    }

    item->provider = [dir, show_hidden](FileItem* self) {
        self->children = FilesModel::scan_directory(dir, show_hidden);
        self->child_count = self->children.size();
        self->size_formatted = std::to_string(self->child_count) + (self->child_count == 1 ? " item" : " items");
        self->subtitle = self->size_formatted;
    };

    return item;
}

std::shared_ptr<FileItem> FilesModel::create_file_item(const fs::path& p) {
    auto item = std::make_shared<FileItem>();
    item->path = p;
    item->id = p.string();
    item->title = p.filename().string();
    if (item->title.empty()) item->title = p.string();
    item->is_hidden = (!item->title.empty() && item->title[0] == '.');
    item->is_dir = false;
    item->is_category = false;

    std::error_code ec;
    auto status = fs::symlink_status(p, ec);
    if (ec) {
        item->size_formatted = "Unknown";
        item->icon = "text-x-generic";
        return item;
    }

    item->is_symlink = fs::is_symlink(status);
    if (item->is_symlink) {
        auto target = fs::read_symlink(p, ec);
        if (!ec) item->symlink_target = target.string();
    }

    item->category = detect_category(p, false);
    item->icon = get_icon_for_category(item->category, item->title);

    switch (item->category) {
        case FileCategory::Image:      item->category_label = "Image"; break;
        case FileCategory::Video:      item->category_label = "Video"; break;
        case FileCategory::Audio:      item->category_label = "Audio"; break;
        case FileCategory::Document:   item->category_label = "Document"; break;
        case FileCategory::CodeText:   item->category_label = "Code/Text"; break;
        case FileCategory::Archive:    item->category_label = "Archive"; break;
        case FileCategory::Executable: item->category_label = "Executable"; break;
        case FileCategory::Other:
        default:                       item->category_label = "File"; break;
    }

    item->permissions_str = (item->is_symlink ? "l" : "-") + format_permissions(status.permissions());

    auto mtime = fs::last_write_time(p, ec);
    if (!ec) {
        item->mtime_formatted = format_time(mtime);
    }

    item->size_bytes = fs::file_size(p, ec);
    if (!ec) {
        item->size_formatted = format_file_size(item->size_bytes);
    } else {
        item->size_formatted = "-";
    }

    item->subtitle = item->size_formatted + " • " + item->category_label;
    return item;
}

std::vector<std::shared_ptr<FileItem>> FilesModel::scan_directory(const fs::path& dir, bool show_hidden) {
    std::vector<std::shared_ptr<FileItem>> items;
    std::error_code ec;

    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
        return items;
    }

    for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; ++it) {
        std::string fname = it->path().filename().string();
        if (!show_hidden && !fname.empty() && fname[0] == '.') {
            continue;
        }

        std::error_code is_dir_ec;
        bool is_sub_dir = fs::is_directory(it->path(), is_dir_ec);
        if (is_sub_dir) {
            items.push_back(create_directory_item(it->path(), "", "", show_hidden));
        } else {
            items.push_back(create_file_item(it->path()));
        }
    }

    // Sort: Folders first, then alphabetically case-insensitive
    std::sort(items.begin(), items.end(), [](const std::shared_ptr<FileItem>& a, const std::shared_ptr<FileItem>& b) {
        if (a->is_dir != b->is_dir) {
            return a->is_dir > b->is_dir; // Folders first
        }
        std::string a_lower = a->title;
        std::string b_lower = b->title;
        std::transform(a_lower.begin(), a_lower.end(), a_lower.begin(), [](unsigned char c) { return std::tolower(c); });
        std::transform(b_lower.begin(), b_lower.end(), b_lower.begin(), [](unsigned char c) { return std::tolower(c); });
        return a_lower < b_lower;
    });

    return items;
}

std::vector<std::shared_ptr<FileItem>> FilesModel::build_default_tree(const std::string& start_path, bool show_hidden) {
    (void)start_path;
    return scan_directory("/", show_hidden);
}

} // namespace miqu
