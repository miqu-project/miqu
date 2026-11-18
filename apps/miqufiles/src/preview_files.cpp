#include "miller_view.hpp"
#include "preview_common.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <gdk-pixbuf/gdk-pixbuf.h>

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

    auto icon = ImageViewBuilder::create()
        ->source("system-file-manager")
        ->targetSize(48)
        ->fitMode(FitMode::Contain)
        ->alpha(0.5f)
        ->build();
    card->add_view(icon, LayoutParams(48, 48, Gravity::CenterHorizontal));

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
    if (m_preview_frame) {
        m_preview_frame->set_visibility(Visibility::Gone);
    }
}

void MillerView::render_photo_preview(const std::shared_ptr<FileItem>& item, const std::shared_ptr<Config>& config) {
    if (!item || !m_preview_frame) return;

    // 1. Full-size photo ImageView taking almost entire pillar
    if (fs::exists(item->path)) {
        auto img = ImageViewBuilder::create()
            ->imageResource(item->path.string())
            ->fitMode(FitMode::Contain)
            ->qualityMode(ImageQuality::FullOriginal)
            ->cornerRadius(get_card_radius(config))
            ->padding(4, 4)
            ->build();
        m_preview_frame->add_view(img, LayoutParams(MATCH_PARENT, MATCH_PARENT));
    }

    // 2. Query dimensions
    int img_w = 0, img_h = 0;
    std::string dim_str;
    if (gdk_pixbuf_get_file_info(item->path.string().c_str(), &img_w, &img_h) && img_w > 0 && img_h > 0) {
        dim_str = std::to_string(img_w) + " × " + std::to_string(img_h);
    }

    std::string meta_line;
    if (!dim_str.empty()) meta_line += dim_str;
    if (!item->size_formatted.empty()) {
        if (!meta_line.empty()) meta_line += "  •  ";
        meta_line += item->size_formatted;
    }
    if (!item->mtime_formatted.empty()) {
        if (!meta_line.empty()) meta_line += "  •  ";
        meta_line += item->mtime_formatted;
    }

    // 3. Floating HUD overlay pill centered horizontally at the bottom
    if (!meta_line.empty()) {
        auto overlay_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->padding(16, 8)
            ->gravity(Gravity::Center)
            ->backgroundColor(config->colors.surface.with_alpha(0.85f))
            ->stroke(1, config->colors.outline.with_alpha(0.35f))
            ->cornerRadius(get_card_radius(config))
            ->margin(0, 0, 0, 16)
            ->build();

        auto meta_view = TextViewBuilder::create()
            ->text(meta_line)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.9f))
            ->center(true)
            ->build();
        overlay_card->add_view(meta_view, LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity::Center));

        m_preview_frame->add_view(overlay_card, LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity::CenterHorizontal | Gravity::Bottom));
    }
}

void MillerView::render_file_preview(const std::shared_ptr<FileItem>& item, const std::shared_ptr<Config>& config) {
    if (!item) return;

    // 1. Text / Code Preview Card (if applicable)
    bool has_text_preview = false;
    if ((item->category == FileCategory::CodeText || item->category == FileCategory::Document) &&
        item->size_bytes > 0 && item->size_bytes < 1024 * 1024 * 5) {
        std::ifstream ifs(item->path);
        if (ifs.is_open()) {
            std::string preview_content;
            std::string line;
            int count = 0;
            while (std::getline(ifs, line) && count < 100) {
                preview_content += line + "\n";
                count++;
            }

            if (!preview_content.empty()) {
                has_text_preview = true;
                auto code_card = LinearLayoutBuilder::create()
                    ->orientation(Orientation::Vertical)
                    ->padding(14, 12)
                    ->spacing(8)
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
                    ->fontFamily("Monospace")
                    ->textSize(static_cast<int>(config->metrics.font_size * 0.95f))
                    ->multiline(true)
                    ->wrapMode(WrapMode::WordChar)
                    ->textColor(config->colors.on_surface)
                    ->build();
                code_card->add_view(code_text, LayoutParams(MATCH_PARENT, WRAP_CONTENT));
                code_card->set_focusable(true);

                add_preview_card(code_card);
            }
        }
    }

    // 2. If no text preview (e.g. binary, empty file, archive, media), show a clean overview card
    if (!has_text_preview) {
        auto overview_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->gravity(Gravity::Center)
            ->padding(24, 32)
            ->spacing(12)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(get_card_radius(config))
            ->build();
        overview_card->set_focusable(true);

        auto icon = ImageViewBuilder::create()
            ->source(item->icon.empty() ? "text-x-generic" : item->icon)
            ->targetSize(56)
            ->fitMode(FitMode::Contain)
            ->build();
        overview_card->add_view(icon, LayoutParams(56, 56, Gravity::CenterHorizontal));

        auto title = TextViewBuilder::create()
            ->text(item->title)
            ->bold(true)
            ->textSize(static_cast<int>(config->metrics.font_size * 1.15f))
            ->textColor(config->colors.on_surface)
            ->center(true)
            ->ellipsize(true)
            ->build();
        overview_card->add_view(title);

        auto hint = TextViewBuilder::create()
            ->text(item->category_label + " • " + item->size_formatted)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->center(true)
            ->build();
        overview_card->add_view(hint);

        add_preview_card(overview_card);
    }
}

void MillerView::show_file_info_popup(const std::shared_ptr<FileItem>& item, View* anchor, const std::shared_ptr<Config>& config) {
    if (!item || !anchor || !config) return;

    if (m_active_popup && m_active_popup->is_showing()) {
        m_active_popup->dismiss();
        m_active_popup = nullptr;
    }

    auto popup_col = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 16)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.35f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    // 1. Header with file icon, title, and category
    auto header = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(12)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto icon = ImageViewBuilder::create()
        ->source(item->icon.empty() ? "text-x-generic" : item->icon)
        ->targetSize(40)
        ->fitMode(FitMode::Contain)
        ->build();
    header->add_view(icon, LayoutParams(40, 40));

    auto title_col = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(2)
        ->build();

    auto title_view = TextViewBuilder::create()
        ->text(item->title)
        ->bold(true)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->ellipsize(true)
        ->build();
    title_col->add_view(title_view);

    auto cat_view = TextViewBuilder::create()
        ->text(item->category_label)
        ->caption(true)
        ->textColor(config->colors.primary)
        ->build();
    title_col->add_view(cat_view);

    header->add_view(title_col, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));
    popup_col->add_view(header);

    // Divider
    auto div = LinearLayoutBuilder::create()
        ->backgroundColor(config->colors.outline.with_alpha(0.2f))
        ->build();
    popup_col->add_view(div, LayoutParams(MATCH_PARENT, 1));

    // Metadata details
    auto add_row = [&](const std::string& label, const std::string& val) {
        if (val.empty()) return;
        auto row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->spacing(10)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto lbl = TextViewBuilder::create()
            ->text(label)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant)
            ->build();
        row->add_view(lbl, LayoutParams(72, WRAP_CONTENT));

        auto v = TextViewBuilder::create()
            ->text(val)
            ->caption(true)
            ->textColor(config->colors.on_surface)
            ->ellipsize(true)
            ->build();
        row->add_view(v, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));

        popup_col->add_view(row);
    };

    add_row("Size", item->size_formatted + " (" + std::to_string(item->size_bytes) + " B)");
    add_row("Location", item->path.string());
    add_row("Modified", item->mtime_formatted);
    add_row("Perms", item->permissions_str);
    if (item->is_symlink && !item->symlink_target.empty()) {
        add_row("Target", item->symlink_target);
    }

    m_active_popup = PopupWindowBuilder::create()
        ->content(popup_col)
        ->width(320)
        ->elevation(6)
        ->cornerRadius(get_card_radius(config))
        ->dismissOnOutsideClick(true)
        ->dismissOnEscape(true)
        ->onDismiss([this]() {
            m_active_popup = nullptr;
        })
        ->build();

    m_active_popup->show_as_dropdown(anchor, PopupGravity::BottomEnd, 0, 4);
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

    auto hero_icon = ImageViewBuilder::create()
        ->source("emblem-documents")
        ->targetSize(48)
        ->fitMode(FitMode::Contain)
        ->build();
    hero_card->add_view(hero_icon, LayoutParams(48, 48, Gravity::CenterHorizontal));

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

        auto icon = ImageViewBuilder::create()
            ->source(it->icon.empty() ? (it->is_category ? "folder" : "text-x-generic") : it->icon)
            ->targetSize(18)
            ->fitMode(FitMode::Contain)
            ->build();
        row->add_view(icon, LayoutParams(18, 18, Gravity::CenterVertical));

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
