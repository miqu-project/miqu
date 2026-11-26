#pragma once

#include "explorer_view.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <memory>
#include <string>

namespace miqu {

class FilesApp {
public:
    explicit FilesApp(std::shared_ptr<AppEngine> engine, const std::string& start_path = "");
    ~FilesApp() = default;

    bool init();
    void sync_theme();
    void handle_key_press(const KeyPressEvent& event);

    std::shared_ptr<Window> get_window() const { return m_window; }

private:
    void setup_ui();
    void update_header();
    void update_nav_buttons_ui();
    void update_view_mode_buttons_ui();
    void update_hidden_btn_ui();
    void toggle_show_hidden();

    std::shared_ptr<AppEngine> m_engine;
    std::string m_start_path;
    std::shared_ptr<Window> m_window;

    std::shared_ptr<LinearLayout> m_root_layout;
    std::shared_ptr<Toolbar> m_toolbar;
    std::shared_ptr<LinearLayout> m_ribbon;

    std::shared_ptr<CardView> m_back_btn;
    std::shared_ptr<TextView> m_back_label;
    std::shared_ptr<CardView> m_forward_btn;
    std::shared_ptr<TextView> m_forward_label;
    std::shared_ptr<CardView> m_up_btn;
    std::shared_ptr<TextView> m_up_label;

    std::shared_ptr<LinearLayout> m_breadcrumb_container;
    std::shared_ptr<SearchView> m_search_view;

    std::shared_ptr<CardView> m_view_mode_btn;
    std::shared_ptr<TextView> m_view_mode_lbl;

    std::shared_ptr<CardView> m_hidden_btn;
    std::shared_ptr<ImageView> m_hidden_icon;
    std::shared_ptr<TextView> m_hidden_label;

    std::shared_ptr<ExplorerView> m_explorer_view;
};

} // namespace miqu
