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

} // namespace miqu
