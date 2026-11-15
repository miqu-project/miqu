#include "miller_view.hpp"
#include "preview_common.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>

namespace fs = std::filesystem;

namespace miqu {

constexpr int MATCH_PARENT = static_cast<int>(LayoutDimension::MatchParent);
constexpr int WRAP_CONTENT = static_cast<int>(LayoutDimension::WrapContent);

void MillerView::render_empty_preview(const std::shared_ptr<Config>& config) {
    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->gravity(Gravity::Center)
        ->padding(24, 32)
        ->spacing(8)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto icon = TextViewBuilder::create()
        ->text("󰉖")
        ->textSize(static_cast<int>(config->metrics.font_size * 2.5f))
        ->textColor(config->colors.on_surface_variant.with_alpha(0.5f))
        ->center(true)
        ->build();
    card->add_view(icon);

    auto text = TextViewBuilder::create()
        ->text("No File Selected")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->center(true)
        ->build();
    card->add_view(text);

    auto hint = TextViewBuilder::create()
        ->text("Select a file or folder to view details")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.6f))
        ->center(true)
        ->build();
    card->add_view(hint);

    add_preview_card(card);
}

void MillerView::render_empty_folder_preview(const std::shared_ptr<FileItem>& item, const std::shared_ptr<Config>& config) {
    if (!item) return;

    m_preview_layout->set_padding(Padding(14, 4, 14, 14));
    m_preview_layout->set_divider_spacing(10);

    // 1. Hero Folder Card
    auto hero_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 16)
        ->spacing(14)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto header_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(14)
        ->gravity(Gravity::CenterVertical)
        ->build();

    // Folder Icon Box
    auto icon_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->gravity(Gravity::Center)
        ->backgroundColor(config->colors.surface_variant.with_alpha(0.4f))
        ->cornerRadius(get_inner_radius(config))
        ->build();

    auto folder_icon = TextViewBuilder::create()
        ->text(item->icon.empty() ? "󰉖" : item->icon)
        ->textSize(static_cast<int>(config->metrics.font_size * 2.2f))
        ->textColor(config->colors.primary.with_alpha(0.85f))
        ->center(true)
        ->build();
    icon_box->add_view(folder_icon);
    header_row->add_view(icon_box, LayoutParams(80, 80));

    // Info Column
    auto info_col = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(4)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto title_view = TextViewBuilder::create()
        ->text(item->title)
        ->textSize(static_cast<int>(config->metrics.font_size * 1.2f))
        ->bold(true)
        ->textColor(config->colors.on_surface)
        ->ellipsize(true)
        ->build();
    info_col->add_view(title_view);

    auto cat_view = TextViewBuilder::create()
        ->text("Folder • Empty (0 items)")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.primary)
        ->ellipsize(true)
        ->build();
    info_col->add_view(cat_view);

    if (!item->mtime_formatted.empty()) {
        auto date_view = TextViewBuilder::create()
            ->text("Modified: " + item->mtime_formatted)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        info_col->add_view(date_view);
    }

    header_row->add_view(info_col, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));
    hero_card->add_view(header_row);

    // Divider
    auto div = LinearLayoutBuilder::create()
        ->backgroundColor(config->colors.outline.with_alpha(0.2f))
        ->build();
    hero_card->add_view(div, LayoutParams(MATCH_PARENT, 1));

    // Action Buttons Row
    auto action_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(10)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto term_btn = ButtonBuilder::create()
        ->text("󰆍 Terminal Here")
        ->padding(12, 6)
        ->cornerRadius(get_btn_radius(config))
        ->textSize(config->metrics.font_size)
        ->build();
    term_btn->set_focusable(true);
    term_btn->set_on_click_listener([item]() {
        if (!item || item->path.empty()) return;
        std::string cmd = "alacritty --working-directory \"" + item->path.string() + "\" &";
        (void)std::system(cmd.c_str());
    });
    action_row->add_view(term_btn, LayoutParams(WRAP_CONTENT, 32));

    auto copy_btn = ButtonBuilder::create()
        ->text("󰅍 Copy Path")
        ->flat(true)
        ->padding(12, 6)
        ->cornerRadius(get_btn_radius(config))
        ->textSize(config->metrics.font_size)
        ->build();
    copy_btn->set_focusable(true);
    copy_btn->set_on_click_listener([item]() {
        if (!item || item->path.empty()) return;
        std::string p = item->path.string();
        std::string cmd = "printf '%s' " + p + " | wl-copy 2>/dev/null || true";
        (void)std::system(cmd.c_str());
    });
    action_row->add_view(copy_btn, LayoutParams(WRAP_CONTENT, 32));

    hero_card->add_view(action_row);
    add_preview_card(hero_card);

    // 2. Path & Status Details Card
    auto detail_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(8)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto location_label = TextViewBuilder::create()
        ->text("Location")
        ->caption(true)
        ->bold(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    detail_card->add_view(location_label);

    auto path_view = TextViewBuilder::create()
        ->text(item->path.string())
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->ellipsize(true)
        ->build();
    detail_card->add_view(path_view);

    auto empty_status = TextViewBuilder::create()
        ->text("This folder is empty. Create or paste files here to populate it.")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.6f))
        ->build();
    detail_card->add_view(empty_status);

    add_preview_card(detail_card);
}

void MillerView::render_category_children_preview(const std::shared_ptr<FileItem>& item, const std::shared_ptr<Config>& config) {
    if (!item) return;
    item->ensure_children();
    if (m_category_list) {
        m_category_list->clear_items();
        m_category_list->set_visibility(Visibility::Visible);
        for (const auto& child : item->children) {
            m_category_list->add_item(create_list_row_item(child, config));
        }
    }
    if (m_preview_scroll) {
        m_preview_scroll->set_visibility(Visibility::Gone);
    }
}

void MillerView::render_file_preview(const std::shared_ptr<FileItem>& item, const std::shared_ptr<Config>& config) {
    if (!item) return;

    // 1. Hero File Card
    auto hero_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 16)
        ->spacing(14)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto header_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(14)
        ->gravity(Gravity::CenterVertical)
        ->build();

    // Image thumbnail or Icon Box
    if (item->category == FileCategory::Image && fs::exists(item->path)) {
        auto img = ImageViewBuilder::create()
            ->source(item->path.string())
            ->cornerRadius(get_inner_radius(config))
            ->build();
        header_row->add_view(img, LayoutParams(96, 96));
    } else {
        auto icon_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->gravity(Gravity::Center)
            ->backgroundColor(config->colors.surface_variant.with_alpha(0.4f))
            ->cornerRadius(get_inner_radius(config))
            ->build();

        auto file_icon = TextViewBuilder::create()
            ->text(item->icon.empty() ? "󰈔" : item->icon)
            ->textSize(static_cast<int>(config->metrics.font_size * 2.2f))
            ->textColor(config->colors.primary.with_alpha(0.8f))
            ->center(true)
            ->build();
        icon_box->add_view(file_icon);
        header_row->add_view(icon_box, LayoutParams(96, 96));
    }

    // Info Column
    auto info_col = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(4)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto title_view = TextViewBuilder::create()
        ->text(item->title)
        ->textSize(static_cast<int>(config->metrics.font_size * 1.2f))
        ->bold(true)
        ->textColor(config->colors.on_surface)
        ->ellipsize(true)
        ->build();
    info_col->add_view(title_view);

    auto cat_view = TextViewBuilder::create()
        ->text(item->category_label)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.primary)
        ->ellipsize(true)
        ->build();
    info_col->add_view(cat_view);

    auto size_view = TextViewBuilder::create()
        ->text(item->size_formatted)
        ->caption(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    info_col->add_view(size_view);

    auto date_view = TextViewBuilder::create()
        ->text(item->mtime_formatted)
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    info_col->add_view(date_view);

    header_row->add_view(info_col, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));
    hero_card->add_view(header_row);

    // Divider
    auto div = LinearLayoutBuilder::create()
        ->backgroundColor(config->colors.outline.with_alpha(0.2f))
        ->build();
    hero_card->add_view(div, LayoutParams(MATCH_PARENT, 1));

    // Action Buttons Row
    auto action_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(10)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto open_btn = ButtonBuilder::create()
        ->text("▶ Open")
        ->padding(12, 6)
        ->cornerRadius(get_btn_radius(config))
        ->textSize(config->metrics.font_size)
        ->build();
    open_btn->set_focusable(true);
    open_btn->set_on_click_listener([item]() {
        if (!item || item->path.empty()) return;
        std::string cmd = "xdg-open \"" + item->path.string() + "\" &";
        (void)std::system(cmd.c_str());
    });
    action_row->add_view(open_btn, LayoutParams(WRAP_CONTENT, 32));

    auto term_btn = ButtonBuilder::create()
        ->text("󰆍 Terminal Here")
        ->flat(true)
        ->padding(12, 6)
        ->cornerRadius(get_btn_radius(config))
        ->textSize(config->metrics.font_size)
        ->build();
    term_btn->set_focusable(true);
    term_btn->set_on_click_listener([item]() {
        if (!item) return;
        fs::path target_dir = item->is_dir ? item->path : item->path.parent_path();
        std::string cmd = "alacritty --working-directory \"" + target_dir.string() + "\" &";
        (void)std::system(cmd.c_str());
    });
    action_row->add_view(term_btn, LayoutParams(WRAP_CONTENT, 32));

    auto copy_btn = ButtonBuilder::create()
        ->text("󰅍 Copy Path")
        ->flat(true)
        ->padding(12, 6)
        ->cornerRadius(get_btn_radius(config))
        ->textSize(config->metrics.font_size)
        ->build();
    copy_btn->set_focusable(true);
    copy_btn->set_on_click_listener([item]() {
        if (!item || item->path.empty()) return;
        std::string p = item->path.string();
        std::string cmd = "printf '%s' " + p + " | wl-copy 2>/dev/null || true";
        (void)std::system(cmd.c_str());
    });
    action_row->add_view(copy_btn, LayoutParams(WRAP_CONTENT, 32));

    hero_card->add_view(action_row);
    add_preview_card(hero_card);

    // 2. Text / Code Preview Card (if applicable)
    if ((item->category == FileCategory::CodeText || item->category == FileCategory::Document) &&
        item->size_bytes > 0 && item->size_bytes < 1024 * 1024 * 5) {
        std::ifstream ifs(item->path);
        if (ifs.is_open()) {
            std::string preview_content;
            std::string line;
            int count = 0;
            while (std::getline(ifs, line) && count < 40) {
                preview_content += line + "\n";
                count++;
            }

            if (!preview_content.empty()) {
                auto code_card = LinearLayoutBuilder::create()
                    ->orientation(Orientation::Vertical)
                    ->padding(14, 12)
                    ->spacing(6)
                    ->backgroundColor(config->colors.surface)
                    ->stroke(1, config->colors.outline.with_alpha(0.3f))
                    ->cornerRadius(get_card_radius(config))
                    ->build();

                auto code_header = TextViewBuilder::create()
                    ->text("File Preview (" + std::to_string(count) + " lines)")
                    ->caption(true)
                    ->textColor(config->colors.primary)
                    ->build();
                code_card->add_view(code_header);

                auto code_text = TextViewBuilder::create()
                    ->text(preview_content)
                    ->caption(true)
                    ->textColor(config->colors.on_surface)
                    ->build();
                code_card->add_view(code_text);

                add_preview_card(code_card);
            }
        }
    }

    // 3. Metadata Info Card
    auto meta_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(8)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto add_meta_row = [&meta_card, config](const std::string& key, const std::string& value) {
        if (value.empty()) return;
        auto row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->spacing(8)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto k_view = TextViewBuilder::create()
            ->text(key)
            ->textSize(static_cast<int>(config->metrics.font_size * 0.9f))
            ->textColor(config->colors.on_surface_variant)
            ->build();
        row->add_view(k_view, LayoutParams(100, WRAP_CONTENT));

        auto v_view = TextViewBuilder::create()
            ->text(value)
            ->textSize(static_cast<int>(config->metrics.font_size * 0.9f))
            ->textColor(config->colors.on_surface)
            ->ellipsize(true)
            ->build();
        row->add_view(v_view, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));

        meta_card->add_view(row);
    };

    add_meta_row("Location", item->path.string());
    add_meta_row("File Size", item->size_formatted + " (" + std::to_string(item->size_bytes) + " bytes)");
    add_meta_row("Category", item->category_label);
    add_meta_row("Permissions", item->permissions_str);
    add_meta_row("Modified", item->mtime_formatted);
    if (item->is_symlink && !item->symlink_target.empty()) {
        add_meta_row("Symlink To", item->symlink_target);
    }

    add_preview_card(meta_card);
}

void MillerView::render_multi_selection_preview(const std::vector<std::shared_ptr<FileItem>>& items, const std::shared_ptr<Config>& config) {
    if (items.empty()) return;

    size_t num_files = 0;
    size_t num_dirs = 0;
    uintmax_t total_bytes = 0;

    for (const auto& it : items) {
        if (!it) continue;
        if (it->is_category || it->is_dir) {
            num_dirs++;
        } else {
            num_files++;
            total_bytes += it->size_bytes;
        }
    }

    // 1. Overview Hero Card
    auto hero_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->gravity(Gravity::Center)
        ->padding(20, 24)
        ->spacing(8)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto hero_icon = TextViewBuilder::create()
        ->text("󰚰")
        ->textSize(static_cast<int>(config->metrics.font_size * 2.4f))
        ->textColor(config->colors.primary)
        ->center(true)
        ->build();
    hero_card->add_view(hero_icon);

    auto hero_title = TextViewBuilder::create()
        ->text(std::to_string(items.size()) + " Items Selected")
        ->textSize(static_cast<int>(config->metrics.font_size * 1.25f))
        ->bold(true)
        ->textColor(config->colors.on_surface)
        ->center(true)
        ->build();
    hero_card->add_view(hero_title);

    std::string count_str;
    if (num_files > 0 && num_dirs > 0) {
        count_str = std::to_string(num_files) + " files, " + std::to_string(num_dirs) + " folders";
    } else if (num_dirs > 0) {
        count_str = std::to_string(num_dirs) + (num_dirs == 1 ? " folder" : " folders");
    } else {
        count_str = std::to_string(num_files) + (num_files == 1 ? " file" : " files");
    }

    auto count_label = TextViewBuilder::create()
        ->text(count_str)
        ->caption(true)
        ->textColor(config->colors.on_surface_variant)
        ->center(true)
        ->build();
    hero_card->add_view(count_label);

    if (total_bytes > 0) {
        auto size_badge = TextViewBuilder::create()
            ->text("Total Size: " + FilesModel::format_file_size(total_bytes))
            ->textSize(static_cast<int>(config->metrics.font_size * 0.95f))
            ->textColor(config->colors.primary)
            ->center(true)
            ->build();
        hero_card->add_view(size_badge);
    }

    add_preview_card(hero_card);

    // 2. Selected Items Breakdown Card
    auto list_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(14, 12)
        ->spacing(6)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto list_header = TextViewBuilder::create()
        ->text("Selection List")
        ->bold(true)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    list_card->add_view(list_header);

    size_t max_preview_items = 8;
    for (size_t i = 0; i < std::min(items.size(), max_preview_items); ++i) {
        const auto& it = items[i];
        if (!it) continue;

        auto row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->spacing(8)
            ->gravity(Gravity::CenterVertical)
            ->padding(4, 3)
            ->build();

        auto icon = TextViewBuilder::create()
            ->text(it->icon.empty() ? (it->is_category ? "󰉋" : "󰈙") : it->icon)
            ->textSize(static_cast<int>(config->metrics.font_size * 0.95f))
            ->textColor(it->is_category ? config->colors.primary : config->colors.on_surface_variant)
            ->build();
        row->add_view(icon, LayoutParams(20, WRAP_CONTENT));

        auto name = TextViewBuilder::create()
            ->text(it->title)
            ->textSize(static_cast<int>(config->metrics.font_size * 0.95f))
            ->textColor(config->colors.on_surface)
            ->ellipsize(true)
            ->build();
        row->add_view(name, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));

        std::string right_text = it->is_category ? "folder" : it->size_formatted;
        auto sz = TextViewBuilder::create()
            ->text(right_text)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        row->add_view(sz, LayoutParams(WRAP_CONTENT, WRAP_CONTENT));

        list_card->add_view(row);
    }

    if (items.size() > max_preview_items) {
        auto more_label = TextViewBuilder::create()
            ->text("+ " + std::to_string(items.size() - max_preview_items) + " more items...")
            ->caption(true)
            ->textColor(config->colors.primary)
            ->padding(4, 2)
            ->build();
        list_card->add_view(more_label);
    }

    add_preview_card(list_card);

    // 3. Selection Shortcuts & Actions Card
    auto hint_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(14, 12)
        ->spacing(4)
        ->backgroundColor(config->colors.surface_variant.with_alpha(0.35f))
        ->stroke(1, config->colors.outline.with_alpha(0.2f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto hint_title = TextViewBuilder::create()
        ->text("Selection Shortcuts")
        ->bold(true)
        ->textSize(static_cast<int>(config->metrics.font_size * 0.9f))
        ->textColor(config->colors.on_surface_variant)
        ->build();
    hint_card->add_view(hint_title);

    auto hint_text = TextViewBuilder::create()
        ->text("Space: Toggle item  |  Shift+↑/↓: Range\nCtrl+A: Select All  |  Esc: Cancel")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.75f))
        ->build();
    hint_card->add_view(hint_text);

    add_preview_card(hint_card);
}

} // namespace miqu
