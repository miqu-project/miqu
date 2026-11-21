#include "input_backend.hpp"
#include "apps_backend.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <algorithm>
#include <mutex>
#include <regex>

namespace fs = std::filesystem;

namespace miqu {

namespace {

static std::mutex s_input_mutex;
static bool s_baseline_loaded = false;

// Active state values loaded from input.conf
static std::string s_active_layout = "us";
static std::string s_kb_variant = "";
static std::string s_kb_options = "";
static std::string s_kb_model = "";
static int s_repeat_rate = 40;
static int s_repeat_delay = 250;

static bool s_tap_to_click = true;
static bool s_natural_scroll = false;
static bool s_disable_while_typing = true;
static float s_pointer_speed = 0.5f; // maps to 0.0 accel_speed
static std::string s_accel_profile = "adaptive";

static float s_mouse_speed = 0.5f; // maps to 0.0 accel_speed
static std::string s_mouse_accel_profile = "adaptive";
static bool s_mouse_natural_scroll = false;

static std::string s_touchscreen_output = "";

static float s_touchpad_swipe_multiplier = 1.0f;
static float s_touchscreen_swipe_multiplier = 1.0f;
static int s_swipe_threshold = 50;
static int s_touchscreen_threshold = 100;
static float s_cancel_ratio = 0.30f;
static float s_min_speed_to_force = 0.3f;
static float s_edge_resistance = 0.15f;

std::string get_input_config_path() {
    const char* home = getenv("HOME");
    if (home) {
        fs::path user_conf = fs::path(home) / ".config" / "miquland" / "input.conf";
        std::error_code ec;
        if (fs::exists(user_conf, ec)) return user_conf.string();
    }
    for (const char* p : {"/usr/share/miquland/input.conf", "/usr/local/share/miquland/input.conf", "assets/input.conf"}) {
        if (fs::exists(p)) {
            return p;
        }
    }
    return "";
}

void save_config_locked() {
    const char* home = getenv("HOME");
    if (!home) return;

    fs::path miqu_dir = fs::path(home) / ".config" / "miquland";
    std::error_code ec;
    if (!fs::exists(miqu_dir, ec) || !fs::exists(miqu_dir / "input.conf", ec)) {
        // Tampering / saving: initialize user config
        std::system("miquland --init-config >/dev/null 2>&1");
    }
    if (!fs::exists(miqu_dir, ec)) {
        fs::create_directories(miqu_dir, ec);
    }
    fs::path conf_path = miqu_dir / "input.conf";

    float tp_raw_speed = (s_pointer_speed * 2.0f) - 1.0f; // 0.0..1.0 -> -1.0..1.0
    float mouse_raw_speed = (s_mouse_speed * 2.0f) - 1.0f;

    std::ostringstream ss;
    ss << "# ==============================================================================\n"
       << "# input.conf — Hardware Peripherals & Input Configuration (Modular Sourced File)\n"
       << "# Miqu Desktop Ecosystem Wayland Compositor\n"
       << "# ==============================================================================\n"
       << "# Hardware peripheral configuration, XKB keymaps, touchpads, and continuous swipe physics.\n"
       << "input {\n"
       << "    # Keyboard layout & repeat behavior:\n"
       << "    keyboard {\n"
       << "        layout = " << (s_active_layout.empty() ? "us" : s_active_layout) << "\n";
    if (!s_kb_variant.empty()) ss << "        variant = " << s_kb_variant << "\n";
    if (!s_kb_options.empty()) ss << "        options = " << s_kb_options << "\n";
    if (!s_kb_model.empty()) ss << "        model = " << s_kb_model << "\n";
    ss << "        repeat_rate = " << s_repeat_rate << "\n"
       << "        repeat_delay = " << s_repeat_delay << "\n"
       << "    }\n\n"
       << "    # Touchpad gestures & ergonomics:\n"
       << "    touchpad {\n"
       << "        tap_to_click = " << (s_tap_to_click ? "true" : "false") << "\n"
       << "        natural_scroll = " << (s_natural_scroll ? "true" : "false") << "\n"
       << "        disable_while_typing = " << (s_disable_while_typing ? "true" : "false") << "\n"
       << "        accel_speed = " << std::fixed << std::setprecision(2) << tp_raw_speed << "\n"
       << "        accel_profile = " << s_accel_profile << "\n"
       << "    }\n\n"
       << "    # Mouse pointer configuration:\n"
       << "    mouse {\n"
       << "        accel_speed = " << std::fixed << std::setprecision(2) << mouse_raw_speed << "\n"
       << "        accel_profile = " << s_mouse_accel_profile << "\n"
       << "        natural_scroll = " << (s_mouse_natural_scroll ? "true" : "false") << "\n"
       << "    }\n\n"
       << "    # Touchscreen hardware association:\n"
       << "    touchscreen {\n";
    if (!s_touchscreen_output.empty()) {
        ss << "        output = " << s_touchscreen_output << "\n";
    }
    ss << "    }\n\n"
       << "    # Multi-finger trackpad and touchscreen workspace swipe physics:\n"
       << "    workspace_swipe {\n"
       << "        touchpad_multiplier = " << std::fixed << std::setprecision(2) << s_touchpad_swipe_multiplier << "\n"
       << "        touchscreen_multiplier = " << std::fixed << std::setprecision(2) << s_touchscreen_swipe_multiplier << "\n"
       << "        swipe_threshold = " << s_swipe_threshold << "\n"
       << "        touchscreen_threshold = " << s_touchscreen_threshold << "\n"
       << "        cancel_ratio = " << std::fixed << std::setprecision(2) << s_cancel_ratio << "\n"
       << "        min_speed_to_force = " << std::fixed << std::setprecision(2) << s_min_speed_to_force << "\n"
       << "        edge_resistance = " << std::fixed << std::setprecision(2) << s_edge_resistance << "\n"
       << "    }\n"
       << "}\n";

    std::ofstream out(conf_path);
    if (out.is_open()) {
        out << ss.str();
        out.flush();
    }

    // Ensure ~/.config/miquland/miquland.conf sources input.conf
    fs::path miquland_conf = miqu_dir / "miquland.conf";
    if (fs::exists(miquland_conf, ec)) {
        std::ifstream min(miquland_conf);
        std::string mline;
        bool has_source = false;
        while (std::getline(min, mline)) {
            std::string t = mline;
            size_t p = t.find_first_not_of(" \t");
            if (p != std::string::npos) t = t.substr(p);
            if (t.find("input.conf") != std::string::npos && t.find("source") != std::string::npos && t.front() != '#') {
                has_source = true;
                break;
            }
        }
        min.close();
        if (!has_source) {
            std::ofstream mappend(miquland_conf, std::ios::app);
            if (mappend.is_open()) {
                mappend << "\n# Sourced modular input configuration:\nsource = ~/.config/miquland/input.conf\n";
            }
        }
    }
}

void load_baseline_config() {
    if (s_baseline_loaded) return;
    s_baseline_loaded = true;

    std::string path = get_input_config_path();
    if (path.empty()) return;

    std::ifstream file(path);
    if (!file.is_open()) return;

    std::string line;
    std::vector<std::string> stack;

    while (std::getline(file, line)) {
        size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line[first] == '#') continue;

        std::string trimmed = line.substr(first);
        size_t comment_pos = trimmed.find('#');
        if (comment_pos != std::string::npos) {
            trimmed = trimmed.substr(0, comment_pos);
        }
        while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t')) trimmed.pop_back();

        size_t brace_open = trimmed.find('{');
        size_t brace_close = trimmed.find('}');

        if (brace_open != std::string::npos) {
            std::string sec_name = trimmed.substr(0, brace_open);
            size_t s = sec_name.find_first_not_of(" \t");
            size_t e = sec_name.find_last_not_of(" \t");
            if (s != std::string::npos && e != std::string::npos) {
                stack.push_back(sec_name.substr(s, e - s + 1));
            }
            continue;
        }

        if (brace_close != std::string::npos) {
            if (!stack.empty()) stack.pop_back();
            continue;
        }

        size_t eq = trimmed.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trimmed.substr(0, eq);
        std::string val = trimmed.substr(eq + 1);

        auto trim_str = [](std::string& s) {
            size_t start = s.find_first_not_of(" \t\r\n");
            size_t end = s.find_last_not_of(" \t\r\n");
            s = (start != std::string::npos && end != std::string::npos) ? s.substr(start, end - start + 1) : "";
        };
        trim_str(key);
        trim_str(val);

        std::string top = stack.empty() ? "" : stack.back();

        if (top == "keyboard") {
            if (key == "layout" && !val.empty()) s_active_layout = val;
            else if (key == "variant") s_kb_variant = val;
            else if (key == "options") s_kb_options = val;
            else if (key == "model") s_kb_model = val;
            else if (key == "repeat_rate") {
                try { s_repeat_rate = std::clamp(std::stoi(val), 1, 100); } catch (...) {}
            } else if (key == "repeat_delay") {
                try { s_repeat_delay = std::clamp(std::stoi(val), 100, 1500); } catch (...) {}
            }
        } else if (top == "touchpad") {
            if (key == "tap_to_click") {
                s_tap_to_click = (val == "true" || val == "1" || val == "yes");
            } else if (key == "natural_scroll") {
                s_natural_scroll = (val == "true" || val == "1" || val == "yes");
            } else if (key == "disable_while_typing") {
                s_disable_while_typing = (val == "true" || val == "1" || val == "yes");
            } else if (key == "accel_profile" && !val.empty()) {
                s_accel_profile = val;
            } else if (key == "accel_speed") {
                try {
                    float raw = std::stof(val); // -1.0 to 1.0
                    s_pointer_speed = std::clamp((raw + 1.0f) * 0.5f, 0.0f, 1.0f);
                } catch (...) {}
            }
        } else if (top == "mouse") {
            if (key == "accel_profile" && !val.empty()) {
                s_mouse_accel_profile = val;
            } else if (key == "accel_speed") {
                try {
                    float raw = std::stof(val);
                    s_mouse_speed = std::clamp((raw + 1.0f) * 0.5f, 0.0f, 1.0f);
                } catch (...) {}
            } else if (key == "natural_scroll") {
                s_mouse_natural_scroll = (val == "true" || val == "1" || val == "yes");
            }
        } else if (top == "touchscreen") {
            if (key == "output") s_touchscreen_output = val;
        } else if (top == "workspace_swipe") {
            if (key == "touchpad_multiplier") {
                try { s_touchpad_swipe_multiplier = std::stof(val); } catch (...) {}
            } else if (key == "touchscreen_multiplier") {
                try { s_touchscreen_swipe_multiplier = std::stof(val); } catch (...) {}
            } else if (key == "swipe_threshold") {
                try { s_swipe_threshold = std::stoi(val); } catch (...) {}
            } else if (key == "touchscreen_threshold") {
                try { s_touchscreen_threshold = std::stoi(val); } catch (...) {}
            } else if (key == "cancel_ratio") {
                try { s_cancel_ratio = std::stof(val); } catch (...) {}
            } else if (key == "min_speed_to_force") {
                try { s_min_speed_to_force = std::stof(val); } catch (...) {}
            } else if (key == "edge_resistance") {
                try { s_edge_resistance = std::stof(val); } catch (...) {}
            }
        }
    }
}

bool is_auxiliary_keyboard(const std::string& name) {
    std::string lower = name;
    for (char& c : lower) c = static_cast<char>(std::tolower(c));
    return (lower.find("power button") != std::string::npos ||
            lower.find("sleep button") != std::string::npos ||
            lower.find("video bus") != std::string::npos ||
            lower.find("intel hid") != std::string::npos ||
            lower.find("dell wmi") != std::string::npos);
}

} // namespace

InputConfigState InputBackend::get_state() {
    std::lock_guard<std::mutex> lock(s_input_mutex);
    load_baseline_config();

    InputConfigState state;
    state.active_layout = s_active_layout;
    state.kb_variant = s_kb_variant;
    state.kb_options = s_kb_options;
    state.kb_model = s_kb_model;
    state.repeat_rate = s_repeat_rate;
    state.repeat_delay = s_repeat_delay;

    state.tap_to_click = s_tap_to_click;
    state.natural_scroll = s_natural_scroll;
    state.disable_while_typing = s_disable_while_typing;
    state.pointer_speed = s_pointer_speed;
    state.accel_profile = s_accel_profile;

    state.mouse_speed = s_mouse_speed;
    state.mouse_accel_profile = s_mouse_accel_profile;
    state.mouse_natural_scroll = s_mouse_natural_scroll;

    state.touchscreen_output = s_touchscreen_output;

    state.touchpad_swipe_multiplier = s_touchpad_swipe_multiplier;
    state.touchscreen_swipe_multiplier = s_touchscreen_swipe_multiplier;
    state.swipe_threshold = s_swipe_threshold;
    state.touchscreen_threshold = s_touchscreen_threshold;
    state.cancel_ratio = s_cancel_ratio;
    state.min_speed_to_force = s_min_speed_to_force;
    state.edge_resistance = s_edge_resistance;

    // Introspect live devices via miquland IPC
    std::string json = AppsBackend::run_cmd("miquctl -j devices 2>/dev/null");
    if (json.empty()) {
        KeyboardDevice def_kb;
        def_kb.name = "Standard PS/2 Keyboard";
        def_kb.layout = "English (US)";
        def_kb.repeat_rate = s_repeat_rate;
        def_kb.repeat_delay = s_repeat_delay;
        def_kb.is_primary = true;
        state.keyboards.push_back(def_kb);

        PointerDevice def_ptr;
        def_ptr.name = "Precision Touchpad";
        def_ptr.is_touchpad = true;
        def_ptr.display_type = "Precision Touchpad";
        state.pointers.push_back(def_ptr);
        return state;
    }

    size_t kb_pos = json.find("\"keyboards\"");
    size_t ptr_pos = json.find("\"pointers\"");
    size_t touch_pos = json.find("\"touch\"");

    // 1. Keyboards
    if (kb_pos != std::string::npos) {
        std::string kb_section = json.substr(kb_pos, (ptr_pos != std::string::npos ? ptr_pos - kb_pos : std::string::npos));
        std::regex obj_regex(R"(\{[^{}]+\})");
        auto begin = std::sregex_iterator(kb_section.begin(), kb_section.end(), obj_regex);
        auto end = std::sregex_iterator();

        for (auto it = begin; it != end; ++it) {
            std::string obj = it->str();
            KeyboardDevice kb;

            std::smatch m;
            std::regex name_re("\"name\"\\s*:\\s*\"([^\"]+)\"");
            if (std::regex_search(obj, m, name_re) && m.size() > 1) kb.name = m[1].str();

            std::regex layout_re("\"layout\"\\s*:\\s*\"([^\"]+)\"");
            if (std::regex_search(obj, m, layout_re) && m.size() > 1) kb.layout = m[1].str();

            std::regex rate_re("\"repeat_rate\"\\s*:\\s*([0-9]+)");
            if (std::regex_search(obj, m, rate_re) && m.size() > 1) {
                try { kb.repeat_rate = std::stoi(m[1].str()); } catch (...) {}
            }

            std::regex delay_re("\"repeat_delay\"\\s*:\\s*([0-9]+)");
            if (std::regex_search(obj, m, delay_re) && m.size() > 1) {
                try { kb.repeat_delay = std::stoi(m[1].str()); } catch (...) {}
            }

            if (!kb.name.empty()) {
                kb.is_primary = !is_auxiliary_keyboard(kb.name);
                state.keyboards.push_back(kb);
            }
        }
    }

    // Ensure at least one primary keyboard is marked
    bool has_primary = false;
    for (const auto& kb : state.keyboards) {
        if (kb.is_primary) { has_primary = true; break; }
    }
    if (!has_primary && !state.keyboards.empty()) {
        state.keyboards[0].is_primary = true;
    }

    // 2. Pointers
    if (ptr_pos != std::string::npos) {
        std::string ptr_section = json.substr(ptr_pos, (touch_pos != std::string::npos ? touch_pos - ptr_pos : std::string::npos));
        std::regex obj_regex(R"(\{[^{}]+\})");
        auto begin = std::sregex_iterator(ptr_section.begin(), ptr_section.end(), obj_regex);
        auto end = std::sregex_iterator();

        for (auto it = begin; it != end; ++it) {
            std::string obj = it->str();
            PointerDevice ptr;
            std::smatch m;
            std::regex name_re("\"name\"\\s*:\\s*\"([^\"]+)\"");
            if (std::regex_search(obj, m, name_re) && m.size() > 1) ptr.name = m[1].str();

            if (!ptr.name.empty()) {
                std::string lower = ptr.name;
                for (char& c : lower) c = static_cast<char>(std::tolower(c));

                if (lower.find("touchpad") != std::string::npos || lower.find("044e:120b") != std::string::npos || lower.find("synaptics") != std::string::npos) {
                    ptr.is_touchpad = true;
                    ptr.display_type = "Precision Touchpad";
                } else if (lower.find("stick") != std::string::npos || lower.find("trackpoint") != std::string::npos || lower.find("dualpoint") != std::string::npos) {
                    ptr.is_trackpoint = true;
                    ptr.display_type = "Pointing Stick (TrackPoint)";
                } else {
                    ptr.display_type = "Mouse / Optical Pointer";
                }
                state.pointers.push_back(ptr);
            }
        }
    }

    // 3. Touch
    if (touch_pos != std::string::npos) {
        std::string touch_section = json.substr(touch_pos);
        std::regex obj_regex(R"(\{[^{}]+\})");
        auto begin = std::sregex_iterator(touch_section.begin(), touch_section.end(), obj_regex);
        auto end = std::sregex_iterator();

        for (auto it = begin; it != end; ++it) {
            std::string obj = it->str();
            TouchDevice tch;
            std::smatch m;
            std::regex name_re("\"name\"\\s*:\\s*\"([^\"]+)\"");
            if (std::regex_search(obj, m, name_re) && m.size() > 1) tch.name = m[1].str();

            if (!tch.name.empty()) {
                tch.mapped_output = s_touchscreen_output.empty() ? "eDP-1" : s_touchscreen_output;
                state.touch_devices.push_back(tch);
            }
        }
    }

    return state;
}

void InputBackend::set_keyboard_layout(const std::string& layout) {
    std::lock_guard<std::mutex> lock(s_input_mutex);
    load_baseline_config();
    s_active_layout = layout;
    save_config_locked();
}

void InputBackend::set_repeat_info(int rate, int delay) {
    std::lock_guard<std::mutex> lock(s_input_mutex);
    load_baseline_config();
    s_repeat_rate = std::clamp(rate, 1, 100);
    s_repeat_delay = std::clamp(delay, 100, 1500);
    save_config_locked();
}

void InputBackend::set_touchpad_options(bool tap_to_click, bool natural_scroll, bool disable_while_typing) {
    std::lock_guard<std::mutex> lock(s_input_mutex);
    load_baseline_config();
    s_tap_to_click = tap_to_click;
    s_natural_scroll = natural_scroll;
    s_disable_while_typing = disable_while_typing;
    save_config_locked();
}

void InputBackend::set_pointer_speed(float speed) {
    std::lock_guard<std::mutex> lock(s_input_mutex);
    load_baseline_config();
    s_pointer_speed = std::clamp(speed, 0.0f, 1.0f);
    save_config_locked();
}

void InputBackend::set_accel_profile(const std::string& profile) {
    std::lock_guard<std::mutex> lock(s_input_mutex);
    load_baseline_config();
    s_accel_profile = profile;
    save_config_locked();
}

void InputBackend::set_mouse_options(float speed, const std::string& profile, bool natural_scroll) {
    std::lock_guard<std::mutex> lock(s_input_mutex);
    load_baseline_config();
    s_mouse_speed = std::clamp(speed, 0.0f, 1.0f);
    s_mouse_accel_profile = profile;
    s_mouse_natural_scroll = natural_scroll;
    save_config_locked();
}

void InputBackend::set_touchscreen_output(const std::string& output) {
    std::lock_guard<std::mutex> lock(s_input_mutex);
    load_baseline_config();
    s_touchscreen_output = output;
    save_config_locked();
}

void InputBackend::set_swipe_physics(float tp_mult, float ts_mult, int threshold, int ts_threshold, float cancel, float min_spd, float edge_res) {
    std::lock_guard<std::mutex> lock(s_input_mutex);
    load_baseline_config();
    s_touchpad_swipe_multiplier = std::clamp(tp_mult, 0.1f, 5.0f);
    s_touchscreen_swipe_multiplier = std::clamp(ts_mult, 0.1f, 5.0f);
    s_swipe_threshold = std::clamp(threshold, 5, 500);
    s_touchscreen_threshold = std::clamp(ts_threshold, 5, 500);
    s_cancel_ratio = std::clamp(cancel, 0.05f, 0.95f);
    s_min_speed_to_force = std::clamp(min_spd, 0.05f, 5.0f);
    s_edge_resistance = std::clamp(edge_res, 0.0f, 0.5f);
    save_config_locked();
}

void InputBackend::save_config(const InputConfigState& state) {
    std::lock_guard<std::mutex> lock(s_input_mutex);
    s_active_layout = state.active_layout;
    s_kb_variant = state.kb_variant;
    s_kb_options = state.kb_options;
    s_kb_model = state.kb_model;
    s_repeat_rate = state.repeat_rate;
    s_repeat_delay = state.repeat_delay;

    s_tap_to_click = state.tap_to_click;
    s_natural_scroll = state.natural_scroll;
    s_disable_while_typing = state.disable_while_typing;
    s_pointer_speed = state.pointer_speed;
    s_accel_profile = state.accel_profile;

    s_mouse_speed = state.mouse_speed;
    s_mouse_accel_profile = state.mouse_accel_profile;
    s_mouse_natural_scroll = state.mouse_natural_scroll;

    s_touchscreen_output = state.touchscreen_output;

    s_touchpad_swipe_multiplier = state.touchpad_swipe_multiplier;
    s_touchscreen_swipe_multiplier = state.touchscreen_swipe_multiplier;
    s_swipe_threshold = state.swipe_threshold;
    s_touchscreen_threshold = state.touchscreen_threshold;
    s_cancel_ratio = state.cancel_ratio;
    s_min_speed_to_force = state.min_speed_to_force;
    s_edge_resistance = state.edge_resistance;

    save_config_locked();
}

} // namespace miqu
