#pragma once

#include <miqutoolkit/miqutoolkit.hpp>
#include <string>
#include <memory>

namespace miqu {

enum class PromptMode {
    None,
    Open,
    SaveAs
};

struct NotepadConfig {
    int window_width = 920;
    int window_height = 640;
    std::string font_family = "Monospace";
    int font_size = 12;
    bool show_status_bar = true;
};

class NotepadApp {
public:
    explicit NotepadApp(std::shared_ptr<AppEngine> engine, const std::string& initial_file = "");
    ~NotepadApp() = default;

    bool init();
    void sync_theme();
    bool handle_key_press(const KeyPressEvent& event);

    std::shared_ptr<Window> get_window() const { return m_window; }

    void new_document();
    void show_open_prompt();
    void show_save_prompt();
    bool open_file(const std::string& path);
    bool save_file(const std::string& path = "");
    void hide_prompt();

private:
    void setup_ui();
    void load_config_options(const std::string& conf_path);
    void update_title_and_stats();
    void update_status(const std::string& msg);
    void handle_prompt_submit();

    std::shared_ptr<AppEngine> m_engine;
    std::shared_ptr<Window> m_window;
    std::string m_initial_file_path;
    std::string m_current_file_path;
    bool m_is_modified = false;
    NotepadConfig m_app_config;

    PromptMode m_prompt_mode = PromptMode::None;

    std::shared_ptr<LinearLayout> m_root_layout;
    std::shared_ptr<Toolbar> m_toolbar;
    std::shared_ptr<ImageButton> m_open_with_btn;
    std::shared_ptr<PopupMenu> m_open_with_menu;

    // Quick file open/save action bar
    std::shared_ptr<LinearLayout> m_prompt_bar;
    std::shared_ptr<TextView> m_prompt_label;
    std::shared_ptr<EditText> m_prompt_input;
    std::shared_ptr<Button> m_prompt_confirm_btn;
    std::shared_ptr<Button> m_prompt_cancel_btn;

    // Editor & scroll view
    std::shared_ptr<ScrollView> m_scroll_view;
    std::shared_ptr<EditText> m_editor;

    // Status bar
    std::shared_ptr<LinearLayout> m_status_bar;
    std::shared_ptr<TextView> m_status_msg;
    std::shared_ptr<TextView> m_stats_label;
};

} // namespace miqu
