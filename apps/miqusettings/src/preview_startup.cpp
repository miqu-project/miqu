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
    // 1. Header Card (Title, File Path, Action Buttons on Right)
    // ==========================================================================
    auto header_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(16, 12)
        ->spacing(12)
        ->gravity(Gravity::CenterVertical)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto icon_view = ImageViewBuilder::create()
        ->source(item->icon.empty() ? "system-run" : item->icon)
        ->targetSize(32)
        ->fitMode(FitMode::Contain)
        ->build();
    LayoutParams icon_lp(32, 32);
    icon_lp.gravity = Gravity::CenterVertical;
    header_card->add_view(icon_view, icon_lp);

    auto text_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(2)
        ->build();

    auto title_lbl = TextViewBuilder::create()
        ->text("Autostart")
        ->textSize(config->metrics.font_size + 2)
        ->bold(true)
        ->textColor(config->colors.on_surface)
        ->build();
    text_box->add_view(title_lbl);

    std::string path_text = !item->last_action_message.empty()
        ? item->last_action_message
        : "~/.config/miquland/autostart.conf";
    auto path_lbl = TextViewBuilder::create()
        ->text(path_text)
        ->caption(true)
        ->textColor(!item->last_action_message.empty() ? config->colors.primary : config->colors.on_surface_variant)
        ->build();
    text_box->add_view(path_lbl);

    LayoutParams tb_lp(0, static_cast<int>(LayoutDimension::WrapContent));
    tb_lp.weight = 1.0f;
    tb_lp.gravity = Gravity::CenterVertical;
    header_card->add_view(text_box, tb_lp);

    // Header Action Buttons
    auto btn_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(8)
        ->gravity(Gravity::CenterVertical)
        ->build();

    if (!cfg.is_editing) {
        auto edit_btn = ButtonBuilder::create()
            ->icon("document-edit")
            ->text("Edit")
            ->outlined(true)
            ->padding(14, 6)
            ->cornerRadius(get_btn_radius(config))
            ->onClick([this, item]() {
                if (!item || !item->autostart_cfg) return;
                item->autostart_cfg->is_editing = true;
                item->last_action_message.clear();
                rebuild_preview_column();
            })
            ->build();
        LayoutParams eb_lp(static_cast<int>(LayoutDimension::WrapContent), 34);
        eb_lp.gravity = Gravity::CenterVertical;
        btn_box->add_view(edit_btn, eb_lp);
    } else {
        auto cancel_btn = ButtonBuilder::create()
            ->text("Cancel")
            ->outlined(true)
            ->padding(12, 6)
            ->cornerRadius(get_btn_radius(config))
            ->onClick([this, item]() {
                if (!item || !item->autostart_cfg) return;
                item->autostart_cfg->is_editing = false;
                item->autostart_cfg->loaded = false; // re-read original from disk
                item->last_action_message.clear();
                rebuild_preview_column();
            })
            ->build();
        LayoutParams cb_lp(static_cast<int>(LayoutDimension::WrapContent), 34);
        cb_lp.gravity = Gravity::CenterVertical;
        btn_box->add_view(cancel_btn, cb_lp);

        auto save_btn = ButtonBuilder::create()
            ->icon("document-save")
            ->text("Save")
            ->primary(true)
            ->padding(16, 6)
            ->cornerRadius(get_btn_radius(config))
            ->onClick([this, item]() {
                if (!item || !item->autostart_cfg) return;
                bool ok = save_autostart_config(item->autostart_cfg->raw_content);
                if (ok) {
                    item->last_action_message = "Saved ~/.config/miquland/autostart.conf";
                    item->autostart_cfg->is_editing = false;
                } else {
                    item->last_action_message = "Failed to save autostart.conf";
                }
                rebuild_preview_column();
                notify_state_changed();
            })
            ->build();
        LayoutParams sb_lp(static_cast<int>(LayoutDimension::WrapContent), 34);
        sb_lp.gravity = Gravity::CenterVertical;
        btn_box->add_view(save_btn, sb_lp);
    }

    LayoutParams bb_lp(static_cast<int>(LayoutDimension::WrapContent), static_cast<int>(LayoutDimension::WrapContent));
    bb_lp.gravity = Gravity::CenterVertical;
    header_card->add_view(btn_box, bb_lp);

    add_preview_card(header_card);

    // ==========================================================================
    // 2. Editor Card (Multiline EditText)
    // ==========================================================================
    auto editor_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(14, 12)
        ->spacing(8)
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
    int editor_h = std::clamp(line_count * 22 + 36, 260, 560);

    LayoutParams edit_lp(static_cast<int>(LayoutDimension::MatchParent), editor_h);
    editor_card->add_view(edit_view, edit_lp);

    add_preview_card(editor_card);
}

} // namespace miqu
