#include "explorer_view.hpp"
#include "preview_common.hpp"
#include <miqutoolkit/core/thread_pool.hpp>
#include <filesystem>
#include <fstream>
#include <thread>
#include <gdk-pixbuf/gdk-pixbuf.h>

namespace fs = std::filesystem;

namespace miqu {

constexpr int MATCH_PARENT = static_cast<int>(LayoutDimension::MatchParent);
constexpr int WRAP_CONTENT = static_cast<int>(LayoutDimension::WrapContent);
constexpr size_t MAX_PREVIEW_BYTES = 65536;

void ExplorerView::render_empty_preview(const std::shared_ptr<Config>& config) {
    if (!m_preview_content) return;

    auto box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->gravity(Gravity::Center)
        ->padding(16, 40)
        ->spacing(8)
        ->build();

    auto icon = ImageViewBuilder::create()
        ->source("system-file-manager")
        ->targetSize(48)
        ->fitMode(FitMode::Contain)
        ->alpha(0.45f)
        ->build();
    box->add_view(icon, LayoutParams(48, 48, Gravity::CenterHorizontal));

    auto title = TextViewBuilder::create()
        ->text("No Item Selected")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface_variant)
        ->center(true)
        ->build();
    box->add_view(title);

    m_preview_content->add_view(box);
}

void ExplorerView::render_multi_preview(const std::vector<std::shared_ptr<FileItem>>& items, const std::shared_ptr<Config>& config) {
    if (!m_preview_content) return;

    uintmax_t total_bytes = 0;
    size_t files = 0;
    size_t dirs = 0;
    for (const auto& it : items) {
        if (it->is_dir) dirs++;
        else {
            files++;
            total_bytes += it->size_bytes;
        }
    }

    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->gravity(Gravity::Center)
        ->padding(16, 24)
        ->spacing(8)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.25f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto icon = ImageViewBuilder::create()
        ->source("edit-select-all")
        ->targetSize(48)
        ->fitMode(FitMode::Contain)
        ->alpha(0.8f)
        ->build();
    card->add_view(icon, LayoutParams(48, 48, Gravity::CenterHorizontal));

    auto title = TextViewBuilder::create()
        ->text(std::to_string(items.size()) + " items selected")
        ->bold(true)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->center(true)
        ->build();
    card->add_view(title);

    std::string detail;
    if (dirs > 0) detail += std::to_string(dirs) + (dirs == 1 ? " folder" : " folders");
    if (files > 0) {
        if (!detail.empty()) detail += ", ";
        detail += std::to_string(files) + (files == 1 ? " file" : " files");
        detail += " (" + FilesModel::format_file_size(total_bytes) + ")";
    }
    if (!detail.empty()) {
        auto sub = TextViewBuilder::create()
            ->text(detail)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant)
            ->center(true)
            ->build();
        card->add_view(sub);
    }

    m_preview_content->add_view(card);
}

void ExplorerView::render_image_preview(const std::shared_ptr<FileItem>& item, const std::shared_ptr<Config>& config) {
    if (!item || !m_preview_content) return;

    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(12, 12)
        ->spacing(10)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.25f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    if (fs::exists(item->path)) {
        auto img = ImageViewBuilder::create()
            ->imageResource(item->path.string())
            ->fitMode(FitMode::Contain)
            ->qualityMode(ImageQuality::ThumbnailFast)
            ->cornerRadius(get_inner_radius(config))
            ->build();
        card->add_view(img, LayoutParams(MATCH_PARENT, 220));
    }

    auto name = TextViewBuilder::create()
        ->text(item->title)
        ->bold(true)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->ellipsize(true)
        ->maxLines(2)
        ->build();
    card->add_view(name);

    auto dims = TextViewBuilder::create()
        ->text(item->size_formatted)
        ->caption(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    card->add_view(dims);

    uint64_t gen = m_preview_load_gen;
    std::weak_ptr<View> weak_self = weak_from_this();
    std::string img_path = item->path.string();
    std::string size_str = item->size_formatted;
    ThreadPool::get().enqueue([this, weak_self, gen, dims, img_path, size_str]() {
        int img_w = 0, img_h = 0;
        if (gdk_pixbuf_get_file_info(img_path.c_str(), &img_w, &img_h) != nullptr && img_w > 0 && img_h > 0) {
            if (auto* eng = AppEngine::instance()) {
                eng->post([this, weak_self, gen, dims, img_w, img_h, size_str]() {
                    if (weak_self.expired() || gen != m_preview_load_gen) return;
                    dims->set_text(std::to_string(img_w) + " × " + std::to_string(img_h) + " px  •  " + size_str);
                });
            }
        }
    });

    if (!item->mtime_formatted.empty()) {
        auto mt = TextViewBuilder::create()
            ->text("Modified: " + item->mtime_formatted)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.8f))
            ->build();
        card->add_view(mt);
    }

    m_preview_content->add_view(card);
}

void ExplorerView::render_text_preview(const std::shared_ptr<FileItem>& item, const std::shared_ptr<Config>& config) {
    if (!item || !m_preview_content) return;

    if (!item->text_loaded) {
        auto loading = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->gravity(Gravity::Center)
            ->padding(16, 24)
            ->spacing(10)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.25f))
            ->cornerRadius(get_card_radius(config))
            ->build();

        auto bar = ProgressBarBuilder::create()
            ->indeterminate(true)
            ->trackHeight(4)
            ->progressColor(config->colors.primary)
            ->trackColor(config->colors.surface_variant)
            ->build();
        loading->add_view(bar, LayoutParams(180, WRAP_CONTENT, Gravity::CenterHorizontal));

        auto lbl = TextViewBuilder::create()
            ->text("Loading preview...")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant)
            ->center(true)
            ->build();
        loading->add_view(lbl);
        m_preview_content->add_view(loading);

        uint64_t gen = m_preview_load_gen;
        std::weak_ptr<View> weak_self = weak_from_this();
        std::string file_path = item->path.string();
        std::thread([this, weak_self, gen, item, file_path]() {
            std::string content;
            std::ifstream ifs(file_path, std::ios::binary);
            if (ifs.is_open()) {
                content.resize(MAX_PREVIEW_BYTES);
                ifs.read(content.data(), static_cast<std::streamsize>(MAX_PREVIEW_BYTES));
                size_t got = static_cast<size_t>(ifs.gcount());
                content.resize(got);
                if (ifs.peek() != EOF) {
                    content += "\n\n... (preview truncated at 64 KB)";
                }
            }
            if (auto* eng = AppEngine::instance()) {
                eng->post([this, weak_self, gen, item, content = std::move(content)]() mutable {
                    if (weak_self.expired() || gen != m_preview_load_gen) return;
                    item->text_content = std::move(content);
                    item->text_loaded = true;
                    rebuild_preview_pane();
                });
            }
        }).detach();
        return;
    }

    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(12, 12)
        ->spacing(8)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.25f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto header = TextViewBuilder::create()
        ->text(item->title + " • " + item->size_formatted)
        ->caption(true)
        ->bold(true)
        ->textColor(config->colors.on_surface_variant)
        ->ellipsize(true)
        ->build();
    card->add_view(header);

    auto body = TextViewBuilder::create()
        ->text(item->text_content.empty() ? "(Empty file)" : item->text_content)
        ->fontFamily("Monospace")
        ->textSize(std::max(10, config->metrics.font_size - 1))
        ->textColor(item->text_content.empty() ? config->colors.on_surface_variant : config->colors.on_surface)
        ->build();
    card->add_view(body, LayoutParams(MATCH_PARENT, WRAP_CONTENT));

    m_preview_content->add_view(card);
}

void ExplorerView::render_metadata_preview(const std::shared_ptr<FileItem>& item, const std::shared_ptr<Config>& config) {
    if (!item || !m_preview_content) return;

    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(14, 16)
        ->spacing(10)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.25f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto icon = ImageViewBuilder::create()
        ->source(item->icon.empty() ? "text-x-generic" : item->icon)
        ->targetSize(56)
        ->fitMode(FitMode::Contain)
        ->build();
    card->add_view(icon, LayoutParams(56, 56, Gravity::CenterHorizontal));

    auto name = TextViewBuilder::create()
        ->text(item->title)
        ->bold(true)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->center(true)
        ->ellipsize(true)
        ->maxLines(2)
        ->build();
    card->add_view(name);

    auto add_row = [&](const std::string& label, const std::string& val) {
        if (val.empty()) return;
        auto row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->spacing(8)
            ->build();
        auto lbl = TextViewBuilder::create()
            ->text(label)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant)
            ->build();
        row->add_view(lbl, LayoutParams(68, WRAP_CONTENT));
        auto v = TextViewBuilder::create()
            ->text(val)
            ->caption(true)
            ->textColor(config->colors.on_surface)
            ->ellipsize(true)
            ->build();
        row->add_view(v, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));
        card->add_view(row);
    };

    add_row("Type", item->is_dir ? "Folder" : item->category_label);
    add_row("Size", item->is_dir ? item->subtitle : item->size_formatted);
    add_row("Modified", item->mtime_formatted);
    add_row("Perms", item->permissions_str);
    if (item->is_symlink && !item->symlink_target.empty()) {
        add_row("Target", item->symlink_target);
    }

    m_preview_content->add_view(card);
}

void ExplorerView::show_file_info_popup(const std::shared_ptr<FileItem>& item, int lx, int ly) {
    auto config = Config::get();
    if (!item || !config || !m_window) return;

    if (m_info_popup && m_info_popup->is_showing()) {
        m_info_popup->dismiss();
        m_info_popup = nullptr;
    }

    auto popup_col = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 16)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.35f))
        ->cornerRadius(get_card_radius(config))
        ->build();

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
        ->text(item->is_dir ? "Folder" : item->category_label)
        ->caption(true)
        ->textColor(config->colors.primary)
        ->build();
    title_col->add_view(cat_view);

    header->add_view(title_col, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));
    popup_col->add_view(header);

    auto div = DividerViewBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->thickness(1)
        ->color(config->colors.outline.with_alpha(0.2f))
        ->build();
    popup_col->add_view(div, LayoutParams(MATCH_PARENT, 1));

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

    m_info_popup = PopupWindowBuilder::create()
        ->content(popup_col)
        ->width(320)
        ->elevation(6)
        ->cornerRadius(get_card_radius(config))
        ->dismissOnOutsideClick(true)
        ->dismissOnEscape(true)
        ->onDismiss([this]() { m_info_popup = nullptr; })
        ->build();

    if (lx >= 0 && ly >= 0) {
        m_info_popup->show_at_location(m_window.get(), lx, ly);
    } else {
        m_info_popup->show_at_location(m_window.get(), 240, 140);
    }
}

} // namespace miqu
