#include "miller_view.hpp"
#include "plugin_backend.hpp"
#include "preview_common.hpp"

namespace miqu {

void MillerView::render_plugin_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    if (!item) return;

    // 1. Header / Hero Switch Card
    auto hero_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(16, 14)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto left_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(4)
        ->build();

    std::string title_text = !item->plugin_info.display_name.empty() ? item->plugin_info.display_name : item->title;
    auto title_view = TextViewBuilder::create()
        ->text(title_text)
        ->bold(true)
        ->textColor(config->colors.on_surface)
        ->build();
    left_box->add_view(title_view);

    std::string status_desc = item->plugin_info.is_active
        ? "Active — Loaded dynamically in compositor session"
        : "Inactive — Plugin is unloaded";
    if (item->plugin_info.path.empty()) {
        status_desc = "No plugin binary available";
    }

    auto status_view = TextViewBuilder::create()
        ->text(status_desc)
        ->caption(true)
        ->textColor(item->plugin_info.is_active ? config->colors.primary : config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    left_box->add_view(status_view);

    LayoutParams left_lp;
    left_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    left_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    left_lp.weight = 1.0f;
    left_lp.gravity = Gravity::CenterVertical;
    hero_card->add_view(left_box, left_lp);

    if (!item->plugin_info.path.empty()) {
        auto sw = SwitchBuilder::create()
            ->checked(item->plugin_info.is_active)
            ->onCheckedChanged([this, item](bool checked) {
                PluginBackend::set_plugin_active(item->plugin_info.path, checked);
                item->plugin_info.is_active = checked;
                item->subtitle = checked ? "Active" : "Inactive";
                rebuild_current_column();
                deferred_rebuild_preview();
                notify_state_changed();
            })
            ->build();

        LayoutParams sw_lp;
        sw_lp.width = 44;
        sw_lp.height = 24;
        sw_lp.gravity = Gravity::CenterVertical;
        hero_card->add_view(sw, sw_lp);
    }

    add_preview_card(hero_card);

    // 2. Metadata & Description Card
    auto info_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto info_hdr = TextViewBuilder::create()
        ->text("Plugin Details & Metadata")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    info_card->add_view(info_hdr);

    auto desc_view = TextViewBuilder::create()
        ->text(item->plugin_info.description.empty() ? "No description provided." : item->plugin_info.description)
        ->textColor(config->colors.on_surface)
        ->build();
    info_card->add_view(desc_view);

    if (!item->plugin_info.path.empty()) {
        std::vector<std::pair<std::string, std::string>> meta_rows = {
            {"Binary Path", item->plugin_info.path},
            {"Version", item->plugin_info.version},
            {"Author", item->plugin_info.author},
            {"ABI Version", "v" + std::to_string(item->plugin_info.abi_version) + " (miquland)"},
            {"Compositor Reload", "Inotify Live Reload (Zero restart)"}
        };

        for (const auto& [lbl, val] : meta_rows) {
            auto row = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->spacing(8)
                ->build();

            auto r_lbl = TextViewBuilder::create()
                ->text(lbl)
                ->caption(true)
                ->textColor(config->colors.on_surface_variant)
                ->build();
            LayoutParams r_lbl_lp;
            r_lbl_lp.width = 150;
            r_lbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
            row->add_view(r_lbl, r_lbl_lp);

            auto r_val = TextViewBuilder::create()
                ->text(val)
                ->textColor(config->colors.on_surface)
                ->build();
            LayoutParams r_val_lp;
            r_val_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            r_val_lp.height = static_cast<int>(LayoutDimension::WrapContent);
            r_val_lp.weight = 1.0f;
            row->add_view(r_val, r_val_lp);

            info_card->add_view(row);
        }
    }

    add_preview_card(info_card);
}

} // namespace miqu
