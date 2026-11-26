#include "explorer_view.hpp"
#include "preview_common.hpp"
#include <xkbcommon/xkbcommon-keysyms.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <thread>

namespace fs = std::filesystem;

namespace miqu {

constexpr int MATCH_PARENT = static_cast<int>(LayoutDimension::MatchParent);
constexpr int WRAP_CONTENT = static_cast<int>(LayoutDimension::WrapContent);

static std::string to_lower_copy(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::tolower(c); });
    return out;
}

ExplorerView::ExplorerView(const std::string& start_path, bool show_hidden)
    : m_show_hidden(show_hidden) {
    set_orientation(Orientation::Horizontal);
    set_background_color(Color::transparent());

    m_selection_model = std::make_shared<SelectionModel>(SelectionMode::Multiple);

    fs::path initial;
    if (!start_path.empty()) {
        std::error_code ec;
        fs::path cand(start_path);
        if (fs::exists(cand, ec) && fs::is_directory(cand, ec)) {
            initial = fs::weakly_canonical(cand, ec);
            if (ec) initial = cand;
        }
    }
    if (initial.empty()) {
        const char* home = std::getenv("HOME");
        initial = home ? fs::path(home) : fs::path("/");
    }

    setup_places();
    setup_ui();
    setup_context_menus();
    navigate_to_dir(initial, false);
}

void ExplorerView::setup_places() {
    m_places.clear();
    const char* home_env = std::getenv("HOME");
    fs::path home = home_env ? fs::path(home_env) : fs::path("/");

    auto add_if_exists = [&](const std::string& title, const std::string& icon, const fs::path& p, bool require_exists = true) {
        std::error_code ec;
        if (!require_exists || (fs::exists(p, ec) && fs::is_directory(p, ec))) {
            m_places.push_back({title, icon, p});
        }
    };

    add_if_exists("Home", "user-home", home);
    add_if_exists("Desktop", "user-desktop", home / "Desktop");
    add_if_exists("Documents", "folder-documents", home / "Documents");
    add_if_exists("Downloads", "folder-download", home / "Downloads");
    add_if_exists("Pictures", "folder-pictures", home / "Pictures");
    add_if_exists("Music", "folder-music", home / "Music");
    add_if_exists("Videos", "folder-videos", home / "Videos");
    add_if_exists("Trash", "user-trash", home / ".local/share/Trash/files");
    add_if_exists("File System", "drive-harddisk", fs::path("/"));

    // Mounted media volumes
    const char* user_env = std::getenv("USER");
    std::vector<fs::path> media_roots;
    if (user_env) {
        media_roots.push_back(fs::path("/run/media") / user_env);
    }
    media_roots.push_back(fs::path("/media"));
    for (const auto& root : media_roots) {
        std::error_code ec;
        if (fs::exists(root, ec) && fs::is_directory(root, ec)) {
            for (const auto& entry : fs::directory_iterator(root, fs::directory_options::skip_permission_denied, ec)) {
                if (entry.is_directory(ec)) {
                    m_places.push_back({entry.path().filename().string(), "drive-removable-media", entry.path()});
                }
            }
        }
    }
}

void ExplorerView::setup_ui() {
    auto config = Config::get();
    int inner_rad = get_inner_radius(config);

    // 1. Left Places Sidebar
    m_sidebar_pillar = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(8, 8)
        ->spacing(4)
        ->cornerRadius(inner_rad)
        ->backgroundColor(Color::transparent())
        ->build();

    m_sidebar_list = ListViewBuilder::create()
        ->itemHeight(34)
        ->spacing(2)
        ->itemCornerRadius(inner_rad)
        ->scrollbar(false)
        ->selectionMode(SelectionMode::Single)
        ->keyboardNavigation(true)
        ->onItemClick([this](size_t idx, std::shared_ptr<View>) {
            set_sidebar_focused(false);
            if (idx < m_places.size()) {
                navigate_to_dir(m_places[idx].path, true);
            }
        })
        ->build();
    m_sidebar_list->set_focusable(false);

    for (const auto& place : m_places) {
        m_sidebar_list->add_item(create_sidebar_row(place, config));
    }

    m_sidebar_pillar->add_view(m_sidebar_list, LayoutParams(MATCH_PARENT, MATCH_PARENT, 1.0f));
    add_view(m_sidebar_pillar, LayoutParams(190, MATCH_PARENT));

    m_sidebar_divider = DividerViewBuilder::create()
        ->orientation(Orientation::Vertical)
        ->thickness(1)
        ->color(config->colors.outline.with_alpha(0.25f))
        ->build();
    add_view(m_sidebar_divider, LayoutParams(1, MATCH_PARENT));

    // 2. Center Content Area (Details Header + Grid/List + Status Bar)
    m_content_pillar = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(10, 6)
        ->spacing(4)
        ->cornerRadius(inner_rad)
        ->backgroundColor(Color::transparent())
        ->build();

    // Details column header
    m_details_header = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(10, 4)
        ->spacing(8)
        ->backgroundColor(config->colors.surface_variant.with_alpha(0.28f))
        ->cornerRadius(inner_rad)
        ->build();

    auto make_hdr_col = [&](std::shared_ptr<TextView>& lbl_out, const std::string& title, SortField field,
                            TextAlignment align, int width, float weight) {
        lbl_out = TextViewBuilder::create()
            ->text(title)
            ->caption(true)
            ->bold(true)
            ->textAlignment(align)
            ->textColor(config->colors.on_surface_variant)
            ->build();

        auto btn = CardViewBuilder::create()
            ->style(CardStyle::Filled)
            ->backgroundColor(Color::transparent())
            ->elevation(0)
            ->cornerRadius(4)
            ->addView(lbl_out)
            ->build();
        btn->set_focusable(false);
        btn->set_on_click_listener([this, field]() { set_sort(field); });

        LayoutParams lp(width, WRAP_CONTENT, Gravity::CenterVertical);
        lp.weight = weight;
        m_details_header->add_view(btn, lp);
    };

    make_hdr_col(m_hdr_name_lbl, "Name", SortField::Name, TextAlignment::Left, MATCH_PARENT, 1.0f);
    make_hdr_col(m_hdr_type_lbl, "Kind", SortField::Type, TextAlignment::Left, 110, 0.0f);
    make_hdr_col(m_hdr_size_lbl, "Size", SortField::Size, TextAlignment::Right, 85, 0.0f);
    make_hdr_col(m_hdr_mod_lbl, "Modified", SortField::Modified, TextAlignment::Right, 140, 0.0f);
    m_details_header->set_visibility(Visibility::Visible);
    m_content_pillar->add_view(m_details_header, LayoutParams(MATCH_PARENT, 28));
    update_details_header();

    auto on_selection_cb = [this](const std::unordered_set<int>&) {
        update_status_bar();
        schedule_preview_rebuild();
    };
    auto on_cursor_cb = [this](int) {
        update_status_bar();
        schedule_preview_rebuild();
    };
    auto on_item_click_cb = [this](size_t, std::shared_ptr<View>) {
        update_status_bar();
        schedule_preview_rebuild();
    };
    auto on_item_dbl_cb = [this](size_t idx, std::shared_ptr<View>) {
        if (idx < m_displayed_items.size()) {
            open_item(m_displayed_items[idx]);
        }
    };

    // Grid View (Primary Explorer Canvas)
    m_grid_view = GridViewBuilder::create()
        ->autoFit(130)
        ->cellHeight(116)
        ->spacing(10, 10)
        ->bottomPadding(96)
        ->clearSelectionOnEmptyClick(true)
        ->selectionModel(m_selection_model)
        ->selectionBorder(true)
        ->onSelectionChanged(on_selection_cb)
        ->onCursorChanged(on_cursor_cb)
        ->onItemClick(on_item_click_cb)
        ->onItemDoubleClick(on_item_dbl_cb)
        ->build();
    m_grid_view->set_visibility(Visibility::Gone);

    // List / Details View
    m_list_view = ListViewBuilder::create()
        ->itemHeight(44)
        ->spacing(3)
        ->itemCornerRadius(inner_rad)
        ->scrollbar(true)
        ->bottomPadding(96)
        ->clearSelectionOnEmptyClick(true)
        ->selectionModel(m_selection_model)
        ->onSelectionChanged(on_selection_cb)
        ->onCursorChanged(on_cursor_cb)
        ->onItemClick(on_item_click_cb)
        ->onItemDoubleClick(on_item_dbl_cb)
        ->build();
    m_list_view->set_visibility(Visibility::Visible);

    m_content_pillar->add_view(m_grid_view, LayoutParams(MATCH_PARENT, MATCH_PARENT, 1.0f));
    m_content_pillar->add_view(m_list_view, LayoutParams(MATCH_PARENT, MATCH_PARENT, 1.0f));

    // Bottom Status Bar
    m_status_label = TextViewBuilder::create()
        ->text("0 items")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    auto status_hint = TextViewBuilder::create()
        ->text("Arrow keys to navigate \u2022 Enter to open")
        ->caption(true)
        ->textAlignment(TextAlignment::Right)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.65f))
        ->build();
    m_status_bar = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(6, 2)
        ->gravity(Gravity::CenterVertical)
        ->build();
    m_status_bar->add_view(m_status_label, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));
    m_status_bar->add_view(status_hint, LayoutParams(WRAP_CONTENT, WRAP_CONTENT));
    m_content_pillar->add_view(m_status_bar, LayoutParams(MATCH_PARENT, 22));

    add_view(m_content_pillar, LayoutParams(MATCH_PARENT, MATCH_PARENT, 1.0f));

    // 3. Right Bare-Metal Preview Pane
    m_preview_divider = DividerViewBuilder::create()
        ->orientation(Orientation::Vertical)
        ->thickness(1)
        ->color(config->colors.outline.with_alpha(0.25f))
        ->build();
    add_view(m_preview_divider, LayoutParams(1, MATCH_PARENT));

    m_preview_pillar = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(10, 10)
        ->spacing(8)
        ->backgroundColor(Color::transparent())
        ->build();

    m_preview_content = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(10)
        ->backgroundColor(Color::transparent())
        ->build();

    m_preview_scroll = ScrollViewBuilder::create()
        ->contentView(m_preview_content)
        ->scrollbar(true)
        ->build();
    m_preview_scroll->set_focusable(false);

    m_preview_pillar->add_view(m_preview_scroll, LayoutParams(MATCH_PARENT, MATCH_PARENT, 1.0f));
    add_view(m_preview_pillar, LayoutParams(280, MATCH_PARENT));
}

std::shared_ptr<View> ExplorerView::create_sidebar_row(const PlaceEntry& place, const std::shared_ptr<Config>& config) {
    auto row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(8, 0)
        ->spacing(10)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto icon = ImageViewBuilder::create()
        ->source(place.icon)
        ->targetSize(18)
        ->fitMode(FitMode::Contain)
        ->build();
    row->add_view(icon, LayoutParams(18, 18, Gravity::CenterVertical));

    auto label = TextViewBuilder::create()
        ->text(place.title)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->ellipsize(true)
        ->maxLines(1)
        ->build();
    row->add_view(label, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));

    return row;
}

std::shared_ptr<View> ExplorerView::create_grid_tile(size_t /*idx*/, const std::shared_ptr<FileItem>& item, const std::shared_ptr<Config>& config) {
    bool is_img = (!item->is_dir && item->category == FileCategory::Image && fs::exists(item->path));
    std::string src = is_img ? item->path.string() : item->icon;

    return GridItemViewBuilder::create()
        ->title(item->title)
        ->iconSource(src)
        ->isImage(is_img)
        ->qualityMode(is_img ? ImageQuality::ThumbnailFast : ImageQuality::FullOriginal)
        ->cornerRadius(get_inner_radius(config))
        ->padding(8, 8)
        ->build();
}

std::shared_ptr<View> ExplorerView::create_list_row(const std::shared_ptr<FileItem>& item, const std::shared_ptr<Config>& config) {
    auto row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(10, 0)
        ->spacing(10)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto icon = ImageViewBuilder::create()
        ->source(item->icon)
        ->targetSize(20)
        ->fitMode(FitMode::Contain)
        ->build();
    row->add_view(icon, LayoutParams(20, 20, Gravity::CenterVertical));

    auto title = TextViewBuilder::create()
        ->text(item->title)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->ellipsize(true)
        ->maxLines(1)
        ->build();
    row->add_view(title, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));

    if (!item->subtitle.empty()) {
        auto sub = TextViewBuilder::create()
            ->text(item->subtitle)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.75f))
            ->build();
        row->add_view(sub, LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity::CenterVertical));
    }

    return row;
}

std::shared_ptr<View> ExplorerView::create_details_row(const std::shared_ptr<FileItem>& item, const std::shared_ptr<Config>& config) {
    auto row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(12, 0)
        ->spacing(10)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto icon = ImageViewBuilder::create()
        ->source(item->icon)
        ->targetSize(22)
        ->fitMode(FitMode::Contain)
        ->build();
    row->add_view(icon, LayoutParams(22, 22, Gravity::CenterVertical));

    auto name_lbl = TextViewBuilder::create()
        ->text(item->title)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->ellipsize(true)
        ->maxLines(1)
        ->build();
    row->add_view(name_lbl, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));

    std::string type_str = item->is_dir ? "Folder" : item->category_label;
    auto type_lbl = TextViewBuilder::create()
        ->text(type_str)
        ->caption(true)
        ->textColor(config->colors.on_surface_variant)
        ->ellipsize(true)
        ->maxLines(1)
        ->build();
    row->add_view(type_lbl, LayoutParams(110, WRAP_CONTENT, Gravity::CenterVertical));

    std::string size_str = item->is_dir ? item->subtitle : item->size_formatted;
    auto size_lbl = TextViewBuilder::create()
        ->text(size_str)
        ->caption(true)
        ->textAlignment(TextAlignment::Right)
        ->textColor(config->colors.on_surface_variant)
        ->ellipsize(true)
        ->maxLines(1)
        ->build();
    row->add_view(size_lbl, LayoutParams(85, WRAP_CONTENT, Gravity::CenterVertical));

    auto mod_lbl = TextViewBuilder::create()
        ->text(item->mtime_formatted)
        ->caption(true)
        ->textAlignment(TextAlignment::Right)
        ->textColor(config->colors.on_surface_variant)
        ->ellipsize(true)
        ->maxLines(1)
        ->build();
    row->add_view(mod_lbl, LayoutParams(140, WRAP_CONTENT, Gravity::CenterVertical));

    return row;
}

void ExplorerView::start_directory_load(const fs::path& dir, const std::string& select_path) {
    uint64_t gen = ++m_dir_load_gen;
    bool show_hidden = m_show_hidden;
    auto alive = m_alive;

    m_all_items.clear();
    m_displayed_items.clear();
    m_grid_tiles.clear();
    if (m_selection_model) {
        m_selection_model->clear_marks();
        m_selection_model->set_cursor(-1);
    }
    if (m_grid_view) m_grid_view->scroll_to_top();
    if (m_list_view) m_list_view->scroll_to_top();
    rebuild_active_items();
    if (m_status_label) m_status_label->set_text("Loading...");
    rebuild_preview_pane();
    sync_sidebar_selection();
    notify_state_changed();

    auto* eng = AppEngine::instance();
    if (!eng) {
        m_all_items = FilesModel::scan_directory(dir, show_hidden);
        apply_sort_and_filter(select_path);
        sync_sidebar_selection();
        notify_state_changed();
        return;
    }

    std::thread([this, alive, gen, dir, show_hidden, select_path]() {
        auto items = FilesModel::scan_directory(dir, show_hidden);
        if (auto* app = AppEngine::instance()) {
            app->post([this, alive, gen, items = std::move(items), select_path]() mutable {
                if (!alive || !alive->load() || gen != m_dir_load_gen) return;
                m_all_items = std::move(items);
                apply_sort_and_filter(select_path);
                sync_sidebar_selection();
                notify_state_changed();
            });
        }
    }).detach();
}

void ExplorerView::navigate_to_dir(const fs::path& dir, bool push_history, const std::string& select_path) {
    std::error_code ec;
    fs::path target = fs::weakly_canonical(dir, ec);
    if (ec || target.empty()) target = dir;
    if (!fs::exists(target, ec) || !fs::is_directory(target, ec)) return;

    if (push_history && !m_current_dir.empty() && m_current_dir != target) {
        m_back_stack.push_back(m_current_dir);
        m_forward_stack.clear();
    }

    m_current_dir = target;
    start_directory_load(m_current_dir, select_path);
}

bool ExplorerView::navigate_back() {
    if (m_back_stack.empty()) return false;
    fs::path prev = m_back_stack.back();
    m_back_stack.pop_back();
    if (!m_current_dir.empty()) {
        m_forward_stack.push_back(m_current_dir);
    }
    navigate_to_dir(prev, false);
    return true;
}

bool ExplorerView::navigate_forward() {
    if (m_forward_stack.empty()) return false;
    fs::path next = m_forward_stack.back();
    m_forward_stack.pop_back();
    if (!m_current_dir.empty()) {
        m_back_stack.push_back(m_current_dir);
    }
    navigate_to_dir(next, false);
    return true;
}

bool ExplorerView::navigate_up() {
    if (!can_navigate_up()) return false;
    std::string child_path = m_current_dir.string();
    navigate_to_dir(m_current_dir.parent_path(), true, child_path);
    return true;
}

bool ExplorerView::navigate_enter() {
    int idx = get_cursor_index();
    if (idx < 0 || idx >= static_cast<int>(m_displayed_items.size())) {
        if (!m_displayed_items.empty()) idx = 0;
        else return false;
    }
    open_item(m_displayed_items[idx]);
    return true;
}

void ExplorerView::open_item(const std::shared_ptr<FileItem>& item) {
    if (!item || item->path.empty()) return;
    if (item->is_dir) {
        navigate_to_dir(item->path, true);
    } else if (FileOpener::is_executable_file(item->path) || !FileOpener::launch_default(item->path)) {
        if (!m_window) return;
        m_open_with_menu = FileOpener::build_open_with_menu(item->path);
        int px = std::max(80, m_window->get_width() / 2 - 120);
        int py = std::max(80, m_window->get_height() / 3);
        m_open_with_menu->show_at_location(m_window.get(), px, py);
    }
}

std::string ExplorerView::get_current_dir_title() const {
    if (m_current_dir.empty()) return "Files";
    std::string name = m_current_dir.filename().string();
    return name.empty() ? m_current_dir.string() : name;
}

void ExplorerView::set_view_mode(ExplorerViewMode mode) {
    if (m_view_mode == mode) return;
    m_view_mode = mode;

    if (m_details_header) {
        m_details_header->set_visibility(mode == ExplorerViewMode::Details ? Visibility::Visible : Visibility::Gone);
    }
    if (m_grid_view) {
        m_grid_view->set_visibility(mode == ExplorerViewMode::Grid ? Visibility::Visible : Visibility::Gone);
    }
    if (m_list_view) {
        m_list_view->set_visibility(mode != ExplorerViewMode::Grid ? Visibility::Visible : Visibility::Gone);
        m_list_view->set_item_height(mode == ExplorerViewMode::Details ? 44 : 38);
    }

    update_details_header();
    rebuild_active_items();
    notify_state_changed();
    request_redraw();
}

void ExplorerView::set_preview_visible(bool visible) {
    if (m_preview_visible == visible) return;
    m_preview_visible = visible;
    Visibility vis = visible ? Visibility::Visible : Visibility::Gone;
    if (m_preview_divider) m_preview_divider->set_visibility(vis);
    if (m_preview_pillar) m_preview_pillar->set_visibility(vis);
    if (visible) rebuild_preview_pane();
    notify_state_changed();
    request_redraw();
}

void ExplorerView::set_sidebar_visible(bool visible) {
    if (m_sidebar_visible == visible) return;
    m_sidebar_visible = visible;
    Visibility vis = visible ? Visibility::Visible : Visibility::Gone;
    if (m_sidebar_pillar) m_sidebar_pillar->set_visibility(vis);
    if (m_sidebar_divider) m_sidebar_divider->set_visibility(vis);
    notify_state_changed();
    request_redraw();
}

void ExplorerView::set_sort(SortField field) {
    if (m_sort_field == field) {
        m_sort_ascending = !m_sort_ascending;
    } else {
        m_sort_field = field;
        m_sort_ascending = true;
    }
    update_details_header();
    apply_sort_and_filter();
}

void ExplorerView::set_search_query(const std::string& query) {
    m_search_query = query;
    apply_sort_and_filter();
}

void ExplorerView::set_show_hidden(bool show) {
    if (m_show_hidden == show) return;
    m_show_hidden = show;
    reload_current_directory();
}

void ExplorerView::apply_sort_and_filter(const std::string& select_path) {
    std::string prev_path = select_path;
    if (prev_path.empty()) {
        int cur = get_cursor_index();
        if (cur >= 0 && cur < static_cast<int>(m_displayed_items.size())) {
            prev_path = m_displayed_items[cur]->path.string();
        }
    }

    m_displayed_items.clear();
    std::string q = to_lower_copy(m_search_query);
    for (const auto& item : m_all_items) {
        if (!q.empty()) {
            if (to_lower_copy(item->title).find(q) == std::string::npos) continue;
        }
        m_displayed_items.push_back(item);
    }

    std::sort(m_displayed_items.begin(), m_displayed_items.end(),
        [this](const std::shared_ptr<FileItem>& a, const std::shared_ptr<FileItem>& b) {
            if (a->is_dir != b->is_dir) return a->is_dir > b->is_dir; // Folders always first
            int cmp = 0;
            switch (m_sort_field) {
                case SortField::Name:
                    cmp = to_lower_copy(a->title).compare(to_lower_copy(b->title));
                    break;
                case SortField::Modified:
                    cmp = a->mtime_formatted.compare(b->mtime_formatted);
                    break;
                case SortField::Type:
                    cmp = a->category_label.compare(b->category_label);
                    if (cmp == 0) cmp = to_lower_copy(a->title).compare(to_lower_copy(b->title));
                    break;
                case SortField::Size:
                    if (a->size_bytes < b->size_bytes) cmp = -1;
                    else if (a->size_bytes > b->size_bytes) cmp = 1;
                    else cmp = to_lower_copy(a->title).compare(to_lower_copy(b->title));
                    break;
            }
            return m_sort_ascending ? (cmp < 0) : (cmp > 0);
        });

    rebuild_active_items();

    if (!prev_path.empty()) {
        for (size_t i = 0; i < m_displayed_items.size(); ++i) {
            if (m_displayed_items[i]->path.string() == prev_path) {
                set_cursor_index(static_cast<int>(i));
                break;
            }
        }
    } else if (m_selection_model) {
        m_selection_model->clear_marks();
        m_selection_model->set_cursor(m_displayed_items.empty() ? -1 : 0);
    }

    update_status_bar();
    rebuild_preview_pane();
}

void ExplorerView::rebuild_active_items() {
    auto config = Config::get();
    m_grid_tiles.clear();
    if (m_view_mode == ExplorerViewMode::Grid) {
        m_grid_tiles.resize(m_displayed_items.size());
        if (m_grid_view) {
            m_grid_view->set_item_provider(m_displayed_items.size(), [this, config](size_t idx) -> std::shared_ptr<View> {
                if (idx >= m_displayed_items.size()) return nullptr;
                if (!m_grid_tiles[idx]) {
                    m_grid_tiles[idx] = create_grid_tile(idx, m_displayed_items[idx], config);
                }
                return m_grid_tiles[idx];
            });
        }
    } else {
        if (m_list_view) {
            std::vector<std::shared_ptr<View>> rows;
            rows.reserve(m_displayed_items.size());
            for (const auto& item : m_displayed_items) {
                rows.push_back(m_view_mode == ExplorerViewMode::Details
                    ? create_details_row(item, config)
                    : create_list_row(item, config));
            }
            m_list_view->set_items(std::move(rows));
        }
    }
    request_redraw();
}

void ExplorerView::update_details_header() {
    auto config = Config::get();
    if (!config) return;
    std::string arrow = m_sort_ascending ? " ▲" : " ▼";

    auto set_hdr = [&](const std::shared_ptr<TextView>& lbl, const std::string& base, SortField f) {
        if (!lbl) return;
        bool active = (m_sort_field == f);
        lbl->set_text(active ? (base + arrow) : base);
        lbl->set_text_color(active ? config->colors.primary : config->colors.on_surface_variant);
    };

    set_hdr(m_hdr_name_lbl, "Name", SortField::Name);
    set_hdr(m_hdr_mod_lbl, "Modified", SortField::Modified);
    set_hdr(m_hdr_type_lbl, "Kind", SortField::Type);
    set_hdr(m_hdr_size_lbl, "Size", SortField::Size);
}

void ExplorerView::update_status_bar() {
    if (!m_status_label) return;
    size_t total = m_displayed_items.size();
    std::string text = std::to_string(total) + (total == 1 ? " item" : " items");

    auto selected = get_selected_or_cursor_items();
    size_t mark_count = m_selection_model ? m_selection_model->marks().size() : 0;
    if (mark_count > 1 || (!selected.empty() && mark_count == 1)) {
        uintmax_t sel_bytes = 0;
        bool has_files = false;
        for (const auto& it : selected) {
            if (!it->is_dir) {
                sel_bytes += it->size_bytes;
                has_files = true;
            }
        }
        text += "  •  " + std::to_string(selected.size()) + " selected";
        if (has_files) {
            text += " (" + FilesModel::format_file_size(sel_bytes) + ")";
        }
    }

    m_status_label->set_text(text);
}

void ExplorerView::sync_sidebar_selection() {
    if (!m_sidebar_list) return;
    std::error_code ec;
    int match = -1;
    for (size_t i = 0; i < m_places.size(); ++i) {
        if (m_places[i].path == m_current_dir || fs::equivalent(m_places[i].path, m_current_dir, ec)) {
            match = static_cast<int>(i);
            break;
        }
    }
    m_sidebar_list->set_selected_index(match);
}

int ExplorerView::get_cursor_index() const {
    return m_selection_model ? m_selection_model->cursor() : -1;
}

void ExplorerView::set_cursor_index(int index) {
    if (!m_selection_model) return;
    if (index >= 0 && index < static_cast<int>(m_displayed_items.size())) {
        m_selection_model->set_cursor(index);
    } else {
        m_selection_model->set_cursor(-1);
    }
    if (m_view_mode == ExplorerViewMode::Grid && m_grid_view) {
        m_grid_view->request_redraw();
    } else if (m_list_view) {
        m_list_view->request_redraw();
    }
}

std::vector<std::shared_ptr<FileItem>> ExplorerView::get_selected_or_cursor_items() const {
    std::vector<std::shared_ptr<FileItem>> result;
    if (!m_selection_model) return result;

    const auto& marks = m_selection_model->marks();
    if (!marks.empty()) {
        std::vector<int> indices(marks.begin(), marks.end());
        std::sort(indices.begin(), indices.end());
        for (int idx : indices) {
            if (idx >= 0 && idx < static_cast<int>(m_displayed_items.size())) {
                result.push_back(m_displayed_items[idx]);
            }
        }
        return result;
    }

    int cur = m_selection_model->cursor();
    if (cur >= 0 && cur < static_cast<int>(m_displayed_items.size())) {
        result.push_back(m_displayed_items[cur]);
    }
    return result;
}

std::shared_ptr<FileItem> ExplorerView::get_preview_target_item() const {
    int cur = get_cursor_index();
    if (cur >= 0 && cur < static_cast<int>(m_displayed_items.size())) {
        return m_displayed_items[cur];
    }
    return nullptr;
}

void ExplorerView::schedule_preview_rebuild() {
    if (!m_preview_visible) return;
    auto* eng = AppEngine::instance();
    if (!eng) {
        rebuild_preview_pane();
        return;
    }
    if (m_preview_debounce_timer != 0) {
        eng->cancel_delayed(m_preview_debounce_timer);
        m_preview_debounce_timer = 0;
    }
    std::weak_ptr<View> weak_self = weak_from_this();
    m_preview_debounce_timer = eng->post_delayed(75, [this, weak_self]() {
        if (weak_self.expired()) return;
        m_preview_debounce_timer = 0;
        rebuild_preview_pane();
    });
}

void ExplorerView::rebuild_preview_pane() {
    if (!m_preview_visible || !m_preview_content) return;
    if (m_preview_debounce_timer != 0) {
        if (auto* eng = AppEngine::instance()) {
            eng->cancel_delayed(m_preview_debounce_timer);
        }
        m_preview_debounce_timer = 0;
    }
    m_preview_load_gen++;
    m_preview_content->clear_views();

    auto config = Config::get();
    if (!config) return;

    size_t marks = m_selection_model ? m_selection_model->marks().size() : 0;
    if (marks > 1) {
        render_multi_preview(get_selected_or_cursor_items(), config);
    } else {
        auto item = get_preview_target_item();
        if (!item) {
            render_empty_preview(config);
        } else if (!item->is_dir && item->category == FileCategory::Image) {
            render_image_preview(item, config);
        } else if (!item->is_dir && FilesModel::is_text_previewable(item)) {
            render_text_preview(item, config);
        } else {
            render_metadata_preview(item, config);
        }
    }

    if (m_preview_pillar) m_preview_pillar->request_redraw();
    request_redraw();
}

void ExplorerView::set_sidebar_focused(bool focused) {
    if (!m_sidebar_visible) focused = false;
    m_sidebar_focused = focused;
    auto config = Config::get();
    if (config) {
        Color active_col = config->colors.primary.with_alpha(0.55f);
        if (m_sidebar_pillar) {
            m_sidebar_pillar->set_stroke(focused ? 1 : 0, focused ? active_col : Color::transparent());
        }
        if (m_content_pillar) {
            m_content_pillar->set_stroke(!focused ? 1 : 0, !focused ? active_col : Color::transparent());
        }
    }
    if (focused && m_sidebar_list && m_sidebar_list->get_selected_index() < 0 && !m_places.empty()) {
        m_sidebar_list->set_selected_index(0);
    } else if (!focused && get_cursor_index() < 0 && !m_displayed_items.empty()) {
        set_cursor_index(0);
        update_status_bar();
        rebuild_preview_pane();
    }
    request_redraw();
}

bool ExplorerView::handle_key(const KeyPressEvent& event) {
    if (!event.pressed) return false;

    // Tab switches focus between Places Sidebar and Main Folder View
    if (event.keysym == XKB_KEY_Tab || event.keysym == XKB_KEY_ISO_Left_Tab) {
        if (m_sidebar_visible) {
            set_sidebar_focused(!m_sidebar_focused);
            return true;
        }
    }

    // View mode & pane shortcuts
    if (event.has_alt() && (event.keysym == XKB_KEY_p || event.keysym == XKB_KEY_P)) {
        toggle_preview();
        return true;
    }
    if (event.has_ctrl() && (event.keysym == XKB_KEY_b || event.keysym == XKB_KEY_B)) {
        if (m_sidebar_visible && m_sidebar_focused) set_sidebar_focused(false);
        toggle_sidebar();
        return true;
    }
    if (event.has_ctrl() && event.keysym == XKB_KEY_1) {
        set_view_mode(ExplorerViewMode::Grid);
        return true;
    }
    if (event.has_ctrl() && event.keysym == XKB_KEY_2) {
        set_view_mode(ExplorerViewMode::List);
        return true;
    }
    if (event.has_ctrl() && event.keysym == XKB_KEY_3) {
        set_view_mode(ExplorerViewMode::Details);
        return true;
    }

    // History navigation
    if (event.has_alt() && event.keysym == XKB_KEY_Left) {
        return navigate_back();
    }
    if (event.has_alt() && event.keysym == XKB_KEY_Right) {
        return navigate_forward();
    }

    // Sidebar-focused keyboard navigation
    if (m_sidebar_focused && m_sidebar_visible && m_sidebar_list) {
        if (event.keysym == XKB_KEY_Escape) {
            set_sidebar_focused(false);
            return true;
        }
        if (event.keysym == XKB_KEY_Return || event.keysym == XKB_KEY_KP_Enter ||
            event.keysym == XKB_KEY_Right || event.keysym == XKB_KEY_l || event.keysym == XKB_KEY_L) {
            if (event.is_repeat) return true;
            int sel = m_sidebar_list->get_selected_index();
            set_sidebar_focused(false);
            if (sel >= 0 && sel < static_cast<int>(m_places.size())) {
                navigate_to_dir(m_places[sel].path, true);
            }
            return true;
        }
        if (m_sidebar_list->on_key(event)) {
            return true;
        }
        return false;
    }

    // Keyboard Context Menu (Menu key or Shift+F10)
    if (event.keysym == XKB_KEY_Menu || (event.has_shift() && event.keysym == XKB_KEY_F10)) {
        int lx = m_sidebar_visible ? 260 : 80;
        int ly = 120;
        auto item = get_preview_target_item();
        if (item) {
            show_item_context_menu(item, lx, ly);
        } else {
            show_empty_context_menu(m_current_dir, lx, ly);
        }
        return true;
    }

    // File management shortcuts
    if (event.has_ctrl() && event.has_shift() && (event.keysym == XKB_KEY_N || event.keysym == XKB_KEY_n)) {
        if (!m_current_dir.empty()) {
            show_text_input_dialog("New Folder", "New Folder", "Create", [this](const std::string& name) {
                if (name.empty()) return;
                std::error_code ec;
                fs::path target = m_current_dir / name;
                if (fs::create_directory(target, ec)) {
                    reload_current_directory(target.string());
                }
            });
            return true;
        }
    }

    if (event.keysym == XKB_KEY_F2) {
        auto item = get_preview_target_item();
        if (item && !item->path.empty()) {
            show_text_input_dialog("Rename", item->title, "Rename", [this, item](const std::string& new_name) {
                if (new_name.empty() || new_name == item->title) return;
                std::error_code ec;
                fs::path target = item->path.parent_path() / new_name;
                if (!fs::exists(target, ec)) {
                    fs::rename(item->path, target, ec);
                    if (!ec) reload_current_directory(target.string());
                }
            });
            return true;
        }
    }

    if (event.has_ctrl() && (event.keysym == XKB_KEY_c || event.keysym == XKB_KEY_C)) {
        auto items = get_selected_or_cursor_items();
        if (!items.empty()) {
            action_copy_items(items, false);
            return true;
        }
    }
    if (event.has_ctrl() && (event.keysym == XKB_KEY_x || event.keysym == XKB_KEY_X)) {
        auto items = get_selected_or_cursor_items();
        if (!items.empty()) {
            action_copy_items(items, true);
            return true;
        }
    }
    if (event.has_ctrl() && (event.keysym == XKB_KEY_v || event.keysym == XKB_KEY_V)) {
        if (!m_clipboard.empty() && !m_current_dir.empty()) {
            action_paste_into(m_current_dir);
            return true;
        }
    }

    if (event.keysym == XKB_KEY_Delete) {
        auto items = get_selected_or_cursor_items();
        if (!items.empty()) {
            if (event.has_shift()) action_delete_items(items);
            else action_trash_items(items);
            return true;
        }
    }

    if ((event.keysym == XKB_KEY_Return || event.keysym == XKB_KEY_KP_Enter) && event.is_repeat) {
        return true;
    }

    // Active view keyboard navigation (Grid vs List/Details)
    if (m_view_mode == ExplorerViewMode::Grid && m_grid_view) {
        if (m_grid_view->on_key(event)) return true;
    } else if (m_list_view) {
        if (m_list_view->on_key(event)) return true;
    }

    if (event.keysym == XKB_KEY_Return || event.keysym == XKB_KEY_KP_Enter) {
        return navigate_enter();
    }
    if (event.keysym == XKB_KEY_BackSpace) {
        return navigate_up();
    }

    return false;
}

void ExplorerView::sync_theme() {
    auto config = Config::get();
    if (!config) return;

    if (m_sidebar_divider) m_sidebar_divider->set_color(config->colors.outline.with_alpha(0.25f));
    if (m_preview_divider) m_preview_divider->set_color(config->colors.outline.with_alpha(0.25f));
    if (m_details_header) m_details_header->set_background_color(config->colors.surface_variant.with_alpha(0.28f));
    if (m_status_label) m_status_label->set_text_color(config->colors.on_surface_variant);

    if (m_sidebar_list) {
        m_sidebar_list->clear_items();
        for (const auto& place : m_places) {
            m_sidebar_list->add_item(create_sidebar_row(place, config));
        }
        sync_sidebar_selection();
    }

    update_details_header();
    rebuild_active_items();
    rebuild_preview_pane();
}

bool ExplorerView::check_and_update_click_debounce() {
    auto now = std::chrono::steady_clock::now();
    uint64_t ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
    if (ms - m_last_click_ms < 220) return false;
    m_last_click_ms = ms;
    return true;
}

void ExplorerView::notify_state_changed() {
    if (m_on_state_changed) m_on_state_changed();
}

} // namespace miqu
