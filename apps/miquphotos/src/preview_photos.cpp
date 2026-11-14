#include "miller_view.hpp"
#include "preview_common.hpp"
#include <filesystem>
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
        ->text("󰋩")
        ->textSize(static_cast<int>(config->metrics.font_size * 2.5f))
        ->textColor(config->colors.on_surface_variant.with_alpha(0.5f))
        ->center(true)
        ->build();
    card->add_view(icon);

    auto text = TextViewBuilder::create()
        ->text("No Photo Selected")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->center(true)
        ->build();
    card->add_view(text);

    auto hint = TextViewBuilder::create()
        ->text("Select an album or photo to view")
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

    // Hero Album Card
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

    auto icon_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->gravity(Gravity::Center)
        ->backgroundColor(config->colors.surface_variant.with_alpha(0.4f))
        ->cornerRadius(get_inner_radius(config))
        ->build();

    auto album_icon = TextViewBuilder::create()
        ->text("󰋩")
        ->textSize(static_cast<int>(config->metrics.font_size * 2.2f))
        ->textColor(config->colors.primary.with_alpha(0.85f))
        ->center(true)
        ->build();
    icon_box->add_view(album_icon);
    header_row->add_view(icon_box, LayoutParams(80, 80));

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
        ->text("Album • Empty (0 photos)")
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

    // Action buttons
    auto div = LinearLayoutBuilder::create()
        ->backgroundColor(config->colors.outline.with_alpha(0.2f))
        ->build();
    hero_card->add_view(div, LayoutParams(MATCH_PARENT, 1));

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

    // Detail card
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
        ->text("No photos found in this folder.")
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

void MillerView::render_photo_preview(const std::shared_ptr<FileItem>& item, const std::shared_ptr<Config>& config) {
    if (!item) return;

    // 1. Hero Image Card
    auto hero_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 16)
        ->spacing(14)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    // Large Photo Preview Thumbnail
    if (fs::exists(item->path)) {
        auto img = ImageViewBuilder::create()
            ->source(item->path.string())
            ->cornerRadius(get_inner_radius(config))
            ->build();
        hero_card->add_view(img, LayoutParams(MATCH_PARENT, 260));
    }

    // Info Row
    auto header_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(14)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto info_col = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(4)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto title_view = TextViewBuilder::create()
        ->text(item->title)
        ->textSize(static_cast<int>(config->metrics.font_size * 1.25f))
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

    auto open_btn = ButtonBuilder::create()
        ->text("▶ Open")
        ->padding(14, 6)
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

    auto term_btn = ButtonBuilder::create()
        ->text("󰆍 Terminal")
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

    hero_card->add_view(action_row);
    add_preview_card(hero_card);

    // 2. Location Card
    auto location_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(8)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto loc_title = TextViewBuilder::create()
        ->text("File Location")
        ->caption(true)
        ->bold(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    location_card->add_view(loc_title);

    auto loc_path = TextViewBuilder::create()
        ->text(item->path.string())
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->ellipsize(true)
        ->build();
    location_card->add_view(loc_path);

    add_preview_card(location_card);
}

} // namespace miqu
