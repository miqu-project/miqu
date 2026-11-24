#include "miller_view.hpp"
#include "preview_common.hpp"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace miqu {

static void load_autostart_config(AutostartConfig& cfg) {
    if (cfg.loaded) return;

    std::string path;
    const char* home = std::getenv("HOME");
    if (home) {
        fs::path user_conf = fs::path(home) / ".config" / "miquland" / "autostart.conf";
        std::error_code ec;
        if (fs::exists(user_conf, ec)) {
            path = user_conf.string();
        }
    }

    if (path.empty()) {
        for (const char* p : {"/usr/share/miquland/autostart.conf",
                              "/usr/local/share/miquland/autostart.conf",
                              "assets/autostart.conf",
                              "miquland/assets/autostart.conf"}) {
            std::error_code ec;
            if (fs::exists(p, ec)) {
                path = p;
                break;
            }
        }
    }

    if (!path.empty()) {
        std::ifstream in(path);
        if (in.is_open()) {
            std::stringstream ss;
            ss << in.rdbuf();
            cfg.raw_content = ss.str();
        }
    }

    if (cfg.raw_content.empty()) {
        cfg.raw_content =
            "# ==============================================================================\n"
            "# autostart.conf — Session Startup & Daemons (Modular Sourced File)\n"
            "# ==============================================================================\n"
            "autostart {\n"
            "    # Miqu Desktop Ecosystem native components\n"
            "    miqupolkit\n"
            "    miqudesk\n"
            "    miquidle\n"
            "    miqubg\n\n"
            "    # System status bars & notification daemons\n"
            "    miqubar\n"
            "    miqunotify\n\n"
            "    # Network & system tray applets (starts after status bar is ready)\n"
            "    nm-applet\n\n"
            "    # Clipboard manager persistence daemon\n"
            "    wl-paste --watch cliphist store\n\n"
            "    # XDG Desktop Portal backend refresh\n"
            "    systemctl --user restart xdg-desktop-portal-wlr\n"
            "}\n\n"
            "# ==============================================================================\n"
            "# ON-RELOAD COMMANDS (OPTIONAL)\n"
            "# ==============================================================================\n"
            "# Commands executed every time configuration is reloaded.\n"
            "# on_reload {\n"
            "#     notify-send \"miquland\" \"Configuration reloaded\"\n"
            "# }\n";
    }

    cfg.loaded = true;
}

static bool save_autostart_config(const std::string& content) {
    const char* home = std::getenv("HOME");
    if (!home) return false;

    fs::path miqu_dir = fs::path(home) / ".config" / "miquland";
    fs::path autostart_conf = miqu_dir / "autostart.conf";
    std::error_code ec;

    if (!fs::exists(miqu_dir, ec) || !fs::exists(autostart_conf, ec)) {
        // Fresh install / missing config: initialize complete standard configuration via miquland
        std::system("miquland --init-config >/dev/null 2>&1");
    }
    if (!fs::exists(miqu_dir, ec)) {
        fs::create_directories(miqu_dir, ec);
    }

    std::ofstream out(autostart_conf);
    if (!out.is_open()) return false;
    out << content;
    out.flush();
    out.close();

    // Notify miquland to reload configuration if running
    std::system("miquctl reload >/dev/null 2>&1 &");
    return true;
}

void MillerView::render_autostart_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    if (!item || !item->autostart_cfg) return;
    auto& cfg = *item->autostart_cfg;
    load_autostart_config(cfg);

    // ==========================================================================
    // 1. Header Card (Title, File Path, Mode Switcher)
    // ==========================================================================
    auto header_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto top_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(12)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto icon_view = ImageViewBuilder::create()
        ->source(item->icon.empty() ? "system-run" : item->icon)
        ->targetSize(32)
        ->fitMode(FitMode::Contain)
        ->build();
    LayoutParams icon_lp(32, 32);
    icon_lp.gravity = Gravity::CenterVertical;
    top_row->add_view(icon_view, icon_lp);

    auto text_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(3)
        ->build();

    auto title_lbl = TextViewBuilder::create()
        ->text("Autostart Services")
        ->textSize(config->metrics.font_size + 2)
        ->bold(true)
        ->textColor(config->colors.on_surface)
        ->build();
    text_box->add_view(title_lbl);

    auto path_lbl = TextViewBuilder::create()
        ->text("~/.config/miquland/autostart.conf")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    text_box->add_view(path_lbl);

    LayoutParams tb_lp(0, static_cast<int>(LayoutDimension::WrapContent));
    tb_lp.weight = 1.0f;
    tb_lp.gravity = Gravity::CenterVertical;
    top_row->add_view(text_box, tb_lp);

    // Mode Toggle Button (Reading Mode vs Edit Mode)
    auto mode_btn = ButtonBuilder::create()
        ->icon(cfg.is_editing ? "document-edit" : "document-properties")
        ->text(cfg.is_editing ? "Editing Mode" : "Reading Mode")
        ->outlined(!cfg.is_editing)
        ->primary(cfg.is_editing)
        ->padding(14, 6)
        ->cornerRadius(get_btn_radius(config))
        ->onClick([this, item]() {
            if (!item || !item->autostart_cfg) return;
            item->autostart_cfg->is_editing = !item->autostart_cfg->is_editing;
            rebuild_preview_column();
        })
        ->build();
    LayoutParams mb_lp(static_cast<int>(LayoutDimension::WrapContent), 34);
    mb_lp.gravity = Gravity::CenterVertical;
    top_row->add_view(mode_btn, mb_lp);

    LayoutParams htop_lp(static_cast<int>(LayoutDimension::MatchParent), static_cast<int>(LayoutDimension::WrapContent));
    header_card->add_view(top_row, htop_lp);

    auto mode_desc = TextViewBuilder::create()
        ->text(cfg.is_editing
            ? "Edit mode active: modify services and startup commands below. Click 'Save' to apply changes."
            : "Reading mode active: view-only mode for ~/.config/miquland/autostart.conf. Click 'Reading Mode' button to toggle editing.")
        ->caption(true)
        ->textColor(cfg.is_editing ? config->colors.primary : config->colors.on_surface_variant)
        ->build();
    header_card->add_view(mode_desc);

    add_preview_card(header_card);

    // ==========================================================================
    // 2. Editor Card (Multiline EditText)
    // ==========================================================================
    auto editor_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(14, 12)
        ->spacing(10)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto edit_view = EditTextBuilder::create()
        ->text(cfg.raw_content)
        ->hint("# Enter autostart services and commands...")
        ->multiline(true)
        ->editable(cfg.is_editing)
        ->onTextChanged([item](std::shared_ptr<EditText> /*view*/, const std::string& new_text) {
            if (item && item->autostart_cfg) {
                item->autostart_cfg->raw_content = new_text;
            }
        })
        ->build();

    int line_count = 1;
    for (char c : cfg.raw_content) {
        if (c == '\n') line_count++;
    }
    int editor_h = std::clamp(line_count * 22 + 36, 240, 520);

    LayoutParams edit_lp(static_cast<int>(LayoutDimension::MatchParent), editor_h);
    editor_card->add_view(edit_view, edit_lp);

    add_preview_card(editor_card);

    // ==========================================================================
    // 3. Actions Card (Save Button & Status Feedback)
    // ==========================================================================
    auto actions_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(14, 10)
        ->spacing(12)
        ->gravity(Gravity::CenterVertical)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto save_btn = ButtonBuilder::create()
        ->icon("document-save")
        ->text("Save")
        ->primary(true)
        ->padding(20, 8)
        ->cornerRadius(get_btn_radius(config))
        ->onClick([this, item]() {
            if (!item || !item->autostart_cfg) return;
            bool ok = save_autostart_config(item->autostart_cfg->raw_content);
            if (ok) {
                item->last_action_message = "Successfully saved ~/.config/miquland/autostart.conf";
                item->autostart_cfg->is_editing = false;
            } else {
                item->last_action_message = "Error: Failed to save ~/.config/miquland/autostart.conf";
            }
            rebuild_preview_column();
            notify_state_changed();
        })
        ->build();
    LayoutParams save_lp(static_cast<int>(LayoutDimension::WrapContent), 36);
    save_lp.gravity = Gravity::CenterVertical;
    actions_card->add_view(save_btn, save_lp);

    auto reload_btn = ButtonBuilder::create()
        ->icon("view-refresh")
        ->text("Reload Compositor")
        ->outlined(true)
        ->padding(16, 8)
        ->cornerRadius(get_btn_radius(config))
        ->onClick([this, item]() {
            if (!item) return;
            std::system("miquctl reload >/dev/null 2>&1 &");
            item->last_action_message = "Sent configuration reload request to miquland";
            rebuild_preview_column();
            notify_state_changed();
        })
        ->build();
    LayoutParams rel_lp(static_cast<int>(LayoutDimension::WrapContent), 36);
    rel_lp.gravity = Gravity::CenterVertical;
    actions_card->add_view(reload_btn, rel_lp);

    if (!item->last_action_message.empty()) {
        auto msg_lbl = TextViewBuilder::create()
            ->text(item->last_action_message)
            ->caption(true)
            ->textColor(config->colors.primary)
            ->build();
        LayoutParams msg_lp(0, static_cast<int>(LayoutDimension::WrapContent));
        msg_lp.weight = 1.0f;
        msg_lp.gravity = Gravity::CenterVertical;
        actions_card->add_view(msg_lbl, msg_lp);
    }

    add_preview_card(actions_card);
}

} // namespace miqu
