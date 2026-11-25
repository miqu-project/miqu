#include "explorer_view.hpp"
#include "preview_common.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace miqu {

static constexpr int MATCH_PARENT = static_cast<int>(LayoutDimension::MatchParent);
static constexpr int WRAP_CONTENT = static_cast<int>(LayoutDimension::WrapContent);

namespace {

int get_dialog_btn_radius(const std::shared_ptr<Config>& config) {
    return (config && config->metrics.corner_radius > 0)
        ? std::max(4, config->metrics.corner_radius / 2)
        : 6;
}

std::string url_encode_path(const std::string& path) {
    std::ostringstream oss;
    oss << std::uppercase << std::hex;
    for (unsigned char c : path) {
        if (std::isalnum(c) || c == '/' || c == '-' || c == '_' || c == '.' || c == '~') {
            oss << c;
        } else {
            oss << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
        }
    }
    return oss.str();
}

std::string current_iso_timestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
    localtime_r(&tt, &tm_buf);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm_buf);
    return std::string(buf);
}

fs::path get_trash_dir() {
    if (const char* xdg = std::getenv("XDG_DATA_HOME")) {
        if (*xdg) return fs::path(xdg) / "Trash";
    }
    if (const char* home = std::getenv("HOME")) {
        if (*home) return fs::path(home) / ".local" / "share" / "Trash";
    }
    return {};
}

fs::path get_desktop_dir() {
    if (const char* xdg = std::getenv("XDG_DESKTOP_DIR")) {
        if (*xdg) return fs::path(xdg);
    }
    if (const char* home = std::getenv("HOME")) {
        if (*home) return fs::path(home) / "Desktop";
    }
    return {};
}

fs::path make_unique_path(const fs::path& dir, const std::string& base_name, const std::string& suffix_tag = "") {
    fs::path candidate = dir / base_name;
    std::error_code ec;
    if (!fs::exists(candidate, ec) && !fs::is_symlink(candidate, ec)) {
        return candidate;
    }

    fs::path p(base_name);
    std::string stem = p.stem().string();
    std::string ext = p.extension().string();
    for (int idx = 1; idx < 10000; ++idx) {
        std::string tag = suffix_tag.empty()
            ? (" (" + std::to_string(idx) + ")")
            : (idx == 1 ? (" (" + suffix_tag + ")") : (" (" + suffix_tag + " " + std::to_string(idx) + ")"));
        candidate = dir / (stem + tag + ext);
        if (!fs::exists(candidate, ec) && !fs::is_symlink(candidate, ec)) {
            return candidate;
        }
    }
    return candidate;
}

bool is_subpath_of(const fs::path& child, const fs::path& parent) {
    std::error_code ec;
    fs::path c = fs::weakly_canonical(child, ec);
    fs::path p = fs::weakly_canonical(parent, ec);
    if (ec) return false;
    auto c_it = c.begin();
    auto p_it = p.begin();
    for (; p_it != p.end(); ++p_it, ++c_it) {
        if (c_it == c.end() || *c_it != *p_it) return false;
    }
    return true;
}

std::string to_lower_ext(const fs::path& p) {
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
    return ext;
}

bool spawn_in_dir(const fs::path& dir, const std::vector<std::string>& argv) {
    if (argv.empty()) return false;
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        setsid();
        if (!dir.empty()) {
            (void)chdir(dir.c_str());
        }
        std::vector<char*> c_args;
        c_args.reserve(argv.size() + 1);
        for (const auto& s : argv) {
            c_args.push_back(const_cast<char*>(s.c_str()));
        }
        c_args.push_back(nullptr);
        execvp(c_args[0], c_args.data());
        _exit(127);
    }
    return true;
}

} // namespace

std::string ExplorerView::resolve_terminal_emulator() {
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

bool ExplorerView::open_terminal_in(const fs::path& dir) {
    std::string term = resolve_terminal_emulator();
    fs::path target = dir;
    std::error_code ec;
    if (!fs::exists(target, ec) || !fs::is_directory(target, ec)) {
        target = fs::current_path(ec);
    }
    return spawn_in_dir(target, {term});
}

bool ExplorerView::run_in_terminal(const fs::path& working_dir, const std::string& shell_command) {
    std::string term = resolve_terminal_emulator();
    std::string wrapped = shell_command + "; echo \"\"; printf 'Press Enter to close...'; read _";
    return spawn_in_dir(working_dir, {term, "-e", "sh", "-c", wrapped});
}

void ExplorerView::copy_text_to_clipboard(const std::string& text, const std::string& mime_type) {
    int pipefd[2];
    if (pipe(pipefd) != 0) return;

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return;
    }
    if (pid == 0) {
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        setsid();
        execlp("wl-copy", "wl-copy", "--type", mime_type.c_str(), nullptr);
        _exit(127);
    }

    close(pipefd[0]);
    size_t written = 0;
    while (written < text.size()) {
        ssize_t n = write(pipefd[1], text.data() + written, text.size() - written);
        if (n <= 0) break;
        written += static_cast<size_t>(n);
    }
    close(pipefd[1]);
    (void)waitpid(pid, nullptr, WNOHANG);
}

void ExplorerView::setup_context_menus() {
    auto on_item_ctx = [this](size_t idx, std::shared_ptr<View>, int lx, int ly) {
        if (idx >= m_displayed_items.size()) return;
        size_t mark_count = m_selection_model ? m_selection_model->marks().size() : 0;
        if (mark_count <= 1 && static_cast<int>(idx) != get_cursor_index()) {
            set_cursor_index(static_cast<int>(idx));
            update_status_bar();
            rebuild_preview_pane();
        }
        show_item_context_menu(m_displayed_items[idx], lx, ly);
    };

    auto on_empty_ctx = [this](int lx, int ly) {
        show_empty_context_menu(m_current_dir, lx, ly);
    };

    if (m_grid_view) {
        m_grid_view->set_on_item_context_click_listener(on_item_ctx);
        m_grid_view->set_on_empty_context_click_listener(on_empty_ctx);
    }

    if (m_list_view) {
        m_list_view->set_on_item_context_click_listener(on_item_ctx);
        m_list_view->set_on_empty_context_click_listener(on_empty_ctx);
    }

    if (m_content_pillar) {
        m_content_pillar->set_context_menu_builder([this](int lx, int ly) -> std::shared_ptr<PopupMenu> {
            show_empty_context_menu(m_current_dir, lx, ly);
            return nullptr;
        });
    }
}

void ExplorerView::reload_current_directory(const std::string& select_path) {
    if (m_current_dir.empty()) return;
    std::string target_path = select_path;
    if (target_path.empty()) {
        auto cur = get_preview_target_item();
        if (cur) target_path = cur->path.string();
    }

    m_all_items = FilesModel::scan_directory(m_current_dir, m_show_hidden);
    if (m_selection_model) {
        m_selection_model->clear_marks();
    }
    apply_sort_and_filter(target_path);
    sync_sidebar_selection();
    notify_state_changed();
}

void ExplorerView::show_item_context_menu(const std::shared_ptr<FileItem>& item, int lx, int ly) {
    if (!item || !m_window) return;

    auto builder = ActionMenuBuilder::create()
        ->minWidth(220)
        ->maxVisibleItems(18)
        ->itemHeight(34);

    size_t mark_count = m_selection_model ? m_selection_model->marks().size() : 0;
    if (mark_count > 1) {
        auto selected = get_selected_or_cursor_items();
        size_t count = selected.size();
        std::string count_str = std::to_string(count) + " items";

        builder->section(count_str + " selected")
            ->item("Copy (" + count_str + ")", "edit-copy", [this, selected]() {
                action_copy_items(selected, false);
            })
            ->item("Cut (" + count_str + ")", "edit-cut", [this, selected]() {
                action_copy_items(selected, true);
            })
            ->item("Copy Paths", "edit-copy", [selected]() {
                std::string joined;
                for (size_t i = 0; i < selected.size(); ++i) {
                    if (i > 0) joined += '\n';
                    joined += selected[i]->path.string();
                }
                copy_text_to_clipboard(joined);
            })
            ->separator();

        auto menu = builder->build();
        MenuItem send_to("Send To", "document-send", nullptr);
        send_to.set_enabled(false);
        menu->add_item(send_to);
        menu->add_separator();
        menu->add_item("Move to Trash (" + count_str + ")", "user-trash", [this, selected]() {
            action_trash_items(selected);
        });
        menu->add_destructive_item("Delete Permanently (" + count_str + ")", "edit-delete", [this, selected]() {
            show_confirm_dialog(
                "Delete Permanently?",
                "Permanently delete " + std::to_string(selected.size()) + " selected items? This cannot be undone.",
                "Delete", true, [this, selected]() {
                    action_delete_items(selected);
                });
        });

        m_context_menu = menu;
        m_context_menu->show_at_location(m_window.get(), lx, ly);
        return;
    }

    // Single item (file or folder)
    std::vector<std::shared_ptr<FileItem>> single_vec = {item};

    if (item->is_dir) {
        builder->item("Open", "folder-open", [this, item]() {
            navigate_to_dir(item->path, true);
        });
    } else {
        builder->item("Open", "document-open", [this, item]() {
            if (!item->path.empty()) {
                FileOpener::open_or_show_menu(item->path, shared_from_this(), m_open_with_menu);
            }
        });
        builder->item("Open With...", "system-run", [this, item, lx, ly]() {
            if (item->path.empty() || !m_window) return;
            m_open_with_menu = FileOpener::build_open_with_menu(item->path);
            m_open_with_menu->show_at_location(m_window.get(), lx, ly);
        });
    }

    if (!item->is_dir && to_lower_ext(item->path) == ".py") {
        builder->item("Run in Python", "media-playback-start", [item]() {
            if (item->path.empty()) return;
            std::string quoted = "'";
            for (char c : item->path.string()) {
                if (c == '\'') quoted += "'\\''";
                else quoted += c;
            }
            quoted += "'";
            run_in_terminal(item->path.parent_path(), "python3 " + quoted);
        });
    }

    if (!item->is_dir && item->category == FileCategory::Image) {
        builder->item("Set as Wallpaper", "preferences-desktop-wallpaper", [this, item]() {
            action_set_as_wallpaper(item);
        });
    }

    builder->separator()
        ->item("Cut", "edit-cut", [this, single_vec]() {
            action_copy_items(single_vec, true);
        })
        ->item("Copy", "edit-copy", [this, single_vec]() {
            action_copy_items(single_vec, false);
        })
        ->item("Copy as Path", "edit-copy", [item]() {
            copy_text_to_clipboard(item->path.string());
        });

    if (!m_clipboard.paths.empty() && !m_current_dir.empty()) {
        std::string paste_label = "Paste (" + std::to_string(m_clipboard.paths.size()) + ") Here";
        builder->item(paste_label, "edit-paste", [this]() {
            action_paste_into(m_current_dir);
        });
    }

    if (item->is_dir && !m_clipboard.paths.empty()) {
        builder->item("Paste Into Folder", "edit-paste", [this, item]() {
            action_paste_into(item->path);
        });
    }

    builder->item("Rename...", "edit-rename", [this, item]() {
        show_text_input_dialog("Rename", item->title, "Rename", [this, item](const std::string& new_name) {
            if (new_name.empty() || new_name == item->title || new_name.find('/') != std::string::npos) return;
            fs::path dst = item->path.parent_path() / new_name;
            std::error_code ec;
            if (fs::exists(dst, ec)) return;
            fs::rename(item->path, dst, ec);
            if (!ec) {
                reload_current_directory(dst.string());
            }
        });
    });

    builder->separator()
        ->item("Open Terminal Here", "utilities-terminal", [item]() {
            fs::path target_dir = item->is_dir ? item->path : item->path.parent_path();
            open_terminal_in(target_dir);
        })
        ->item("Add to Desktop (Shortcut)", "emblem-symbolic-link", [this, item]() {
            action_add_to_desktop(item);
        });

    auto menu = builder->build();

    MenuItem send_to("Send To", "document-send", nullptr);
    send_to.set_enabled(false);
    menu->add_item(send_to);

    menu->add_separator();
    menu->add_item("Move to Trash", "user-trash", [this, single_vec]() {
        action_trash_items(single_vec);
    });
    menu->add_destructive_item("Delete Permanently", "edit-delete", [this, item, single_vec]() {
        show_confirm_dialog(
            "Delete Permanently?",
            "Permanently delete \"" + item->title + "\"? This cannot be undone.",
            "Delete", true, [this, single_vec]() {
                action_delete_items(single_vec);
            });
    });
    menu->add_separator();
    menu->add_item("Properties", "dialog-information", [this, item, lx, ly]() {
        show_file_info_popup(item, lx, ly);
    });

    m_context_menu = menu;
    m_context_menu->show_at_location(m_window.get(), lx, ly);
}

void ExplorerView::show_empty_context_menu(const fs::path& dir, int lx, int ly) {
    if (!m_window) return;

    std::error_code ec;
    bool has_dir = !dir.empty() && fs::exists(dir, ec) && fs::is_directory(dir, ec);

    auto builder = ActionMenuBuilder::create()
        ->minWidth(210)
        ->maxVisibleItems(14)
        ->itemHeight(34);

    if (has_dir) {
        builder->item("New Folder...", "folder-new", [this, dir]() {
            show_text_input_dialog("New Folder", "New Folder", "Create", [this, dir](const std::string& name) {
                if (name.empty() || name.find('/') != std::string::npos) return;
                fs::path target = dir / name;
                std::error_code create_ec;
                if (fs::create_directory(target, create_ec)) {
                    reload_current_directory(target.string());
                }
            });
        })
        ->item("New File...", "document-new", [this, dir, lx, ly]() {
            show_new_file_menu(dir, lx, ly);
        });

        if (!m_clipboard.paths.empty()) {
            std::string paste_label = "Paste (" + std::to_string(m_clipboard.paths.size()) + ")";
            builder->item(paste_label, "edit-paste", [this, dir]() {
                action_paste_into(dir);
            });
        }

        builder->separator()
            ->item("Open Terminal Here", "utilities-terminal", [dir]() {
                open_terminal_in(dir);
            })
            ->item("Copy Folder Path", "edit-copy", [dir]() {
                copy_text_to_clipboard(dir.string());
            });
    }

    builder->separator()
        ->checkable("Show Hidden Files", m_show_hidden, [this](bool checked) {
            set_show_hidden(checked);
        })
        ->item("Refresh", "view-refresh", [this]() {
            reload_current_directory();
        });

    if (has_dir) {
        builder->separator()
            ->item("Properties", "dialog-information", [this, dir, lx, ly]() {
                auto dir_item = FilesModel::create_file_item(dir);
                if (dir_item) {
                    show_file_info_popup(dir_item, lx, ly);
                }
            });
    }

    m_context_menu = builder->build();
    m_context_menu->show_at_location(m_window.get(), lx, ly);
}

void ExplorerView::show_new_file_menu(const fs::path& dir, int lx, int ly) {
    if (!m_window || dir.empty()) return;

    struct FileTemplate {
        const char* label;
        const char* icon;
        const char* default_name;
        const char* initial_content;
    };

    static constexpr FileTemplate templates[] = {
        {"Plain Text (.txt)",   "text-x-generic",  "untitled.txt",  ""},
        {"Python Script (.py)", "text-x-python",   "script.py",     "#!/usr/bin/env python3\n\n"},
        {"C++ Source (.cpp)",   "text-x-c++src",   "main.cpp",      "#include <iostream>\n\nint main() {\n    return 0;\n}\n"},
        {"C++ Header (.hpp)",   "text-x-c++hdr",   "header.hpp",    "#pragma once\n\n"},
        {"Shell Script (.sh)",  "text-x-script",   "script.sh",     "#!/usr/bin/env bash\n\n"},
        {"Markdown (.md)",      "text-markdown",   "README.md",     "# Title\n\n"},
        {"JSON (.json)",        "application-json","data.json",     "{\n}\n"},
        {"Custom File...",      "document-new",    "newfile",       ""},
    };

    auto builder = ActionMenuBuilder::create()
        ->minWidth(210)
        ->maxVisibleItems(10)
        ->itemHeight(34)
        ->section("New File");

    for (const auto& tmpl : templates) {
        std::string label = tmpl.label;
        std::string icon = tmpl.icon;
        std::string def_name = tmpl.default_name;
        std::string content = tmpl.initial_content;
        builder->item(label, icon, [this, dir, def_name, content]() {
            show_text_input_dialog("New File", def_name, "Create", [this, dir, content](const std::string& name) {
                if (name.empty() || name.find('/') != std::string::npos) return;
                fs::path target = dir / name;
                std::error_code ec;
                if (fs::exists(target, ec)) return;
                std::ofstream ofs(target);
                if (ofs.is_open()) {
                    if (!content.empty()) ofs << content;
                    ofs.close();
                    reload_current_directory(target.string());
                }
            });
        });
    }

    m_context_menu = builder->build();
    m_context_menu->show_at_location(m_window.get(), lx, ly);
}

void ExplorerView::show_text_input_dialog(const std::string& title, const std::string& initial_value,
                                        const std::string& confirm_label, std::function<void(const std::string&)> on_confirm) {
    auto config = Config::get();
    if (!m_window || !config) return;

    if (m_dialog_popup && m_dialog_popup->is_showing()) {
        m_dialog_popup->dismiss();
        m_dialog_popup = nullptr;
    }

    int card_rad = get_card_radius(config);
    int btn_rad = get_dialog_btn_radius(config);

    auto container = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(18, 16)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.35f))
        ->cornerRadius(card_rad)
        ->build();

    auto title_view = TextViewBuilder::create()
        ->text(title)
        ->bold(true)
        ->textSize(static_cast<int>(config->metrics.font_size * 1.05f))
        ->textColor(config->colors.on_surface)
        ->build();
    container->add_view(title_view);

    auto input = EditTextBuilder::create()
        ->text(initial_value)
        ->multiline(false)
        ->padding(10, 8)
        ->backgroundColor(config->colors.surface_variant.with_alpha(0.45f))
        ->textColor(config->colors.on_surface)
        ->build();
    container->add_view(input, LayoutParams(300, WRAP_CONTENT));

    auto btn_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(8)
        ->gravity(Gravity::End | Gravity::CenterVertical)
        ->build();

    auto cancel_btn = ButtonBuilder::create()
        ->text("Cancel")
        ->flat(true)
        ->padding(12, 6)
        ->cornerRadius(btn_rad)
        ->onClick([this]() {
            if (m_dialog_popup) {
                m_dialog_popup->dismiss();
                m_dialog_popup = nullptr;
            }
        })
        ->build();

    auto ok_btn = ButtonBuilder::create()
        ->text(confirm_label)
        ->padding(14, 6)
        ->cornerRadius(btn_rad)
        ->onClick([this, input, on_confirm = std::move(on_confirm)]() {
            std::string val = input->get_text();
            if (m_dialog_popup) {
                m_dialog_popup->dismiss();
                m_dialog_popup = nullptr;
            }
            if (on_confirm) on_confirm(val);
        })
        ->build();
    ok_btn->set_background_color(config->colors.primary);
    ok_btn->set_text_color(config->colors.on_primary);

    btn_row->add_view(cancel_btn, LayoutParams(WRAP_CONTENT, WRAP_CONTENT));
    btn_row->add_view(ok_btn, LayoutParams(WRAP_CONTENT, WRAP_CONTENT));
    container->add_view(btn_row, LayoutParams(MATCH_PARENT, WRAP_CONTENT));

    m_dialog_popup = PopupWindowBuilder::create()
        ->content(container)
        ->width(340)
        ->elevation(6)
        ->cornerRadius(card_rad)
        ->dismissOnOutsideClick(true)
        ->dismissOnEscape(true)
        ->onDismiss([this]() {
            m_dialog_popup = nullptr;
        })
        ->build();

    int px = std::max(16, (m_window->get_width() - 340) / 2);
    int py = std::max(16, (m_window->get_height() - 150) / 2);
    m_dialog_popup->show_at_location(m_window.get(), px, py);
    input->set_focused(true);
}

void ExplorerView::show_confirm_dialog(const std::string& title, const std::string& message,
                                     const std::string& confirm_label, bool destructive, std::function<void()> on_confirm) {
    auto config = Config::get();
    if (!m_window || !config) return;

    if (m_dialog_popup && m_dialog_popup->is_showing()) {
        m_dialog_popup->dismiss();
        m_dialog_popup = nullptr;
    }

    int card_rad = get_card_radius(config);
    int btn_rad = get_dialog_btn_radius(config);

    auto container = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(18, 16)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.35f))
        ->cornerRadius(card_rad)
        ->build();

    auto title_view = TextViewBuilder::create()
        ->text(title)
        ->bold(true)
        ->textSize(static_cast<int>(config->metrics.font_size * 1.05f))
        ->textColor(destructive ? config->colors.primary : config->colors.on_surface)
        ->build();
    container->add_view(title_view);

    auto msg_view = TextViewBuilder::create()
        ->text(message)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    container->add_view(msg_view, LayoutParams(310, WRAP_CONTENT));

    auto btn_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(8)
        ->gravity(Gravity::End | Gravity::CenterVertical)
        ->build();

    auto cancel_btn = ButtonBuilder::create()
        ->text("Cancel")
        ->flat(true)
        ->padding(12, 6)
        ->cornerRadius(btn_rad)
        ->onClick([this]() {
            if (m_dialog_popup) {
                m_dialog_popup->dismiss();
                m_dialog_popup = nullptr;
            }
        })
        ->build();

    auto ok_btn = ButtonBuilder::create()
        ->text(confirm_label)
        ->padding(14, 6)
        ->cornerRadius(btn_rad)
        ->onClick([this, on_confirm = std::move(on_confirm)]() {
            if (m_dialog_popup) {
                m_dialog_popup->dismiss();
                m_dialog_popup = nullptr;
            }
            if (on_confirm) on_confirm();
        })
        ->build();
    ok_btn->set_background_color(config->colors.primary);
    ok_btn->set_text_color(config->colors.on_primary);

    btn_row->add_view(cancel_btn, LayoutParams(WRAP_CONTENT, WRAP_CONTENT));
    btn_row->add_view(ok_btn, LayoutParams(WRAP_CONTENT, WRAP_CONTENT));
    container->add_view(btn_row, LayoutParams(MATCH_PARENT, WRAP_CONTENT));

    m_dialog_popup = PopupWindowBuilder::create()
        ->content(container)
        ->width(350)
        ->elevation(6)
        ->cornerRadius(card_rad)
        ->dismissOnOutsideClick(true)
        ->dismissOnEscape(true)
        ->onDismiss([this]() {
            m_dialog_popup = nullptr;
        })
        ->build();

    int px = std::max(16, (m_window->get_width() - 350) / 2);
    int py = std::max(16, (m_window->get_height() - 150) / 2);
    m_dialog_popup->show_at_location(m_window.get(), px, py);
}

void ExplorerView::action_copy_items(const std::vector<std::shared_ptr<FileItem>>& items, bool is_cut) {
    m_clipboard.paths.clear();
    m_clipboard.is_cut = is_cut;

    std::string uri_list;
    for (const auto& item : items) {
        if (!item || item->path.empty()) continue;
        m_clipboard.paths.push_back(item->path);
        uri_list += "file://" + url_encode_path(item->path.string()) + "\r\n";
    }

    if (!uri_list.empty()) {
        copy_text_to_clipboard(uri_list, "text/uri-list");
    }
}

void ExplorerView::action_paste_into(const fs::path& target_dir) {
    if (m_clipboard.paths.empty() || target_dir.empty()) return;

    std::error_code ec;
    if (!fs::exists(target_dir, ec) || !fs::is_directory(target_dir, ec)) return;

    std::string last_pasted;
    for (const auto& src : m_clipboard.paths) {
        if (!fs::exists(src, ec) && !fs::is_symlink(src, ec)) continue;
        if (fs::is_directory(src, ec) && is_subpath_of(target_dir, src)) continue;

        fs::path dst = make_unique_path(target_dir, src.filename().string(), m_clipboard.is_cut ? "" : "copy");
        if (m_clipboard.is_cut) {
            fs::rename(src, dst, ec);
            if (ec) {
                ec.clear();
                fs::copy(src, dst, fs::copy_options::recursive | fs::copy_options::copy_symlinks, ec);
                if (!ec) {
                    fs::remove_all(src, ec);
                }
            }
        } else {
            fs::copy(src, dst, fs::copy_options::recursive | fs::copy_options::copy_symlinks, ec);
        }

        if (!ec) {
            last_pasted = dst.string();
        }
    }

    if (m_clipboard.is_cut) {
        m_clipboard.paths.clear();
        m_clipboard.is_cut = false;
    }

    reload_current_directory(last_pasted);
}

void ExplorerView::action_trash_items(const std::vector<std::shared_ptr<FileItem>>& items) {
    fs::path trash_root = get_trash_dir();
    if (trash_root.empty()) return;

    fs::path files_dir = trash_root / "files";
    fs::path info_dir = trash_root / "info";
    std::error_code ec;
    fs::create_directories(files_dir, ec);
    fs::create_directories(info_dir, ec);
    if (ec) return;

    std::string ts = current_iso_timestamp();
    for (const auto& item : items) {
        if (!item || item->path.empty()) continue;
        fs::path dst = make_unique_path(files_dir, item->path.filename().string());
        fs::path info_file = info_dir / (dst.filename().string() + ".trashinfo");

        {
            std::ofstream ofs(info_file);
            if (ofs.is_open()) {
                ofs << "[Trash Info]\n"
                    << "Path=" << url_encode_path(item->path.string()) << "\n"
                    << "DeletionDate=" << ts << "\n";
            }
        }

        fs::rename(item->path, dst, ec);
        if (ec) {
            ec.clear();
            fs::copy(item->path, dst, fs::copy_options::recursive | fs::copy_options::copy_symlinks, ec);
            if (!ec) {
                fs::remove_all(item->path, ec);
            }
            if (ec) {
                fs::remove(info_file, ec);
            }
        }
    }

    reload_current_directory();
}

void ExplorerView::action_delete_items(const std::vector<std::shared_ptr<FileItem>>& items) {
    std::error_code ec;
    for (const auto& item : items) {
        if (!item || item->path.empty()) continue;
        fs::remove_all(item->path, ec);
    }
    reload_current_directory();
}

void ExplorerView::action_add_to_desktop(const std::shared_ptr<FileItem>& item) {
    if (!item || item->path.empty()) return;
    fs::path desktop = get_desktop_dir();
    if (desktop.empty()) return;

    std::error_code ec;
    fs::create_directories(desktop, ec);
    if (ec) return;

    fs::path link_path = make_unique_path(desktop, item->path.filename().string(), "shortcut");
    fs::create_symlink(item->path, link_path, ec);
}

void ExplorerView::action_set_as_wallpaper(const std::shared_ptr<FileItem>& item) {
    if (!item || item->path.empty()) return;
    FileOpener::spawn_detached({"miqubg", "img", item->path.string()});
}

} // namespace miqu
