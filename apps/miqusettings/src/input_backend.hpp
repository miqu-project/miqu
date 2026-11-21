#pragma once

#include <string>
#include <vector>
#include <memory>

namespace miqu {

struct KeyboardDevice {
    std::string name;
    std::string type = "keyboard";
    std::string layout = "English (US)";
    int repeat_rate = 40;
    int repeat_delay = 250;
    bool is_primary = false;
};

struct PointerDevice {
    std::string name;
    std::string type = "pointer";
    std::string display_type = "Pointing Device";
    bool is_touchpad = false;
    bool is_trackpoint = false;
};

struct TouchDevice {
    std::string name;
    std::string type = "touch";
    std::string mapped_output = "eDP-1";
};

struct InputConfigState {
    // Hardware introspection (from miquctl -j devices)
    std::vector<KeyboardDevice> keyboards;
    std::vector<PointerDevice> pointers;
    std::vector<TouchDevice> touch_devices;

    // 1. Keyboard
    std::string active_layout = "us";
    std::string kb_variant = "";
    std::string kb_options = "";
    std::string kb_model = "";
    int repeat_rate = 40;
    int repeat_delay = 250;

    // 2. Touchpad
    bool tap_to_click = true;
    bool natural_scroll = false;
    bool disable_while_typing = true;
    float pointer_speed = 0.5f; // 0.0 to 1.0 (corresponds to -1.0 to 1.0 in conf)
    std::string accel_profile = "adaptive";

    // 3. Mouse
    float mouse_speed = 0.5f;   // 0.0 to 1.0 (corresponds to -1.0 to 1.0 in conf)
    std::string mouse_accel_profile = "adaptive";
    bool mouse_natural_scroll = false;

    // 4. Touchscreen
    std::string touchscreen_output = "";

    // 5. Continuous Workspace Swipe Physics (input.conf)
    float touchpad_swipe_multiplier = 1.0f;
    float touchscreen_swipe_multiplier = 1.0f;
    int swipe_threshold = 50;
    int touchscreen_threshold = 100;
    float cancel_ratio = 0.30f;
    float min_speed_to_force = 0.3f;
    float edge_resistance = 0.15f;
};

class InputBackend {
public:
    static InputConfigState get_state();

    // Persistent setters that synchronize directly to ~/.config/miquland/input.conf
    static void set_keyboard_layout(const std::string& layout);
    static void set_repeat_info(int rate, int delay);
    static void set_touchpad_options(bool tap_to_click, bool natural_scroll, bool disable_while_typing);
    static void set_pointer_speed(float speed);
    static void set_accel_profile(const std::string& profile);
    static void set_mouse_options(float speed, const std::string& profile, bool natural_scroll);
    static void set_touchscreen_output(const std::string& output);
    static void set_swipe_physics(float tp_mult, float ts_mult, int threshold, int ts_threshold, float cancel, float min_spd, float edge_res);
    static void save_config(const InputConfigState& state);
};

} // namespace miqu
