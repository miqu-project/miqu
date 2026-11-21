#include "miller_view.hpp"
#include "preview_common.hpp"
#include "input_backend.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace miqu {

namespace {

constexpr int MATCH_PARENT = static_cast<int>(LayoutDimension::MatchParent);
constexpr int WRAP_CONTENT = static_cast<int>(LayoutDimension::WrapContent);



std::shared_ptr<View> create_keyboards_list_card(
    const std::vector<KeyboardDevice>& keyboards,
    const std::shared_ptr<Config>& config
) {
    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(10)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto title = TextViewBuilder::create()
        ->text("Connected Keyboards (" + std::to_string(keyboards.size()) + ")")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    card->add_view(title);

    for (const auto& kb : keyboards) {
        auto row_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->padding(10, 8)
            ->spacing(10)
            ->gravity(Gravity::CenterVertical)
            ->backgroundColor(config->colors.surface_variant.with_alpha(0.35f))
            ->cornerRadius(get_inner_radius(config))
            ->build();

        auto kb_icon = ImageViewBuilder::create()
            ->source("input-keyboard")
            ->targetSize(20)
            ->fitMode(FitMode::Contain)
            ->alpha(kb.is_primary ? 1.0f : 0.6f)
            ->build();
        row_card->add_view(kb_icon, LayoutParams(20, 20, Gravity::CenterVertical));

        auto text_col = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto name_row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->spacing(8)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto name = TextViewBuilder::create()
            ->text(kb.name)
            ->textSize(config->metrics.font_size)
            ->bold(kb.is_primary)
            ->textColor(config->colors.on_surface)
            ->build();
        name_row->add_view(name);

        if (kb.is_primary) {
            auto badge = TextViewBuilder::create()
                ->text("Primary")
                ->caption(true)
                ->textColor(config->colors.primary)
                ->build();
            name_row->add_view(badge);
        }
        text_col->add_view(name_row);

        auto details = TextViewBuilder::create()
            ->text("Layout: " + kb.layout + "  •  Repeat: " + std::to_string(kb.repeat_rate) + "/s (" + std::to_string(kb.repeat_delay) + "ms)")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        text_col->add_view(details);

        row_card->add_view(text_col, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));
        card->add_view(row_card);
    }

    LayoutParams card_lp;
    card_lp.width = MATCH_PARENT;
    card_lp.height = WRAP_CONTENT;
    card->set_layout_params(card_lp);
    return card;
}

std::shared_ptr<View> create_layout_selector_card(
    const std::string& active_layout,
    std::function<void(const std::string&)> on_select,
    const std::shared_ptr<Config>& config
) {
    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(10)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto title = TextViewBuilder::create()
        ->text("Quick Layout Selection")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    card->add_view(title);

    auto subtitle = TextViewBuilder::create()
        ->text("Switch default XKB keymap layout across compositor & applications:")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    card->add_view(subtitle);

    auto btn_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(8)
        ->gravity(Gravity::CenterVertical)
        ->build();

    struct LayoutOption { std::string code; std::string label; };
    std::vector<LayoutOption> layouts = {
        {"us", "US (ANSI)"},
        {"gb", "UK (ISO)"},
        {"de", "DE (German)"},
        {"fr", "FR (French)"},
        {"es", "ES (Spanish)"},
        {"ru", "RU (Russian)"},
        {"jp", "JP (Japanese)"}
    };

    for (const auto& l : layouts) {
        bool is_cur = (l.code == active_layout);
        auto btn = ButtonBuilder::create()
            ->text(l.code)
            ->style(is_cur ? ButtonStyle::Primary : ButtonStyle::Standard)
            ->onClick([on_select, code = l.code]() {
                if (on_select) on_select(code);
            })
            ->build();
        btn_row->add_view(btn, LayoutParams(WRAP_CONTENT, 30));
    }

    card->add_view(btn_row);

    LayoutParams card_lp;
    card_lp.width = MATCH_PARENT;
    card_lp.height = WRAP_CONTENT;
    card->set_layout_params(card_lp);
    return card;
}

std::shared_ptr<View> create_typetest_sandbox_card(
    const std::string& title_str,
    const std::string& hint_str,
    const std::shared_ptr<Config>& config
) {
    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(8)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto title = TextViewBuilder::create()
        ->text(title_str)
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    card->add_view(title);

    auto desc = TextViewBuilder::create()
        ->text(hint_str)
        ->caption(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    card->add_view(desc);

    auto search_box = SearchViewBuilder::create()
        ->hint("Click here and type to test keystrokes...")
        ->focused(false)
        ->focusBorder(false)
        ->padding(10, 8)
        ->elevation(0)
        ->backgroundColor(config->colors.surface_variant.with_alpha(0.4f))
        ->build();
    search_box->set_focused(false);
    if (auto et = search_box->get_edit_text()) {
        et->set_focused(false);
    }
    card->add_view(search_box, LayoutParams(MATCH_PARENT, 36));

    LayoutParams card_lp;
    card_lp.width = MATCH_PARENT;
    card_lp.height = WRAP_CONTENT;
    card->set_layout_params(card_lp);
    return card;
}

std::shared_ptr<View> create_repeat_delay_card(
    int delay_ms,
    std::function<void(int)> on_delay_changed,
    const std::shared_ptr<Config>& config
) {
    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(10)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto header = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto title = TextViewBuilder::create()
        ->text("Repeat Delay (Sensitivity)")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    header->add_view(title, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));

    auto val_label = TextViewBuilder::create()
        ->text(std::to_string(delay_ms) + " ms")
        ->bold(true)
        ->textColor(config->colors.primary)
        ->build();
    header->add_view(val_label, LayoutParams(WRAP_CONTENT, WRAP_CONTENT));
    card->add_view(header);

    auto desc = TextViewBuilder::create()
        ->text("Time before character repetition begins while holding a key down:")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    card->add_view(desc);

    // Slider range 150ms to 1000ms
    float norm = (delay_ms - 150.0f) / (1000.0f - 150.0f);
    auto slider = SliderBuilder::create()
        ->value(std::clamp(norm, 0.0f, 1.0f))
        ->onValueChanged([val_label, on_delay_changed](float val, bool user) {
            (void)user;
            int ms = static_cast<int>(std::round(150.0f + val * 850.0f));
            val_label->set_text(std::to_string(ms) + " ms");
            val_label->request_redraw();
            if (on_delay_changed) on_delay_changed(ms);
        })
        ->build();
    card->add_view(slider, LayoutParams(MATCH_PARENT, 26));

    // Preset buttons
    auto presets = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(8)
        ->build();

    struct Preset { std::string label; int ms; };
    std::vector<Preset> list = {
        {"Short (250ms)", 250},
        {"Default (600ms)", 600},
        {"Long (800ms)", 800}
    };

    for (const auto& p : list) {
        auto btn = ButtonBuilder::create()
            ->text(p.label)
            ->style(ButtonStyle::Standard)
            ->onClick([on_delay_changed, ms = p.ms]() {
                if (on_delay_changed) on_delay_changed(ms);
            })
            ->build();
        presets->add_view(btn, LayoutParams(WRAP_CONTENT, 28));
    }
    card->add_view(presets);

    LayoutParams card_lp;
    card_lp.width = MATCH_PARENT;
    card_lp.height = WRAP_CONTENT;
    card->set_layout_params(card_lp);
    return card;
}

std::shared_ptr<View> create_repeat_rate_card(
    int rate_cps,
    std::function<void(int)> on_rate_changed,
    const std::shared_ptr<Config>& config
) {
    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(10)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto header = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto title = TextViewBuilder::create()
        ->text("Repeat Rate (Speed)")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    header->add_view(title, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));

    auto val_label = TextViewBuilder::create()
        ->text(std::to_string(rate_cps) + " char/s")
        ->bold(true)
        ->textColor(config->colors.primary)
        ->build();
    header->add_view(val_label, LayoutParams(WRAP_CONTENT, WRAP_CONTENT));
    card->add_view(header);

    auto desc = TextViewBuilder::create()
        ->text("Characters repeated per second while continuously holding a key down:")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    card->add_view(desc);

    // Slider range 10 to 50 char/s
    float norm = (rate_cps - 10.0f) / (50.0f - 10.0f);
    auto slider = SliderBuilder::create()
        ->value(std::clamp(norm, 0.0f, 1.0f))
        ->onValueChanged([val_label, on_rate_changed](float val, bool user) {
            (void)user;
            int r = static_cast<int>(std::round(10.0f + val * 40.0f));
            val_label->set_text(std::to_string(r) + " char/s");
            val_label->request_redraw();
            if (on_rate_changed) on_rate_changed(r);
        })
        ->build();
    card->add_view(slider, LayoutParams(MATCH_PARENT, 26));

    // Preset buttons
    auto presets = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(8)
        ->build();

    struct Preset { std::string label; int r; };
    std::vector<Preset> list = {
        {"Slow (15/s)", 15},
        {"Balanced (25/s)", 25},
        {"Fast (40/s)", 40}
    };

    for (const auto& p : list) {
        auto btn = ButtonBuilder::create()
            ->text(p.label)
            ->style(ButtonStyle::Standard)
            ->onClick([on_rate_changed, r = p.r]() {
                if (on_rate_changed) on_rate_changed(r);
            })
            ->build();
        presets->add_view(btn, LayoutParams(WRAP_CONTENT, 28));
    }
    card->add_view(presets);

    LayoutParams card_lp;
    card_lp.width = MATCH_PARENT;
    card_lp.height = WRAP_CONTENT;
    card->set_layout_params(card_lp);
    return card;
}

std::shared_ptr<View> create_touchpad_ergonomics_card(
    bool tap_to_click,
    bool natural_scroll,
    bool disable_while_typing,
    std::function<void(bool, bool, bool)> on_update,
    const std::shared_ptr<Config>& config
) {
    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto title = TextViewBuilder::create()
        ->text("Touchpad Ergonomics")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    card->add_view(title);

    auto add_toggle_row = [&](const std::string& name, const std::string& desc_text, bool cur_val, auto cb) {
        auto row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->spacing(10)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto col = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto lbl = TextViewBuilder::create()
            ->text(name)
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        col->add_view(lbl);

        auto d = TextViewBuilder::create()
            ->text(desc_text)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        col->add_view(d);

        row->add_view(col, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));

        auto sw = SwitchBuilder::create()
            ->checked(cur_val)
            ->onCheckedChanged([cb](bool chk) {
                cb(chk);
            })
            ->build();
        row->add_view(sw, LayoutParams(WRAP_CONTENT, WRAP_CONTENT));
        card->add_view(row);
    };

    add_toggle_row(
        "Natural Scrolling",
        "Content moves smoothly in the direction of finger motion",
        natural_scroll,
        [tap_to_click, disable_while_typing, on_update](bool chk) {
            if (on_update) on_update(tap_to_click, chk, disable_while_typing);
        }
    );

    add_toggle_row(
        "Tap-to-Click",
        "Tap surface with 1 finger for left-click, 2 for right-click",
        tap_to_click,
        [natural_scroll, disable_while_typing, on_update](bool chk) {
            if (on_update) on_update(chk, natural_scroll, disable_while_typing);
        }
    );

    add_toggle_row(
        "Disable While Typing",
        "Ignore accidental touchpad palm touches during keyboard typing",
        disable_while_typing,
        [tap_to_click, natural_scroll, on_update](bool chk) {
            if (on_update) on_update(tap_to_click, natural_scroll, chk);
        }
    );

    LayoutParams card_lp;
    card_lp.width = MATCH_PARENT;
    card_lp.height = WRAP_CONTENT;
    card->set_layout_params(card_lp);
    return card;
}

std::shared_ptr<View> create_pointer_speed_card(
    float speed_val,
    const std::string& accel_profile,
    std::function<void(float)> on_speed_changed,
    std::function<void(const std::string&)> on_profile_changed,
    const std::shared_ptr<Config>& config
) {
    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(10)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto header = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto title = TextViewBuilder::create()
        ->text("Pointer Speed & Acceleration")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    header->add_view(title, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));

    int pct = static_cast<int>(std::round(speed_val * 100.0f));
    auto val_label = TextViewBuilder::create()
        ->text(std::to_string(pct) + "%")
        ->bold(true)
        ->textColor(config->colors.primary)
        ->build();
    header->add_view(val_label, LayoutParams(WRAP_CONTENT, WRAP_CONTENT));
    card->add_view(header);

    auto slider = SliderBuilder::create()
        ->value(std::clamp(speed_val, 0.0f, 1.0f))
        ->onValueChanged([val_label, on_speed_changed](float val, bool user) {
            (void)user;
            int p = static_cast<int>(std::round(val * 100.0f));
            val_label->set_text(std::to_string(p) + "%");
            val_label->request_redraw();
            if (on_speed_changed) on_speed_changed(val);
        })
        ->build();
    card->add_view(slider, LayoutParams(MATCH_PARENT, 26));

    // Acceleration Profile
    auto prof_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(8)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto prof_lbl = TextViewBuilder::create()
        ->text("Acceleration Profile:")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    prof_row->add_view(prof_lbl);

    bool is_adap = (accel_profile == "adaptive");
    auto btn_adap = ButtonBuilder::create()
        ->text("Adaptive (Dynamic)")
        ->style(is_adap ? ButtonStyle::Primary : ButtonStyle::Standard)
        ->onClick([on_profile_changed]() {
            if (on_profile_changed) on_profile_changed("adaptive");
        })
        ->build();
    prof_row->add_view(btn_adap, LayoutParams(WRAP_CONTENT, 28));

    auto btn_flat = ButtonBuilder::create()
        ->text("Flat (1:1 Raw)")
        ->style(!is_adap ? ButtonStyle::Primary : ButtonStyle::Standard)
        ->onClick([on_profile_changed]() {
            if (on_profile_changed) on_profile_changed("flat");
        })
        ->build();
    prof_row->add_view(btn_flat, LayoutParams(WRAP_CONTENT, 28));

    card->add_view(prof_row);

    LayoutParams card_lp;
    card_lp.width = MATCH_PARENT;
    card_lp.height = WRAP_CONTENT;
    card->set_layout_params(card_lp);
    return card;
}

std::shared_ptr<View> create_pointers_list_card(
    const std::vector<PointerDevice>& pointers,
    const std::shared_ptr<Config>& config
) {
    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(10)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto title = TextViewBuilder::create()
        ->text("Detected Pointing Devices (" + std::to_string(pointers.size()) + ")")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    card->add_view(title);

    for (const auto& ptr : pointers) {
        auto row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->padding(10, 8)
            ->spacing(10)
            ->gravity(Gravity::CenterVertical)
            ->backgroundColor(config->colors.surface_variant.with_alpha(0.35f))
            ->cornerRadius(get_inner_radius(config))
            ->build();

        std::string icon_str = ptr.is_touchpad ? "input-touchpad" : "input-mouse";
        auto icon = ImageViewBuilder::create()
            ->source(icon_str)
            ->targetSize(20)
            ->fitMode(FitMode::Contain)
            ->build();
        row->add_view(icon, LayoutParams(20, 20, Gravity::CenterVertical));

        auto text_col = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto name = TextViewBuilder::create()
            ->text(ptr.name)
            ->textSize(config->metrics.font_size)
            ->bold(true)
            ->textColor(config->colors.on_surface)
            ->build();
        text_col->add_view(name);

        auto type_lbl = TextViewBuilder::create()
            ->text(ptr.display_type)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        text_col->add_view(type_lbl);

        row->add_view(text_col, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));
        card->add_view(row);
    }

    LayoutParams card_lp;
    card_lp.width = MATCH_PARENT;
    card_lp.height = WRAP_CONTENT;
    card->set_layout_params(card_lp);
    return card;
}

std::shared_ptr<View> create_touch_devices_card(
    const std::vector<TouchDevice>& touch_devices,
    const std::shared_ptr<Config>& config
) {
    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(10)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto title = TextViewBuilder::create()
        ->text("Detected Touchscreen Digitizers (" + std::to_string(touch_devices.size()) + ")")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    card->add_view(title);

    if (touch_devices.empty()) {
        auto empty = TextViewBuilder::create()
            ->text("No physical touchscreen digitizers detected on this seat")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant)
            ->build();
        card->add_view(empty);
    } else {
        for (const auto& tch : touch_devices) {
            auto row = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->padding(10, 8)
                ->spacing(10)
                ->gravity(Gravity::CenterVertical)
                ->backgroundColor(config->colors.surface_variant.with_alpha(0.35f))
                ->cornerRadius(get_inner_radius(config))
                ->build();

            auto icon = ImageViewBuilder::create()
                ->source("input-touchpad")
                ->targetSize(20)
                ->fitMode(FitMode::Contain)
                ->build();
            row->add_view(icon, LayoutParams(20, 20, Gravity::CenterVertical));

            auto col = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->spacing(2)
                ->build();

            auto name = TextViewBuilder::create()
                ->text(tch.name)
                ->textSize(config->metrics.font_size)
                ->bold(true)
                ->textColor(config->colors.on_surface)
                ->build();
            col->add_view(name);

            auto sub = TextViewBuilder::create()
                ->text("Capacitive Multi-Touch  •  Associated Display: " + tch.mapped_output)
                ->caption(true)
                ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
                ->build();
            col->add_view(sub);

            row->add_view(col, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));
            card->add_view(row);
        }
    }

    LayoutParams card_lp;
    card_lp.width = MATCH_PARENT;
    card_lp.height = WRAP_CONTENT;
    card->set_layout_params(card_lp);
    return card;
}

std::shared_ptr<View> create_workspace_swipe_card(
    const InputConfigState& state,
    std::function<void(float, float, int, int, float, float, float)> on_swipe_changed,
    const std::shared_ptr<Config>& config
) {
    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(14)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto title = TextViewBuilder::create()
        ->text("Continuous Workspace Swipe Physics")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    card->add_view(title);

    auto subtitle = TextViewBuilder::create()
        ->text("Kinematics and transition tuning for multi-finger touchpad and touchscreen workspace navigation (workspace_swipe in input.conf)")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    card->add_view(subtitle);

    // 1. Touchpad Swipe Multiplier
    {
        auto row = LinearLayoutBuilder::create()->orientation(Orientation::Horizontal)->gravity(Gravity::CenterVertical)->build();
        auto lbl = TextViewBuilder::create()->text("Touchpad Multiplier")->textSize(config->metrics.font_size)->textColor(config->colors.on_surface)->build();
        row->add_view(lbl, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));

        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << state.touchpad_swipe_multiplier << "x";
        auto val_lbl = TextViewBuilder::create()->text(ss.str())->bold(true)->textColor(config->colors.primary)->build();
        row->add_view(val_lbl, LayoutParams(WRAP_CONTENT, WRAP_CONTENT));
        card->add_view(row);

        float norm = std::clamp((state.touchpad_swipe_multiplier - 0.1f) / 3.9f, 0.0f, 1.0f);
        auto slider = SliderBuilder::create()
            ->value(norm)
            ->onValueChanged([val_lbl, state, on_swipe_changed](float val, bool user) {
                (void)user;
                float actual = 0.1f + val * 3.9f;
                std::ostringstream s;
                s << std::fixed << std::setprecision(2) << actual << "x";
                val_lbl->set_text(s.str());
                val_lbl->request_redraw();
                if (on_swipe_changed) on_swipe_changed(actual, state.touchscreen_swipe_multiplier, state.swipe_threshold, state.touchscreen_threshold, state.cancel_ratio, state.min_speed_to_force, state.edge_resistance);
            })
            ->build();
        card->add_view(slider, LayoutParams(MATCH_PARENT, 26));
    }

    // 2. Touchscreen Swipe Multiplier
    {
        auto row = LinearLayoutBuilder::create()->orientation(Orientation::Horizontal)->gravity(Gravity::CenterVertical)->build();
        auto lbl = TextViewBuilder::create()->text("Touchscreen Multiplier")->textSize(config->metrics.font_size)->textColor(config->colors.on_surface)->build();
        row->add_view(lbl, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));

        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << state.touchscreen_swipe_multiplier << "x";
        auto val_lbl = TextViewBuilder::create()->text(ss.str())->bold(true)->textColor(config->colors.primary)->build();
        row->add_view(val_lbl, LayoutParams(WRAP_CONTENT, WRAP_CONTENT));
        card->add_view(row);

        float norm = std::clamp((state.touchscreen_swipe_multiplier - 0.1f) / 3.9f, 0.0f, 1.0f);
        auto slider = SliderBuilder::create()
            ->value(norm)
            ->onValueChanged([val_lbl, state, on_swipe_changed](float val, bool user) {
                (void)user;
                float actual = 0.1f + val * 3.9f;
                std::ostringstream s;
                s << std::fixed << std::setprecision(2) << actual << "x";
                val_lbl->set_text(s.str());
                val_lbl->request_redraw();
                if (on_swipe_changed) on_swipe_changed(state.touchpad_swipe_multiplier, actual, state.swipe_threshold, state.touchscreen_threshold, state.cancel_ratio, state.min_speed_to_force, state.edge_resistance);
            })
            ->build();
        card->add_view(slider, LayoutParams(MATCH_PARENT, 26));
    }

    // 3. Swipe Distance Threshold
    {
        auto row = LinearLayoutBuilder::create()->orientation(Orientation::Horizontal)->gravity(Gravity::CenterVertical)->build();
        auto lbl = TextViewBuilder::create()->text("Swipe Distance Threshold")->textSize(config->metrics.font_size)->textColor(config->colors.on_surface)->build();
        row->add_view(lbl, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));

        auto val_lbl = TextViewBuilder::create()->text(std::to_string(state.swipe_threshold) + " px")->bold(true)->textColor(config->colors.primary)->build();
        row->add_view(val_lbl, LayoutParams(WRAP_CONTENT, WRAP_CONTENT));
        card->add_view(row);

        float norm = std::clamp((state.swipe_threshold - 10.0f) / 190.0f, 0.0f, 1.0f);
        auto slider = SliderBuilder::create()
            ->value(norm)
            ->onValueChanged([val_lbl, state, on_swipe_changed](float val, bool user) {
                (void)user;
                int actual = static_cast<int>(std::round(10.0f + val * 190.0f));
                val_lbl->set_text(std::to_string(actual) + " px");
                val_lbl->request_redraw();
                if (on_swipe_changed) on_swipe_changed(state.touchpad_swipe_multiplier, state.touchscreen_swipe_multiplier, actual, state.touchscreen_threshold, state.cancel_ratio, state.min_speed_to_force, state.edge_resistance);
            })
            ->build();
        card->add_view(slider, LayoutParams(MATCH_PARENT, 26));
    }

    // 4. Cancel Return Ratio
    {
        auto row = LinearLayoutBuilder::create()->orientation(Orientation::Horizontal)->gravity(Gravity::CenterVertical)->build();
        auto lbl = TextViewBuilder::create()->text("Cancel Return Ratio")->textSize(config->metrics.font_size)->textColor(config->colors.on_surface)->build();
        row->add_view(lbl, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));

        int pct = static_cast<int>(std::round(state.cancel_ratio * 100.0f));
        auto val_lbl = TextViewBuilder::create()->text(std::to_string(pct) + "%")->bold(true)->textColor(config->colors.primary)->build();
        row->add_view(val_lbl, LayoutParams(WRAP_CONTENT, WRAP_CONTENT));
        card->add_view(row);

        float norm = std::clamp((state.cancel_ratio - 0.10f) / 0.70f, 0.0f, 1.0f);
        auto slider = SliderBuilder::create()
            ->value(norm)
            ->onValueChanged([val_lbl, state, on_swipe_changed](float val, bool user) {
                (void)user;
                float actual = 0.10f + val * 0.70f;
                int p = static_cast<int>(std::round(actual * 100.0f));
                val_lbl->set_text(std::to_string(p) + "%");
                val_lbl->request_redraw();
                if (on_swipe_changed) on_swipe_changed(state.touchpad_swipe_multiplier, state.touchscreen_swipe_multiplier, state.swipe_threshold, state.touchscreen_threshold, actual, state.min_speed_to_force, state.edge_resistance);
            })
            ->build();
        card->add_view(slider, LayoutParams(MATCH_PARENT, 26));
    }

    LayoutParams card_lp;
    card_lp.width = MATCH_PARENT;
    card_lp.height = WRAP_CONTENT;
    card->set_layout_params(card_lp);
    return card;
}

std::shared_ptr<View> create_mouse_ergonomics_card(
    bool natural_scroll,
    std::function<void(bool)> on_natural_scroll_changed,
    const std::shared_ptr<Config>& config
) {
    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto title = TextViewBuilder::create()
        ->text("Mouse Scrolling & Behavior")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    card->add_view(title);

    auto row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(10)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto col = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(2)
        ->build();

    auto t = TextViewBuilder::create()
        ->text("Natural (Inverted) Scrolling")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    col->add_view(t);

    auto d = TextViewBuilder::create()
        ->text("Invert scroll wheel direction to match touch drag motion")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    col->add_view(d);

    row->add_view(col, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));

    auto sw = SwitchBuilder::create()
        ->checked(natural_scroll)
        ->onCheckedChanged([on_natural_scroll_changed](bool chk) {
            if (on_natural_scroll_changed) on_natural_scroll_changed(chk);
        })
        ->build();
    row->add_view(sw, LayoutParams(WRAP_CONTENT, WRAP_CONTENT));
    card->add_view(row);

    LayoutParams card_lp;
    card_lp.width = MATCH_PARENT;
    card_lp.height = WRAP_CONTENT;
    card->set_layout_params(card_lp);
    return card;
}

} // namespace

void MillerView::render_keyboards_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    (void)item;
    auto state = InputBackend::get_state();

    add_preview_card(create_keyboards_list_card(state.keyboards, config));

    add_preview_card(create_layout_selector_card(
        state.active_layout,
        [this](const std::string& l) {
            InputBackend::set_keyboard_layout(l);
            rebuild_preview_column();
        },
        config
    ));

    add_preview_card(create_typetest_sandbox_card(
        "Typing Test Sandbox",
        "Click the test box below and type to test layout mapping and keystrokes live:",
        config
    ));
}

void MillerView::render_repeat_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    (void)item;
    auto state = InputBackend::get_state();

    add_preview_card(create_repeat_delay_card(
        state.repeat_delay,
        [this, state](int d) {
            InputBackend::set_repeat_info(state.repeat_rate, d);
        },
        config
    ));

    add_preview_card(create_repeat_rate_card(
        state.repeat_rate,
        [this, state](int r) {
            InputBackend::set_repeat_info(r, state.repeat_delay);
        },
        config
    ));

    add_preview_card(create_typetest_sandbox_card(
        "Key Repeat Sandbox",
        "Hold down any key inside the test field below to test repeat delay and speed:",
        config
    ));
}

void MillerView::render_pointers_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    (void)item;
    auto state = InputBackend::get_state();

    add_preview_card(create_touchpad_ergonomics_card(
        state.tap_to_click,
        state.natural_scroll,
        state.disable_while_typing,
        [this](bool tap, bool nat, bool dwt) {
            InputBackend::set_touchpad_options(tap, nat, dwt);
            rebuild_preview_column();
        },
        config
    ));

    add_preview_card(create_pointer_speed_card(
        state.pointer_speed,
        state.accel_profile,
        [](float spd) {
            InputBackend::set_pointer_speed(spd);
        },
        [this](const std::string& prof) {
            InputBackend::set_accel_profile(prof);
            rebuild_preview_column();
        },
        config
    ));

    std::vector<PointerDevice> touchpads;
    for (const auto& p : state.pointers) {
        if (p.is_touchpad) touchpads.push_back(p);
    }
    add_preview_card(create_pointers_list_card(touchpads.empty() ? state.pointers : touchpads, config));
}

void MillerView::render_mouse_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    (void)item;
    auto state = InputBackend::get_state();

    add_preview_card(create_mouse_ergonomics_card(
        state.mouse_natural_scroll,
        [this, state](bool nat) {
            InputBackend::set_mouse_options(state.mouse_speed, state.mouse_accel_profile, nat);
            rebuild_preview_column();
        },
        config
    ));

    add_preview_card(create_pointer_speed_card(
        state.mouse_speed,
        state.mouse_accel_profile,
        [state](float spd) {
            InputBackend::set_mouse_options(spd, state.mouse_accel_profile, state.mouse_natural_scroll);
        },
        [this, state](const std::string& prof) {
            InputBackend::set_mouse_options(state.mouse_speed, prof, state.mouse_natural_scroll);
            rebuild_preview_column();
        },
        config
    ));

    std::vector<PointerDevice> mice;
    for (const auto& p : state.pointers) {
        if (!p.is_touchpad) mice.push_back(p);
    }
    add_preview_card(create_pointers_list_card(mice.empty() ? state.pointers : mice, config));
}

void MillerView::render_touch_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    (void)item;
    auto state = InputBackend::get_state();

    add_preview_card(create_touch_devices_card(state.touch_devices, config));

    add_preview_card(create_workspace_swipe_card(
        state,
        [this](float tp, float ts, int th, int ts_th, float cancel, float min_spd, float edge_res) {
            InputBackend::set_swipe_physics(tp, ts, th, ts_th, cancel, min_spd, edge_res);
            rebuild_preview_column();
        },
        config
    ));
}

} // namespace miqu
