#include "notepad_app.hpp"
#include "portal_file_chooser.hpp"
#include <xkbcommon/xkbcommon-keysyms.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <thread>

namespace miqu {

namespace fs = std::filesystem;

NotepadApp::NotepadApp(std::shared_ptr<AppEngine> engine, const std::string& initial_file)
    : m_engine(std::move(engine)), m_initial_file_path(initial_file) {}

bool NotepadApp::init() {
    if (!m_engine) return false;

    // 1. Resolve canonical configuration path
    std::string user_conf = FsUtils::get_user_config_file("miqunotepad");
    std::string target_conf = fs::exists(user_conf) ? user_conf : FsUtils::get_system_config_file("miqunotepad");

    // Load custom application options
    load_config_options(target_conf);

    // 2. Setup user interface
    setup_ui();

    // 3. Build window
    m_window = WindowBuilder::create()
        ->title("miqunotepad")
        ->appId("miqunotepad")
        ->role(WindowRole::Toplevel)
        ->preferredSize(m_app_config.window_width, m_app_config.window_height)
        ->keyboardInteractive(true)
        ->contentView(m_root_layout)
        ->onClose([this]() {
            if (m_engine) m_engine->quit();
        })
        ->onShortcut([this](const KeyPressEvent& ev) -> bool {
            return handle_key_press(ev);
        })
        ->onKey([this](const KeyPressEvent& ev) {
            handle_key_press(ev);
        })
        ->build();

    if (!m_window) {
        std::cerr << "[miqunotepad] Failed to create window.\n";
        return false;
    }

    m_engine->register_window(m_window);

    // Initial file load or blank document
    if (!m_initial_file_path.empty()) {
        std::string expanded = FsUtils::expand_user_path(m_initial_file_path);
        if (fs::exists(expanded)) {
            open_file(expanded);
        } else {
            m_current_file_path = expanded;
            m_editor->clear();
            m_is_modified = false;
            update_title_and_stats();
            update_status("New file: " + expanded);
        }
    } else {
        new_document();
    }

    sync_theme();
    m_editor->set_focused(true);

    return true;
}

void NotepadApp::load_config_options(const std::string& conf_path) {
    if (conf_path.empty() || !fs::exists(conf_path)) return;
    std::ifstream file(conf_path);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        size_t hash_pos = line.find('#');
        if (hash_pos != std::string::npos) line = line.substr(0, hash_pos);
        size_t semi_pos = line.find(';');
        if (semi_pos != std::string::npos) line = line.substr(0, semi_pos);

        auto trim = [](std::string s) -> std::string {
            while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) s.erase(s.begin());
            while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.pop_back();
            return s;
        };
        line = trim(line);
        if (line.empty() || line.front() == '[') continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));

        if (key == "window_width") {
            try { m_app_config.window_width = std::stoi(val); } catch (...) {}
        } else if (key == "window_height") {
            try { m_app_config.window_height = std::stoi(val); } catch (...) {}
        } else if (key == "font_family") {
            if (!val.empty()) m_app_config.font_family = val;
        } else if (key == "font_size") {
            try { m_app_config.font_size = std::stoi(val); } catch (...) {}
        } else if (key == "show_status_bar") {
            m_app_config.show_status_bar = (val == "true" || val == "1" || val == "yes");
        }
    }
}

void NotepadApp::setup_ui() {
    m_root_layout = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(0)
        ->build();

    // 1. Toolbar
    m_toolbar = ToolbarBuilder::create()
        ->title("Untitled")
        ->subtitle("[New Document]")
        ->iconText("📝")
        ->toolbarHeight(46)
        ->showDivider(true)
        ->onClose([this]() {
            if (m_engine) m_engine->quit();
        })
        ->closeTooltip("Exit miqunotepad")
        ->build();

    auto new_btn = m_toolbar->add_action("+", [this]() {
        new_document();
    });
    new_btn->set_tooltip("New File");
    new_btn->set_tooltip_shortcut("Ctrl+N");

    auto open_btn = m_toolbar->add_action("📂", [this]() {
        open_with_portal();
    });
    open_btn->set_tooltip("Open File");
    open_btn->set_tooltip_shortcut("Ctrl+O");

    auto save_btn = m_toolbar->add_action("💾", [this]() {
        save_file();
    });
    save_btn->set_tooltip("Save File");
    save_btn->set_tooltip_shortcut("Ctrl+S");

    auto saveas_btn = m_toolbar->add_action("⤓", [this]() {
        save_with_portal();
    });
    saveas_btn->set_tooltip("Save As");
    saveas_btn->set_tooltip_shortcut("Ctrl+Shift+S");

    m_open_with_btn = m_toolbar->add_action("↗", [this]() {
        if (m_current_file_path.empty()) {
            update_status("Save file first to open in another app");
            return;
        }
        m_open_with_menu = FileOpener::build_open_with_menu(m_current_file_path, "miqunotepad.desktop");
        m_open_with_menu->show_as_dropdown(m_open_with_btn, PopupGravity::BottomStart, 0, 4);
    });
    m_open_with_btn->set_tooltip("Open With Other App");

    m_root_layout->add_view(m_toolbar);

    // 2. Prompt Bar (Inline file path input for Open / Save As)
    m_prompt_bar = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(8)
        ->build();
    m_prompt_bar->set_padding(14, 6, 14, 6);
    m_prompt_bar->set_visibility(Visibility::Gone);

    m_prompt_label = TextViewBuilder::create("File:")
        ->bold(true)
        ->build();
    m_prompt_label->set_layout_params(LayoutParams(static_cast<int>(LayoutDimension::WrapContent), static_cast<int>(LayoutDimension::WrapContent), Gravity::CenterVertical));

    m_prompt_input = EditTextBuilder::create()
        ->hint("Enter file path...")
        ->editable(true)
        ->drawBackground(true)
        ->onSubmit([this](const std::string&) {
            handle_prompt_submit();
        })
        ->build();
    LayoutParams input_params(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f);
    input_params.gravity = Gravity::CenterVertical;
    m_prompt_input->set_layout_params(input_params);

    m_prompt_confirm_btn = ButtonBuilder::create()
        ->text("Open")
        ->primary(true)
        ->build();
    m_prompt_confirm_btn->set_layout_params(LayoutParams(static_cast<int>(LayoutDimension::WrapContent), 32, Gravity::CenterVertical));
    m_prompt_confirm_btn->set_on_click_listener([this]() {
        handle_prompt_submit();
    });

    m_prompt_cancel_btn = ButtonBuilder::create()
        ->text("Cancel")
        ->outlined(true)
        ->build();
    m_prompt_cancel_btn->set_layout_params(LayoutParams(static_cast<int>(LayoutDimension::WrapContent), 32, Gravity::CenterVertical));
    m_prompt_cancel_btn->set_on_click_listener([this]() {
        hide_prompt();
    });

    m_prompt_bar->add_view(m_prompt_label);
    m_prompt_bar->add_view(m_prompt_input);
    m_prompt_bar->add_view(m_prompt_confirm_btn);
    m_prompt_bar->add_view(m_prompt_cancel_btn);

    m_root_layout->add_view(m_prompt_bar);

    // 3. Scrollable Editor Area
    m_editor = EditTextBuilder::create()
        ->multiline(true)
        ->editable(true)
        ->fontFamily(m_app_config.font_family)
        ->fontSize(m_app_config.font_size)
        ->drawBackground(false)
        ->drawFocusRing(false)
        ->build();
    m_editor->set_padding(18, 14, 18, 14);
    m_editor->set_on_text_changed_listener([this](auto, const std::string&) {
        m_is_modified = true;
        update_title_and_stats();
    });

    m_scroll_view = ScrollViewBuilder::create()
        ->contentView(m_editor)
        ->scrollbar(true)
        ->build();
    m_scroll_view->set_layout_params(LayoutParams(static_cast<int>(LayoutDimension::MatchParent), 0, 1.0f));

    m_root_layout->add_view(m_scroll_view);

    // 4. Bottom Status Bar
    m_status_bar = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(8)
        ->build();
    m_status_bar->set_padding(16, 4, 16, 4);
    m_status_bar->set_layout_params(LayoutParams(static_cast<int>(LayoutDimension::MatchParent), 26));

    if (!m_app_config.show_status_bar) {
        m_status_bar->set_visibility(Visibility::Gone);
    }

    m_status_msg = TextViewBuilder::create("Ready")
        ->caption(true)
        ->muted(true)
        ->build();
    LayoutParams status_params(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f);
    status_params.gravity = Gravity::CenterVertical;
    m_status_msg->set_layout_params(status_params);

    m_stats_label = TextViewBuilder::create("1 lines | 0 chars | UTF-8")
        ->caption(true)
        ->muted(true)
        ->build();
    m_stats_label->set_layout_params(LayoutParams(static_cast<int>(LayoutDimension::WrapContent), static_cast<int>(LayoutDimension::WrapContent), Gravity::CenterVertical));

    m_status_bar->add_view(m_status_msg);
    m_status_bar->add_view(m_stats_label);

    m_root_layout->add_view(m_status_bar);
}

void NotepadApp::sync_theme() {
    auto config = Config::get();
    if (!config) return;

    m_root_layout->set_background_color(config->colors.background);

    if (m_prompt_bar) {
        m_prompt_bar->set_background_color(config->colors.surface_variant.with_alpha(0.5f));
    }
    if (m_prompt_label) {
        m_prompt_label->set_text_color(config->colors.on_surface);
    }
    if (m_status_bar) {
        m_status_bar->set_background_color(config->colors.surface_variant.with_alpha(0.35f));
    }
    if (m_status_msg) {
        m_status_msg->set_text_color(config->colors.on_surface_variant);
    }
    if (m_stats_label) {
        m_stats_label->set_text_color(config->colors.on_surface_variant);
    }
    if (m_editor) {
        m_editor->set_text_color(config->colors.on_background);
    }

    if (m_window) {
        m_window->schedule_redraw();
    }
}

void NotepadApp::new_document() {
    m_current_file_path.clear();
    m_editor->clear();
    m_is_modified = false;
    hide_prompt();
    update_title_and_stats();
    update_status("New document");
    m_editor->set_focused(true);
    if (m_window) m_window->schedule_redraw();
}

void NotepadApp::show_open_prompt() {
    m_prompt_mode = PromptMode::Open;
    m_prompt_label->set_text("Open File:");
    m_prompt_confirm_btn->set_text("Open");
    m_prompt_input->set_text("");
    m_prompt_input->set_hint("e.g. ~/notes.txt or /path/to/file");
    m_prompt_bar->set_visibility(Visibility::Visible);
    m_prompt_input->set_focused(true);
    if (m_window) m_window->schedule_redraw();
}

void NotepadApp::show_save_prompt() {
    m_prompt_mode = PromptMode::SaveAs;
    m_prompt_label->set_text("Save As:");
    m_prompt_confirm_btn->set_text("Save");
    m_prompt_input->set_text(!m_current_file_path.empty() ? m_current_file_path : "~/Untitled.txt");
    m_prompt_input->set_hint("e.g. ~/document.txt");
    m_prompt_bar->set_visibility(Visibility::Visible);
    m_prompt_input->set_focused(true);
    if (m_window) m_window->schedule_redraw();
}

void NotepadApp::open_with_portal() {
    if (m_portal_picker_active) return;
    m_portal_picker_active = true;
    update_status("Opening file picker...");

    std::string current_dir;
    if (!m_current_file_path.empty()) {
        std::error_code ec;
        fs::path p = fs::path(FsUtils::expand_user_path(m_current_file_path)).parent_path();
        if (fs::exists(p, ec)) current_dir = p.string();
    }

    auto engine = m_engine;
    std::thread([this, engine, current_dir]() {
        PortalPickResult res = PortalFileChooser::pick_open_file("Open File", current_dir);
        if (!engine) return;
        engine->post([this, res]() {
            m_portal_picker_active = false;
            if (!res.available) {
                show_open_prompt();
                return;
            }
            if (res.accepted && !res.path.empty()) {
                open_file(res.path);
            } else {
                update_status("Ready");
            }
        });
    }).detach();
}

void NotepadApp::save_with_portal() {
    if (m_portal_picker_active) return;
    m_portal_picker_active = true;
    update_status("Opening save dialog...");

    std::string current_name = "Untitled.txt";
    std::string current_dir;
    if (!m_current_file_path.empty()) {
        fs::path p(FsUtils::expand_user_path(m_current_file_path));
        if (p.has_filename()) current_name = p.filename().string();
        std::error_code ec;
        if (p.has_parent_path() && fs::exists(p.parent_path(), ec)) {
            current_dir = p.parent_path().string();
        }
    }

    auto engine = m_engine;
    std::thread([this, engine, current_name, current_dir]() {
        PortalPickResult res = PortalFileChooser::pick_save_file("Save File", current_name, current_dir);
        if (!engine) return;
        engine->post([this, res]() {
            m_portal_picker_active = false;
            if (!res.available) {
                show_save_prompt();
                return;
            }
            if (res.accepted && !res.path.empty()) {
                save_file(res.path);
            } else {
                update_status("Ready");
            }
        });
    }).detach();
}

void NotepadApp::hide_prompt() {
    m_prompt_mode = PromptMode::None;
    m_prompt_bar->set_visibility(Visibility::Gone);
    m_editor->set_focused(true);
    if (m_window) m_window->schedule_redraw();
}

void NotepadApp::handle_prompt_submit() {
    std::string path = m_prompt_input->get_text();
    if (path.empty()) {
        update_status("Please provide a file path");
        return;
    }

    if (m_prompt_mode == PromptMode::Open) {
        if (open_file(path)) {
            hide_prompt();
        }
    } else if (m_prompt_mode == PromptMode::SaveAs) {
        if (save_file(path)) {
            hide_prompt();
        }
    }
}

bool NotepadApp::open_file(const std::string& path) {
    std::string expanded = FsUtils::expand_user_path(path);
    std::ifstream file(expanded, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        update_status("Error opening file: " + path);
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    m_current_file_path = expanded;
    m_editor->set_text(std::move(content));
    m_is_modified = false;
    update_title_and_stats();
    std::string fname = fs::path(expanded).filename().string();
    update_status("Opened " + fname);
    if (m_window) m_window->schedule_redraw();
    return true;
}

bool NotepadApp::save_file(const std::string& path) {
    std::string target = path.empty() ? m_current_file_path : path;
    if (target.empty()) {
        save_with_portal();
        return false;
    }

    std::string expanded = FsUtils::expand_user_path(target);
    std::ofstream file(expanded, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        update_status("Error saving file: " + target);
        return false;
    }

    const std::string& content = m_editor->get_text();
    file.write(content.data(), content.size());
    file.close();

    m_current_file_path = expanded;
    m_is_modified = false;
    update_title_and_stats();
    std::string fname = fs::path(expanded).filename().string();
    update_status("Saved " + fname);
    if (m_window) m_window->schedule_redraw();
    return true;
}

void NotepadApp::update_title_and_stats() {
    std::string fname = m_current_file_path.empty() ? "Untitled" : fs::path(m_current_file_path).filename().string();
    std::string mod_prefix = m_is_modified ? "*" : "";

    std::string win_title = mod_prefix + fname + " - miqunotepad";
    if (m_window) {
        m_window->set_title(win_title);
    }

    if (m_toolbar) {
        m_toolbar->set_title(fname + (m_is_modified ? " *" : ""));
        m_toolbar->set_subtitle(!m_current_file_path.empty() ? m_current_file_path : "[Untitled document]");
    }

    if (m_stats_label && m_editor) {
        const std::string& text = m_editor->get_text();
        int lines = 1;
        for (char c : text) {
            if (c == '\n') lines++;
        }
        int chars = static_cast<int>(text.size());
        m_stats_label->set_text(std::to_string(lines) + " lines | " + std::to_string(chars) + " chars | UTF-8");
    }

    if (m_window) {
        m_window->schedule_redraw();
    }
}

void NotepadApp::update_status(const std::string& msg) {
    if (m_status_msg) {
        m_status_msg->set_text(msg);
        if (m_window) m_window->schedule_redraw();
    }
}

bool NotepadApp::handle_key_press(const KeyPressEvent& ev) {
    if (!ev.pressed) return false;

    // 1. When prompt bar is active
    if (m_prompt_mode != PromptMode::None) {
        if (ev.keysym == XKB_KEY_Escape) {
            hide_prompt();
            return true;
        }
        if (ev.keysym == XKB_KEY_Return || ev.keysym == XKB_KEY_KP_Enter) {
            handle_prompt_submit();
            return true;
        }
        return false;
    }

    // 2. Window Shortcuts
    if (ev.has_ctrl()) {
        if (ev.keysym == XKB_KEY_n || ev.keysym == XKB_KEY_N) {
            new_document();
            return true;
        }
        if (!ev.has_shift() && (ev.keysym == XKB_KEY_o || ev.keysym == XKB_KEY_O)) {
            open_with_portal();
            return true;
        }
        if (!ev.has_shift() && (ev.keysym == XKB_KEY_s || ev.keysym == XKB_KEY_S)) {
            save_file();
            return true;
        }
        if (ev.has_shift() && (ev.keysym == XKB_KEY_s || ev.keysym == XKB_KEY_S)) {
            save_with_portal();
            return true;
        }
        if (ev.keysym == XKB_KEY_q || ev.keysym == XKB_KEY_Q) {
            if (m_engine) m_engine->quit();
            return true;
        }
    }

    return false;
}

} // namespace miqu
