#include "miller_view.hpp"
#include "preview_common.hpp"
#include <filesystem>
#include <iostream>
#include <cstdlib>
#include <gdk-pixbuf/gdk-pixbuf.h>

namespace fs = std::filesystem;

namespace miqu {

constexpr int MATCH_PARENT = static_cast<int>(LayoutDimension::MatchParent);
constexpr int WRAP_CONTENT = static_cast<int>(LayoutDimension::WrapContent);

void MillerView::render_empty_preview(const std::shared_ptr<Config>& config) {
    if (!m_preview_frame) return;

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

    m_preview_frame->add_view(card, LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity::Center));
}

void MillerView::render_empty_folder_preview(const std::shared_ptr<FileItem>& item, const std::shared_ptr<Config>& config) {
    if (!item || !m_preview_frame) return;

    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->gravity(Gravity::Center)
        ->padding(24, 32)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto icon = TextViewBuilder::create()
        ->text("󰋩")
        ->textSize(static_cast<int>(config->metrics.font_size * 2.5f))
        ->textColor(config->colors.primary.with_alpha(0.85f))
        ->center(true)
        ->build();
    card->add_view(icon);

    auto title = TextViewBuilder::create()
        ->text(item->title)
        ->textSize(static_cast<int>(config->metrics.font_size * 1.2f))
        ->bold(true)
        ->textColor(config->colors.on_surface)
        ->center(true)
        ->build();
    card->add_view(title);

    auto hint = TextViewBuilder::create()
        ->text("Album • Empty (0 photos)")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.6f))
        ->center(true)
        ->build();
    card->add_view(hint);

    m_preview_frame->add_view(card, LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity::Center));
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

    // 3. Floating HUD overlay card over the photo at the bottom
    auto overlay_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(14, 10)
        ->spacing(8)
        ->backgroundColor(config->colors.surface.with_alpha(0.85f))
        ->stroke(1, config->colors.outline.with_alpha(0.35f))
        ->cornerRadius(get_card_radius(config))
        ->margin(12, 0, 12, 12)
        ->build();

    // Top info row
    auto info_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(2)
        ->build();

    auto title_view = TextViewBuilder::create()
        ->text(item->title)
        ->textSize(static_cast<int>(config->metrics.font_size * 1.15f))
        ->bold(true)
        ->textColor(config->colors.on_surface)
        ->ellipsize(true)
        ->build();
    info_row->add_view(title_view);

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

    if (!meta_line.empty()) {
        auto meta_view = TextViewBuilder::create()
            ->text(meta_line)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.85f))
            ->ellipsize(true)
            ->build();
        info_row->add_view(meta_view);
    }

    overlay_card->add_view(info_row, LayoutParams(MATCH_PARENT, WRAP_CONTENT));

    // Action buttons row
    auto action_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(8)
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

    overlay_card->add_view(action_row, LayoutParams(MATCH_PARENT, WRAP_CONTENT));

    m_preview_frame->add_view(overlay_card, LayoutParams(MATCH_PARENT, WRAP_CONTENT, Gravity::Bottom));
}

} // namespace miqu
