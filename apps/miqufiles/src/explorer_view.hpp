#pragma once

#include "files_model.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <filesystem>
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <unordered_set>

namespace miqu {

enum class ExplorerViewMode {
    Grid,
    List,
    Details
};

enum class SortField {
    Name,
    Modified,
    Type,
    Size
};

struct PlaceEntry {
    std::string title;
    std::string icon;
    std::filesystem::path path;
};

struct FilesClipboard {
    std::vector<std::filesystem::path> paths;
    bool is_cut = false;
    bool empty() const { return paths.empty(); }
    void clear() { paths.clear(); is_cut = false; }
};

class ExplorerView : public LinearLayout {
public:
    explicit ExplorerView(const std::string& start_path = "", bool show_hidden = false);
    ~ExplorerView() override = default;

    void set_explorer_window(std::shared_ptr<Window> window) { m_window = std::move(window); }
    void set_on_state_changed(std::function<void()> cb) { m_on_state_changed = std::move(cb); }

    // Navigation & History
    void navigate_to_dir(const std::filesystem::path& dir, bool push_history = true);
    bool navigate_back();
    bool navigate_forward();
    bool navigate_up();
    bool navigate_enter();

    bool can_navigate_back() const { return !m_back_stack.empty(); }
    bool can_navigate_forward() const { return !m_forward_stack.empty(); }
    bool can_navigate_up() const {
        return !m_current_dir.empty() && m_current_dir.has_parent_path() && m_current_dir != m_current_dir.parent_path();
    }

    const std::filesystem::path& get_current_dir() const { return m_current_dir; }
    std::string get_current_dir_title() const;

    // View Modes & Panes
    void set_view_mode(ExplorerViewMode mode);
    ExplorerViewMode get_view_mode() const { return m_view_mode; }

    void set_preview_visible(bool visible);
    bool is_preview_visible() const { return m_preview_visible; }
    void toggle_preview() { set_preview_visible(!m_preview_visible); }

    void set_sidebar_visible(bool visible);
    bool is_sidebar_visible() const { return m_sidebar_visible; }
    void toggle_sidebar() { set_sidebar_visible(!m_sidebar_visible); }
    void set_sidebar_focused(bool focused);
    bool is_sidebar_focused() const { return m_sidebar_focused; }

    // Sorting
    void set_sort(SortField field);
    SortField get_sort_field() const { return m_sort_field; }
    bool is_sort_ascending() const { return m_sort_ascending; }

    // Search & Hidden
    void set_search_query(const std::string& query);
    bool is_searching() const { return !m_search_query.empty(); }
    void set_show_hidden(bool show);
    bool get_show_hidden() const { return m_show_hidden; }

    // Keyboard & Theme
    bool handle_key(const KeyPressEvent& event);
    void sync_theme();
    bool check_and_update_click_debounce();

    // Directory reload
    void reload_current_directory(const std::string& select_path = "");

private:
    void setup_ui();
    void setup_places();
    void sync_sidebar_selection();
    void setup_context_menus();
    void apply_sort_and_filter(const std::string& select_path = "");
    void rebuild_active_items();
    void update_details_header();
    void update_status_bar();
    void schedule_preview_rebuild();
    void rebuild_preview_pane();
    void notify_state_changed();

    // Row / Tile builders
    std::shared_ptr<View> create_grid_tile(size_t idx, const std::shared_ptr<FileItem>& item, const std::shared_ptr<Config>& config);
    std::shared_ptr<View> create_list_row(const std::shared_ptr<FileItem>& item, const std::shared_ptr<Config>& config);
    std::shared_ptr<View> create_details_row(const std::shared_ptr<FileItem>& item, const std::shared_ptr<Config>& config);
    std::shared_ptr<View> create_sidebar_row(const PlaceEntry& place, const std::shared_ptr<Config>& config);

    // Selection helpers
    int get_cursor_index() const;
    void set_cursor_index(int index);
    std::vector<std::shared_ptr<FileItem>> get_selected_or_cursor_items() const;
    std::shared_ptr<FileItem> get_preview_target_item() const;
    void open_item(const std::shared_ptr<FileItem>& item);

    // Preview renderers (bare-metal)
    void render_empty_preview(const std::shared_ptr<Config>& config);
    void render_multi_preview(const std::vector<std::shared_ptr<FileItem>>& items, const std::shared_ptr<Config>& config);
    void render_image_preview(const std::shared_ptr<FileItem>& item, const std::shared_ptr<Config>& config);
    void render_text_preview(const std::shared_ptr<FileItem>& item, const std::shared_ptr<Config>& config);
    void render_metadata_preview(const std::shared_ptr<FileItem>& item, const std::shared_ptr<Config>& config);
    void show_file_info_popup(const std::shared_ptr<FileItem>& item, int lx = -1, int ly = -1);

    // Context menu & file operations (context_menu.cpp)
    void show_item_context_menu(const std::shared_ptr<FileItem>& item, int lx, int ly);
    void show_empty_context_menu(const std::filesystem::path& dir, int lx, int ly);
    void show_new_file_menu(const std::filesystem::path& dir, int lx, int ly);
    void show_text_input_dialog(const std::string& title, const std::string& initial_value,
                                const std::string& confirm_label, std::function<void(const std::string&)> on_confirm);
    void show_confirm_dialog(const std::string& title, const std::string& message,
                             const std::string& confirm_label, bool destructive, std::function<void()> on_confirm);

    void action_copy_items(const std::vector<std::shared_ptr<FileItem>>& items, bool is_cut);
    void action_paste_into(const std::filesystem::path& target_dir);
    void action_trash_items(const std::vector<std::shared_ptr<FileItem>>& items);
    void action_delete_items(const std::vector<std::shared_ptr<FileItem>>& items);
    void action_add_to_desktop(const std::shared_ptr<FileItem>& item);
    void action_set_as_wallpaper(const std::shared_ptr<FileItem>& item);

    static std::string resolve_terminal_emulator();
    static bool open_terminal_in(const std::filesystem::path& dir);
    static bool run_in_terminal(const std::filesystem::path& working_dir, const std::string& shell_command);
    static void copy_text_to_clipboard(const std::string& text, const std::string& mime_type = "text/plain");

    // State
    std::shared_ptr<Window> m_window;
    std::function<void()> m_on_state_changed;

    std::filesystem::path m_current_dir;
    std::vector<std::filesystem::path> m_back_stack;
    std::vector<std::filesystem::path> m_forward_stack;

    bool m_show_hidden = false;
    std::string m_search_query;
    ExplorerViewMode m_view_mode = ExplorerViewMode::Grid;
    SortField m_sort_field = SortField::Name;
    bool m_sort_ascending = true;
    bool m_preview_visible = true;
    bool m_sidebar_visible = true;
    bool m_sidebar_focused = false;

    std::vector<PlaceEntry> m_places;
    std::vector<std::shared_ptr<FileItem>> m_all_items;
    std::vector<std::shared_ptr<FileItem>> m_displayed_items;
    std::vector<std::shared_ptr<View>> m_grid_tiles;
    std::shared_ptr<SelectionModel> m_selection_model;

    // UI Components
    std::shared_ptr<LinearLayout> m_sidebar_pillar;
    std::shared_ptr<ListView> m_sidebar_list;
    std::shared_ptr<DividerView> m_sidebar_divider;

    std::shared_ptr<LinearLayout> m_content_pillar;
    std::shared_ptr<LinearLayout> m_details_header;
    std::shared_ptr<TextView> m_hdr_name_lbl;
    std::shared_ptr<TextView> m_hdr_mod_lbl;
    std::shared_ptr<TextView> m_hdr_type_lbl;
    std::shared_ptr<TextView> m_hdr_size_lbl;

    std::shared_ptr<GridView> m_grid_view;
    std::shared_ptr<ListView> m_list_view;
    std::shared_ptr<LinearLayout> m_status_bar;
    std::shared_ptr<TextView> m_status_label;

    std::shared_ptr<DividerView> m_preview_divider;
    std::shared_ptr<LinearLayout> m_preview_pillar;
    std::shared_ptr<ScrollView> m_preview_scroll;
    std::shared_ptr<LinearLayout> m_preview_content;

    // Popups & async guards
    std::shared_ptr<PopupMenu> m_context_menu;
    std::shared_ptr<PopupMenu> m_open_with_menu;
    std::shared_ptr<PopupWindow> m_info_popup;
    std::shared_ptr<PopupWindow> m_dialog_popup;
    FilesClipboard m_clipboard;
    uint64_t m_preview_load_gen = 0;
    uint64_t m_preview_debounce_timer = 0;
    uint64_t m_last_click_ms = 0;
};

} // namespace miqu
