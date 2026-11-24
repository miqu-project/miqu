#include "miller_view.hpp"
#include "preview_common.hpp"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace miqu {

void load_autostart_config(AutostartConfig& cfg) {
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

bool save_autostart_config(const std::string& content) {
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
    // Editor Card (Clean full-height multiline editor, controls placed on top ribbon)
    // ==========================================================================
    auto editor_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(8)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, cfg.is_editing ? config->colors.primary.with_alpha(0.85f)
                                   : config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto edit_view = EditTextBuilder::create()
        ->text(cfg.raw_content)
        ->fontFamily("Monospace")
        ->hint("# Enter autostart services and commands...")
        ->multiline(true)
        ->drawBackground(false)
        ->editable(cfg.is_editing)
        ->onTextChanged([item](std::shared_ptr<EditText> /*view*/, const std::string& new_text) {
            if (item && item->autostart_cfg) {
                item->autostart_cfg->raw_content = new_text;
            }
        })
        ->build();

    if (cfg.is_editing) {
        edit_view->set_focused(true);
    }

    LayoutParams edit_lp(static_cast<int>(LayoutDimension::MatchParent),
                         static_cast<int>(LayoutDimension::WrapContent));
    editor_card->add_view(edit_view, edit_lp);

    add_preview_card(editor_card);
}

} // namespace miqu
