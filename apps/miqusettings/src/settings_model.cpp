#include "settings_model.hpp"
#include "wifi_backend.hpp"
#include "bluetooth_backend.hpp"
#include "display_backend.hpp"
#include "ethernet_backend.hpp"
#include "firewall_backend.hpp"
#include "airplane_backend.hpp"
#include "dns_backend.hpp"
#include "monitor_backend.hpp"
#include "apps_backend.hpp"
#include "xdg_backend.hpp"
#include "battery_backend.hpp"
#include "plugin_backend.hpp"

namespace miqu {

std::shared_ptr<SettingItem> SettingItem::create_category(
    std::string id, std::string title, std::string icon,
    std::vector<std::shared_ptr<SettingItem>> children
) {
    auto item = std::make_shared<SettingItem>();
    item->id = std::move(id);
    item->title = std::move(title);
    item->subtitle = "";
    item->icon = std::move(icon);
    item->is_category = true;
    item->children = std::move(children);
    return item;
}

std::shared_ptr<SettingItem> SettingItem::create_leaf(
    std::string id, std::string title, std::string subtitle, std::string icon
) {
    auto item = std::make_shared<SettingItem>();
    item->id = std::move(id);
    item->title = std::move(title);
    item->subtitle = std::move(subtitle);
    item->icon = std::move(icon);
    item->is_category = false;
    return item;
}

std::vector<std::shared_ptr<SettingItem>> SettingsModel::build_default_tree() {
    std::vector<std::shared_ptr<SettingItem>> root;

    // ==============================================================================
    // 1. Connectivity Category (Depth 1)
    // ==============================================================================
    {
        // 1.1 Wi-Fi (Direct Leaf node with interactive UI)
        auto wifi = SettingItem::create_leaf("net_wifi", "Wi-Fi", "", "network-wireless");
        wifi->has_switch = true;
        wifi->switch_val = true;
        wifi->switch_label = "Wi-Fi";
        wifi->wifi_networks = {
            {"Miqu-Fiber-5G", "Connected", "network-wireless", true, "E4:70:B8:52:2F:BF", 92, "5 GHz", "36", "WPA3", "1200 Mbit/s", "WPA3 • 5 GHz (Ch 36) • 92% • 1200 Mbit/s • E4:70:B8:52:2F:BF"},
            {"Home-Mesh-Office", "Saved", "network-wireless", true, "A0:B1:C2:D3:E4:F5", 78, "5 GHz", "44", "WPA2", "866 Mbit/s", "WPA2 • 5 GHz (Ch 44) • 78% • 866 Mbit/s • A0:B1:C2:D3:E4:F5"},
            {"Miqu-Lab-Secure", "Saved", "network-wireless", true, "12:34:56:78:9A:BC", 60, "5 GHz", "149", "WPA2 Enterprise", "540 Mbit/s", "WPA2 Enterprise • 5 GHz (Ch 149) • 60% • 540 Mbit/s • 12:34:56:78:9A:BC"},
            {"Cafe-Express-WiFi", "Connect", "network-wireless", false, "FE:DC:BA:98:76:54", 85, "2.4 GHz", "6", "Open", "144 Mbit/s", "Open • 2.4 GHz (Ch 6) • 85% • 144 Mbit/s • FE:DC:BA:98:76:54"},
            {"Airport-Free-5G", "Connect", "network-wireless", false, "00:11:22:33:44:55", 52, "5 GHz", "157", "Open", "300 Mbit/s", "Open • 5 GHz (Ch 157) • 52% • 300 Mbit/s • 00:11:22:33:44:55"}
        };
        // 1.2 Bluetooth (Leaf)
        auto bt = SettingItem::create_leaf("net_bluetooth", "Bluetooth", "", "bluetooth");
        bt->has_switch = true;
        bt->switch_val = true;
        bt->switch_label = "Bluetooth";
        bt->bt_devices = {
            {"Sony WH-1000XM4", "Connected", "audio-headphones", "80%"},
            {"Logitech MX Master 3S", "Paired", "input-mouse", ""},
            {"Keychron K2 Pro", "Paired", "input-keyboard", ""},
            {"DualSense Wireless Controller", "Tap to pair", "input-gaming", ""}
        };

        // 1.3 Ethernet (Leaf)
        auto eth = SettingItem::create_leaf("net_ethernet", "Ethernet", "", "network-wired");
        eth->eth_profile = {
            "Wired Connection 1",
            "Connected",
            "192.168.1.105",
            "255.255.255.0",
            "192.168.1.1",
            "1.1.1.1, 8.8.8.8",
            "Automatic (DHCP)",
            "enp0s31f6",
            "e4:54:e8:1b:9a:2c",
            "24",
            "",
            1000,
            "Full Duplex",
            true,
            true,
            1500
        };

        // 1.4 VPN (Leaf)
        auto vpn = SettingItem::create_leaf("net_vpn", "VPN", "", "network-vpn");
        vpn->is_vpn = true;
        vpn->has_switch = true;
        vpn->switch_label = "VPN Tunnel";

        // 1.5 Airplane Mode (Leaf)
        auto airplane = SettingItem::create_leaf("net_airplane", "Airplane Mode", "", "airplane-mode");
        airplane->has_switch = true;
        airplane->switch_label = "Airplane Mode";

        // 1.6 Private DNS (Leaf)
        auto private_dns = SettingItem::create_leaf("net_private_dns", "Private DNS", "", "security-high");
        private_dns->is_private_dns = true;

        // 1.7 Firewall (Leaf)
        auto firewall = SettingItem::create_leaf("net_firewall", "Firewall", "", "security-high");
        firewall->is_firewall = true;
        firewall->firewall_cfg.enabled = true;
        firewall->firewall_cfg.incoming_policy = "Drop (Deny)";
        firewall->firewall_cfg.outgoing_policy = "Allow";
        firewall->firewall_cfg.selected_preset = "Home";
        firewall->firewall_cfg.allow_kdeconnect = true;
        firewall->firewall_cfg.allow_ssh = false;
        firewall->firewall_cfg.allow_samba = false;
        firewall->firewall_cfg.allow_cups = true;
        firewall->firewall_cfg.monitor_enabled = true;

        // 1.8 Per App Network Access (Leaf)
        auto per_app_net = SettingItem::create_leaf("net_per_app", "Per App Network Access", "", "applications-system");
        per_app_net->is_per_app_network = true;

        root.push_back(SettingItem::create_category(
            "cat_connectivity", "Connectivity", "network-wireless",
            {wifi, bt, eth, vpn, airplane, private_dns, firewall, per_app_net}
        ));
    }

    // ==============================================================================
    // 2. Window Management (Depth 1)
    // ==============================================================================
    {
        auto shared_tiling = std::make_shared<TilingConfig>();

        // 2.1 Layout Mode
        auto tile_mode = SettingItem::create_leaf("tile_mode", "Layout Mode", "", "preferences-system-windows");
        tile_mode->is_tiling_config = true;
        tile_mode->tiling_cfg = shared_tiling;

        // 2.2 Windows
        auto tile_windows = SettingItem::create_leaf("tile_windows", "Windows", "", "preferences-system-windows");
        tile_windows->is_tiling_config = true;
        tile_windows->tiling_cfg = shared_tiling;

        // 2.3 Dwindle (BSP) Tuning
        auto tile_split = SettingItem::create_leaf("tile_split", "Dwindle (BSP) Tuning", "", "view-split-left-right");
        tile_split->is_tiling_config = true;
        tile_split->tiling_cfg = shared_tiling;

        // 2.4 Focus Behaviour
        auto tile_focus = SettingItem::create_leaf("tile_focus", "Focus Behaviour", "", "input-mouse");
        tile_focus->is_tiling_config = true;
        tile_focus->tiling_cfg = shared_tiling;

        root.push_back(SettingItem::create_category(
            "cat_tiling", "Window Management", "preferences-system-windows",
            {tile_mode, tile_windows, tile_split, tile_focus}
        ));
    }

    // ==============================================================================
    // 3. Keybindings (Depth 1)
    // ==============================================================================
    {
        // 3.1 System & Session
        auto kb_system = SettingItem::create_leaf("kb_system", "System & Session", "", "system-shutdown");
        kb_system->is_keybinding_group = true;
        kb_system->keybindings = {
            {"Power Menu", "Open system shutdown, restart and suspend dialog", {"Super", "Shift", "Q"}},
            {"Lock Screen", "Lock session immediately via Wayland lock protocol", {"Super", "Alt", "L"}},
            {"Reload Compositor", "Reload configuration and dynamic theme files without restart", {"Super", "Shift", "R"}},
            {"Toggle Overview", "Interactive live workspace expose and window switcher", {"Super", "Tab"}},
            {"Notification Center", "Toggle slide-out notification drawer and history", {"Super", "N"}}
        };

        // 3.2 Core Applications
        auto kb_apps = SettingItem::create_leaf("kb_apps", "Applications", "", "applications-system");
        kb_apps->is_keybinding_group = true;
        kb_apps->keybindings = {
            {"Terminal", "Spawn default terminal emulator (Kitty)", {"Super", "T"}},
            {"Application Launcher", "Search and launch desktop applications", {"Super", "Space"}},
            {"Universal Search", "Search commands, web, and system actions", {"Super", "S"}},
            {"Web Browser", "Launch default web browser (Firefox)", {"Super", "F"}},
            {"File Manager", "Open terminal file manager (Yazi)", {"Super", "E"}},
            {"Control Center", "System settings & control center", {"Super", "I"}}
        };

        // 3.3 Window Management
        auto kb_windows = SettingItem::create_leaf("kb_windows", "Window Management", "", "preferences-system-windows");
        kb_windows->is_keybinding_group = true;
        kb_windows->keybindings = {
            {"Close Window", "Gracefully close the currently active focused window", {"Super", "Q"}},
            {"Toggle Floating", "Toggle window between floating and tiled layout modes", {"Super", "V"}},
            {"Toggle Fullscreen", "Expand window to fill display area", {"Super", "M"}},
            {"Toggle Layout", "Cycle through available tiling layouts (Dwindle, Stack, etc.)", {"Super", "L"}},
            {"Swap with Master", "Promote active window to dominant master position", {"Super", "Return"}},
            {"Toggle Split", "Toggle horizontal or vertical binary space partitioning", {"Super", "Alt", "Space"}}
        };

        // 3.4 Navigation & Workspaces
        auto kb_nav = SettingItem::create_leaf("kb_nav", "Navigation & Workspaces", "", "video-display");
        kb_nav->is_keybinding_group = true;
        kb_nav->keybindings = {
            {"Next Window", "Shift keyboard focus to next window in current workspace", {"Super", "J"}},
            {"Previous Window", "Shift keyboard focus to previous window", {"Super", "K"}},
            {"Next Workspace", "Switch to adjacent next workspace", {"Super", "Shift", "Right"}},
            {"Previous Workspace", "Switch to adjacent previous workspace", {"Super", "Shift", "Left"}},
            {"Switch to Workspace 1..9", "Jump directly to numbered workspace", {"Super", "Shift", "1..9"}},
            {"Move Window to Workspace 1..9", "Send focused window to numbered workspace", {"Super", "Alt", "1..9"}}
        };

        root.push_back(SettingItem::create_category(
            "cat_keybindings", "Keybindings", "preferences-desktop-keyboard-shortcuts",
            {kb_system, kb_apps, kb_windows, kb_nav}
        ));
    }

    // ==============================================================================
    // 3. Applications (Depth 1)
    // ==============================================================================
    {
        // 3.1 Desktop Applications (Category)
        auto cat_desktop_apps = SettingItem::create_category(
            "cat_desktop_apps", "Desktop Applications", "applications-system"
        );
        cat_desktop_apps->children_provider = []() {
            std::vector<std::shared_ptr<SettingItem>> desktop_items = AppsBackend::get_desktop_app_items();
            if (desktop_items.empty()) {
                auto app_firefox = SettingItem::create_leaf("app_firefox", "Firefox Web Browser", "", "firefox");
                app_firefox->is_installed_app = true;
                app_firefox->app_info.app_name = "Firefox Web Browser";
                app_firefox->app_info.desktop_file = "/usr/share/applications/firefox.desktop";
                app_firefox->app_info.binary_path = "/usr/bin/firefox";
                desktop_items.push_back(app_firefox);
            }
            return desktop_items;
        };

        // 3.2 System Packages (Category)
        auto cat_system_packages = SettingItem::create_category(
            "cat_system_packages", "System Packages", "system-software-install"
        );
        cat_system_packages->children_provider = []() {
            return AppsBackend::get_system_package_items();
        };

        // 3.2 Default Applications (Category)
        auto def_browser = SettingItem::create_leaf("def_browser", "Web Browser", XdgBackend::get_default_app_name("text/html", "Firefox"), "web-browser");
        def_browser->is_default_app_role = true;
        def_browser->default_app_role = {
            "Web Browser", "text/html, x-scheme-handler/http, x-scheme-handler/https",
            {}, "text/html", false
        };
        def_browser->telemetry = {
            {"MIME Types", "text/html, x-scheme-handler/http, x-scheme-handler/https"},
            {"Config File", "~/.config/mimeapps.list"},
            {"Specification", "XDG MIME Applications Specification v1.4"}
        };

        auto def_filemanager = SettingItem::create_leaf("def_filemanager", "File Manager", XdgBackend::get_default_app_name("inode/directory", "Miqu Files"), "system-file-manager");
        def_filemanager->is_default_app_role = true;
        def_filemanager->default_app_role = {
            "File Manager", "inode/directory",
            {}, "inode/directory", false
        };
        def_filemanager->telemetry = {
            {"MIME Types", "inode/directory"},
            {"Config File", "~/.config/mimeapps.list"},
            {"Specification", "XDG MIME Applications Specification v1.4"}
        };

        auto def_pdf = SettingItem::create_leaf("def_pdf", "PDF Reader", XdgBackend::get_default_app_name("application/pdf", "Document Viewer"), "application-pdf");
        def_pdf->is_default_app_role = true;
        def_pdf->default_app_role = {
            "PDF Reader", "application/pdf",
            {}, "application/pdf", false
        };
        def_pdf->telemetry = {
            {"MIME Types", "application/pdf"},
            {"Config File", "~/.config/mimeapps.list"},
            {"Specification", "XDG MIME Applications Specification v1.4"}
        };

        auto def_image = SettingItem::create_leaf("def_image", "Image Viewer", XdgBackend::get_default_app_name("image/png", "Gallery"), "image-x-generic");
        def_image->is_default_app_role = true;
        def_image->default_app_role = {
            "Image Viewer", "image/png, image/jpeg, image/webp, image/gif, image/svg+xml",
            {}, "image/png", false
        };
        def_image->telemetry = {
            {"MIME Types", "image/png, image/jpeg, image/webp, image/gif, image/svg+xml"},
            {"Config File", "~/.config/mimeapps.list"},
            {"Specification", "XDG MIME Applications Specification v1.4"}
        };

        auto def_editor = SettingItem::create_leaf("def_editor", "Text Editor", XdgBackend::get_default_app_name("text/plain", "Geany"), "accessories-text-editor");
        def_editor->is_default_app_role = true;
        def_editor->default_app_role = {
            "Text Editor", "text/plain, text/markdown",
            {}, "text/plain", false
        };
        def_editor->telemetry = {
            {"MIME Types", "text/plain, text/markdown"},
            {"Config File", "~/.config/mimeapps.list"},
            {"Specification", "XDG MIME Applications Specification v1.4"}
        };

        auto def_video = SettingItem::create_leaf("def_video", "Video Player", XdgBackend::get_default_app_name("video/mp4", "Celluloid"), "video-x-generic");
        def_video->is_default_app_role = true;
        def_video->default_app_role = {
            "Video Player", "video/mp4, video/mkv, video/webm, video/x-matroska",
            {}, "video/mp4", false
        };
        def_video->telemetry = {
            {"MIME Types", "video/mp4, video/mkv, video/webm, video/x-matroska"},
            {"Config File", "~/.config/mimeapps.list"},
            {"Specification", "XDG MIME Applications Specification v1.4"}
        };

        auto def_music = SettingItem::create_leaf("def_music", "Music Player", XdgBackend::get_default_app_name("audio/mpeg", "Miqu Music"), "audio-x-generic");
        def_music->is_default_app_role = true;
        def_music->default_app_role = {
            "Music Player", "audio/mpeg, audio/flac, audio/ogg, audio/wav",
            {}, "audio/mpeg", false
        };
        def_music->telemetry = {
            {"MIME Types", "audio/mpeg, audio/flac, audio/ogg, audio/wav"},
            {"Config File", "~/.config/mimeapps.list"},
            {"Specification", "XDG MIME Applications Specification v1.4"}
        };

        auto def_mail = SettingItem::create_leaf("def_mail", "Mail Reader", XdgBackend::get_default_app_name("x-scheme-handler/mailto", "Thunderbird"), "internet-mail");
        def_mail->is_default_app_role = true;
        def_mail->default_app_role = {
            "Mail Reader", "x-scheme-handler/mailto, message/rfc822",
            {}, "x-scheme-handler/mailto", false
        };
        def_mail->telemetry = {
            {"MIME Types", "x-scheme-handler/mailto, message/rfc822"},
            {"Config File", "~/.config/mimeapps.list"},
            {"Specification", "XDG MIME Applications Specification v1.4"}
        };

        auto def_terminal = SettingItem::create_leaf("def_terminal", "Terminal Emulator", "Kitty", "utilities-terminal");
        def_terminal->is_default_app_role = true;
        def_terminal->default_app_role = {
            "Terminal Emulator", "x-scheme-handler/terminal",
            {}, "x-scheme-handler/terminal", false
        };
        def_terminal->telemetry = {
            {"MIME Types", "x-scheme-handler/terminal"},
            {"Config File", "~/.config/mimeapps.list"},
            {"Specification", "XDG MIME Applications Specification v1.4"}
        };

        auto cat_default_apps = SettingItem::create_category(
            "cat_default_apps", "Default Applications", "preferences-desktop-default-applications",
            {def_browser, def_filemanager, def_pdf, def_image, def_editor, def_video, def_music, def_mail, def_terminal}
        );

        root.push_back(SettingItem::create_category(
            "cat_applications", "Applications", "applications-system",
            {cat_desktop_apps, cat_system_packages, cat_default_apps}
        ));
    }

    // ==============================================================================
    // 5. Display (Depth 1)
    // ==============================================================================
    {
        // 5.1 Monitors
        auto cat_monitors = SettingItem::create_category("cat_monitors", "Monitors", "video-display");
        cat_monitors->children_provider = []() {
            std::vector<std::shared_ptr<SettingItem>> mon_items;
            auto connected_monitors = MonitorBackend::get_monitors();
            for (const auto& mon_info : connected_monitors) {
                std::string id = "mon_" + mon_info.name;
                std::string title = !mon_info.description.empty() ? mon_info.description : mon_info.name;
                auto mon_item = SettingItem::create_leaf(id, title, "", "video-display");
                MonitorBackend::sync_monitor_item(mon_item, mon_info);
                mon_items.push_back(mon_item);
            }

            if (mon_items.empty()) {
                auto def_item = SettingItem::create_leaf("mon_edp1", "eDP-1", "", "video-display");
                MonitorInfo def_info;
                MonitorBackend::sync_monitor_item(def_item, def_info);
                mon_items.push_back(def_item);
            }
            return mon_items;
        };

        // 5.2 Brightness
        auto disp_bright = SettingItem::create_leaf("disp_bright", "Brightness", "", "display-brightness");
        disp_bright->is_display_brightness = true;
        disp_bright->brightness_val = 0.85f;

        // 5.3 Screen Timeout & Lock (Unified Inactivity & Security)
        auto disp_timeout_lock = SettingItem::create_leaf("disp_timeout_lock", "Screen Timeout & Lock", "", "system-lock-screen");
        disp_timeout_lock->is_timeout_and_lock = true;
        disp_timeout_lock->timeout_lock_cfg.lock_timeout_minutes = 1;
        disp_timeout_lock->timeout_lock_cfg.display_off_minutes = 5;
        disp_timeout_lock->timeout_lock_cfg.sleep_minutes = 15;
        disp_timeout_lock->timeout_lock_cfg.hibernate_delay_minutes = 120;
        disp_timeout_lock->timeout_lock_cfg.sleep_mode = "Hybrid Sleep (Recommended)";
        disp_timeout_lock->timeout_lock_cfg.lid_action = "Sleep";
        disp_timeout_lock->timeout_lock_cfg.power_button_action = "Show power menu (miqupower)";
        disp_timeout_lock->timeout_lock_cfg.critical_battery_action = "Hibernate";
        disp_timeout_lock->timeout_lock_cfg.dim_screen = true;
        disp_timeout_lock->timeout_lock_cfg.prevent_sleep_media = true;
        disp_timeout_lock->timeout_lock_cfg.energy_saver_battery = true;
        disp_timeout_lock->timeout_lock_cfg.clamshell_mode = true;
        disp_timeout_lock->timeout_lock_cfg.lock_before_sleep = true;
        disp_timeout_lock->timeout_lock_cfg.lock_on_display_off = true;
        disp_timeout_lock->timeout_lock_cfg.require_password_immediately = true;
        disp_timeout_lock->timeout_lock_cfg.grace_period_seconds = 0;
        disp_timeout_lock->timeout_lock_cfg.lockscreen_show_power_actions = true;
        disp_timeout_lock->timeout_lock_cfg.lockscreen_24h_clock = true;

        // 5.4 Night Light
        auto disp_nightlight = SettingItem::create_leaf("disp_nightlight", "Night Light", "", "preferences-desktop-display");
        disp_nightlight->is_display_nightlight = true;

        root.push_back(SettingItem::create_category(
            "cat_display", "Display", "video-display",
            {cat_monitors, disp_bright, disp_timeout_lock, disp_nightlight}
        ));
    }

    // ==============================================================================
    // 6. Appearance (Depth 1)
    // ==============================================================================
    {
        auto ws_wallpaper = SettingItem::create_leaf("ws_wallpaper", "Wallpaper and Style", "", "preferences-desktop-wallpaper");
        ws_wallpaper->is_wallpaper = true;

        auto ws_icon_packs = SettingItem::create_leaf("ws_icon_packs", "Icon Packs", "", "preferences-desktop-icons");
        ws_icon_packs->is_icon_packs = true;

        auto ws_cursor = SettingItem::create_leaf("ws_cursor", "Cursor", "", "input-mouse");
        ws_cursor->is_cursor = true;

        auto ws_gaps_border = SettingItem::create_leaf("ws_gaps_border", "Window Gaps and Border", "", "preferences-system-windows");
        ws_gaps_border->is_gaps_border = true;

        auto ws_login_screen = SettingItem::create_leaf("ws_login_screen", "Login Screen (Display Manager)", "", "system-lock-screen");
        ws_login_screen->is_login_screen = true;

        root.push_back(SettingItem::create_category(
            "cat_appearance", "Appearance", "preferences-desktop-theme",
            {ws_wallpaper, ws_icon_packs, ws_cursor, ws_gaps_border, ws_login_screen}
        ));
    }

    // ==============================================================================
    // 7. Plugins (Depth 1)
    // ==============================================================================
    {
        auto cat_plugins = SettingItem::create_category(
            "cat_plugins", "Plugins", "preferences-plugin"
        );
        cat_plugins->children_provider = []() {
            return PluginBackend::get_plugin_items();
        };

        root.push_back(cat_plugins);
    }

    // ==============================================================================
    // 3. Sound (Depth 1)
    // ==============================================================================
    {
        // 3.1 Speaker (Leaf)
        auto speaker = SettingItem::create_leaf("sound_speaker", "Speaker", "", "audio-volume-high");
        speaker->has_slider = true;
        speaker->slider_val = 0.70f;
        speaker->slider_label = "Volume";
        speaker->slider_unit = "%";
        speaker->has_switch = true;
        speaker->switch_val = false;
        speaker->switch_label = "Mute";
        speaker->has_secondary_switch = false;
        speaker->has_action = false;

        // 3.2 Mic (Leaf)
        auto mic = SettingItem::create_leaf("sound_mic", "Mic", "", "audio-input-microphone");
        mic->has_slider = true;
        mic->slider_val = 0.82f;
        mic->slider_label = "Gain";
        mic->slider_unit = "%";
        mic->has_switch = true;
        mic->switch_val = false;
        mic->switch_label = "Mute";
        mic->has_secondary_switch = false;
        mic->has_action = false;

        root.push_back(SettingItem::create_category(
            "cat_audio", "Sound", "audio-volume-high",
            {speaker, mic}
        ));
    }

    // ==============================================================================
    // 9. Keyboard & Mouse (Depth 1)
    // ==============================================================================
    {
        // 9.1 Keyboards & Layout (Leaf)
        auto in_kb = SettingItem::create_leaf("input_keyboards", "Keyboards & Layout", "", "input-keyboard");
        in_kb->is_input_keyboards = true;

        // 9.2 Typing & Repeat Speed (Leaf)
        auto in_rep = SettingItem::create_leaf("input_repeat", "Typing & Repeat Speed", "", "preferences-desktop-keyboard");
        in_rep->is_input_repeat = true;

        // 9.3 Pointers & Touchpad (Leaf)
        auto in_ptr = SettingItem::create_leaf("input_pointers", "Pointers & Touchpad", "", "input-mouse");
        in_ptr->is_input_pointers = true;

        // 9.4 Touchscreen & Gestures (Leaf)
        auto in_touch = SettingItem::create_leaf("input_touch", "Touchscreen & Gestures", "", "input-touchpad");
        in_touch->is_input_touch = true;

        root.push_back(SettingItem::create_category(
            "cat_input", "Keyboard & Mouse", "input-keyboard",
            {in_kb, in_rep, in_ptr, in_touch}
        ));
    }

    // ==============================================================================
    // 8. Storage (Depth 1)
    // ==============================================================================
    {
        auto stor_root = SettingItem::create_leaf("stor_root", "Root Filesystem (/)", "", "drive-harddisk");
        stor_root->has_slider = true;
        stor_root->slider_val = 0.42f;
        stor_root->slider_label = "Storage Used (42% • 420.5 GB of 1.0 TB)";
        stor_root->slider_unit = "%";
        stor_root->slider_read_only = true;
        stor_root->has_switch = true;
        stor_root->switch_val = true;
        stor_root->switch_label = "Periodic SSD TRIM (fstrim.timer)";
        stor_root->has_secondary_switch = true;
        stor_root->secondary_switch_val = true;
        stor_root->secondary_switch_label = "ZSTD Transparent Compression";
        stor_root->has_action = true;
        stor_root->action_label = "Run Drive Health Diagnostic (SMART)";

        auto stor_home = SettingItem::create_leaf("stor_home", "User Data (~/)", "", "folder");
        stor_home->has_slider = true;
        stor_home->slider_val = 0.28f;
        stor_home->slider_label = "User Directory Space (280.4 GB)";
        stor_home->slider_unit = "%";
        stor_home->slider_read_only = true;
        stor_home->has_switch = true;
        stor_home->switch_val = true;
        stor_home->switch_label = "Automatic Cache Cleaner";
        stor_home->has_secondary_switch = false;
        stor_home->has_action = true;
        stor_home->action_label = "Clean Package & App Cache";

        root.push_back(SettingItem::create_category(
            "cat_storage", "Storage", "drive-harddisk",
            {stor_root, stor_home}
        ));
    }

    // ==============================================================================
    // 7. Battery (Depth 1)
    // ==============================================================================
    {
        auto shared_battery_cfg = std::make_shared<BatteryConfig>();

        // 7.1 Battery Info (Leaf) - loaded lazily on visibility
        auto battery_info = SettingItem::create_leaf("bat_info", "Battery Info", "", "battery");
        battery_info->is_battery_info = true;
        battery_info->battery_cfg = shared_battery_cfg;

        // 7.2 Power & Performance Mode (Leaf) - loaded lazily on visibility
        auto power_mode = SettingItem::create_leaf("bat_power_mode", "Power & Performance Mode", "Balanced", "preferences-system-power");
        power_mode->is_power_mode = true;
        power_mode->battery_cfg = shared_battery_cfg;

        auto cat_battery = SettingItem::create_category(
            "cat_battery", "Battery", "battery",
            {battery_info, power_mode}
        );

        root.push_back(cat_battery);
    }

    // ==============================================================================
    // 8. Specifications & Diagnostics (Depth 1)
    // ==============================================================================
    {
        // 8.1 Device Specifications (Subcategory)
        auto sys_board = SettingItem::create_leaf("spec_sys_board", "Computer & Motherboard", "", "computer");
        sys_board->is_hardware_spec = true;
        auto sys_firmware = SettingItem::create_leaf("spec_sys_firmware", "BIOS & Firmware", "", "security-high");
        sys_firmware->is_hardware_spec = true;
        auto sys_cpu = SettingItem::create_leaf("spec_sys_cpu", "Processor (CPU)", "", "cpu");
        sys_cpu->is_hardware_spec = true;
        auto sys_thermals = SettingItem::create_leaf("spec_sys_thermals", "Thermals & Cooling", "", "temperature-warm");
        sys_thermals->is_hardware_spec = true;
        auto sys_memory = SettingItem::create_leaf("spec_sys_memory", "Memory (RAM & Swap)", "", "drive-harddisk");
        sys_memory->is_hardware_spec = true;
        auto sys_uptime = SettingItem::create_leaf("spec_sys_uptime", "System Activity & Uptime", "", "preferences-system-time");
        sys_uptime->is_hardware_spec = true;

        auto cat_spec_sys = SettingItem::create_category(
            "cat_spec_system", "Device Specifications", "computer",
            {sys_board, sys_firmware, sys_cpu, sys_thermals, sys_memory, sys_uptime}
        );

        // 8.2 Operating System & Services (Subcategory)
        auto srv_os = SettingItem::create_leaf("spec_srv_os", "Operating System & Shell", "", "distributor-logo");
        srv_os->is_hardware_spec = true;
        auto srv_desktop = SettingItem::create_leaf("spec_srv_desktop", "Desktop Daemons", "", "preferences-desktop-theme");
        srv_desktop->is_hardware_spec = true;
        auto srv_network = SettingItem::create_leaf("spec_srv_network", "Network Services", "", "network-wireless");
        srv_network->is_hardware_spec = true;
        auto srv_security = SettingItem::create_leaf("spec_srv_security", "Security & Firewalls", "", "security-high");
        srv_security->is_hardware_spec = true;
        auto srv_sound = SettingItem::create_leaf("spec_srv_sound", "Sound & Media", "", "audio-volume-high");
        srv_sound->is_hardware_spec = true;
        auto srv_power = SettingItem::create_leaf("spec_srv_power", "Power Daemons", "", "battery");
        srv_power->is_hardware_spec = true;
        auto srv_packages = SettingItem::create_leaf("spec_srv_packages", "Package Ecosystem", "", "system-software-install");
        srv_packages->is_hardware_spec = true;

        auto cat_spec_srv = SettingItem::create_category(
            "cat_spec_services", "Operating System & Services", "applications-system",
            {srv_os, srv_desktop, srv_network, srv_security, srv_sound, srv_power, srv_packages}
        );

        // 8.3 Displays & Graphics (Subcategory)
        auto disp_gpu = SettingItem::create_leaf("spec_disp_gpu", "Graphics Processor (GPU)", "", "video-display");
        disp_gpu->is_hardware_spec = true;
        auto disp_monitors = SettingItem::create_leaf("spec_disp_monitors", "Connected Displays", "", "video-display");
        disp_monitors->is_hardware_spec = true;
        auto disp_backlight = SettingItem::create_leaf("spec_disp_backlight", "Backlight Controller", "", "display-brightness");
        disp_backlight->is_hardware_spec = true;

        auto cat_spec_disp = SettingItem::create_category(
            "cat_spec_display", "Displays & Graphics", "video-display",
            {disp_gpu, disp_monitors, disp_backlight}
        );

        // 8.4 Audio Subsystem (Subcategory)
        auto aud_cards = SettingItem::create_leaf("spec_aud_cards", "Controllers & Codecs", "", "audio-card");
        aud_cards->is_hardware_spec = true;
        auto aud_endpoints = SettingItem::create_leaf("spec_aud_endpoints", "Endpoints & Graph", "", "audio-volume-high");
        aud_endpoints->is_hardware_spec = true;

        auto cat_spec_aud = SettingItem::create_category(
            "cat_spec_audio", "Audio Subsystem", "audio-volume-high",
            {aud_cards, aud_endpoints}
        );

        // 8.5 Network Adapters (Subcategory)
        auto net_wifi = SettingItem::create_leaf("spec_net_wifi", "Wireless Interfaces", "", "network-wireless");
        net_wifi->is_hardware_spec = true;
        auto net_ethernet = SettingItem::create_leaf("spec_net_ethernet", "Ethernet Interfaces", "", "network-wired");
        net_ethernet->is_hardware_spec = true;
        auto net_bluetooth = SettingItem::create_leaf("spec_net_bluetooth", "Bluetooth Controllers", "", "bluetooth");
        net_bluetooth->is_hardware_spec = true;
        auto net_routing = SettingItem::create_leaf("spec_net_routing", "DNS & Routing", "", "network-workgroup");
        net_routing->is_hardware_spec = true;

        auto cat_spec_net = SettingItem::create_category(
            "cat_spec_network", "Network Adapters", "network-wireless",
            {net_wifi, net_ethernet, net_bluetooth, net_routing}
        );

        // 8.6 Storage & Drives (Subcategory)
        auto stor_drives = SettingItem::create_leaf("spec_stor_drives", "Physical Drives", "", "drive-harddisk");
        stor_drives->is_hardware_spec = true;
        auto stor_mounts = SettingItem::create_leaf("spec_stor_mounts", "Filesystem Mounts", "", "folder");
        stor_mounts->is_hardware_spec = true;
        auto stor_zram = SettingItem::create_leaf("spec_stor_zram", "In-Memory Swap (ZRAM)", "", "memory");
        stor_zram->is_hardware_spec = true;

        auto cat_spec_stor = SettingItem::create_category(
            "cat_spec_storage", "Storage & Drives", "drive-harddisk",
            {stor_drives, stor_mounts, stor_zram}
        );

        // 8.7 Battery & Power (Subcategory)
        auto pwr_battery = SettingItem::create_leaf("spec_pwr_battery", "Battery Telemetry", "", "battery");
        pwr_battery->is_hardware_spec = true;
        auto pwr_adapter = SettingItem::create_leaf("spec_pwr_adapter", "AC Power Adapter", "", "battery-charging");
        pwr_adapter->is_hardware_spec = true;
        auto pwr_profile = SettingItem::create_leaf("spec_pwr_profile", "Platform Power Profile", "", "preferences-system-power");
        pwr_profile->is_hardware_spec = true;

        auto cat_spec_pwr = SettingItem::create_category(
            "cat_spec_power", "Battery & Power", "battery",
            {pwr_battery, pwr_adapter, pwr_profile}
        );

        // 8.8 Input Devices (Subcategory)
        auto inp_keyboards = SettingItem::create_leaf("spec_inp_keyboards", "Keyboards", "", "input-keyboard");
        inp_keyboards->is_hardware_spec = true;
        auto inp_pointers = SettingItem::create_leaf("spec_inp_pointers", "Mice & Touchpads", "", "input-mouse");
        inp_pointers->is_hardware_spec = true;
        auto inp_touch = SettingItem::create_leaf("spec_inp_touch", "Touchscreens & Digitizers", "", "input-touchpad");
        inp_touch->is_hardware_spec = true;
        auto inp_switches = SettingItem::create_leaf("spec_inp_switches", "Chassis Switches", "", "preferences-system");
        inp_switches->is_hardware_spec = true;

        auto cat_spec_inp = SettingItem::create_category(
            "cat_spec_input", "Input Devices", "input-keyboard",
            {inp_keyboards, inp_pointers, inp_touch, inp_switches}
        );

        root.push_back(SettingItem::create_category(
            "cat_hardware_specs", "Specifications & Diagnostics", "preferences-system-hardware",
            {cat_spec_sys, cat_spec_srv, cat_spec_disp, cat_spec_aud, cat_spec_net, cat_spec_stor, cat_spec_pwr, cat_spec_inp}
        ));
    }

    // ==============================================================================
    // 9. System (Depth 1)
    // ==============================================================================
    {
        // 9.1 About (Leaf)
        auto sys_about = SettingItem::create_leaf("sys_about", "About", "Arch Linux • miquland desktop", "help-about");
        sys_about->has_action = true;
        sys_about->action_label = "Check for Ecosystem Updates";

        root.push_back(SettingItem::create_category(
            "cat_system", "System", "preferences-system",
            {sys_about}
        ));
    }

    return root;
}

} // namespace miqu
