#pragma once

#include "settings_model.hpp"
#include <string>
#include <vector>
#include <memory>

namespace miqu {

class PluginBackend {
public:
    static std::vector<PluginItemInfo> get_available_plugins();
    static std::vector<std::shared_ptr<SettingItem>> get_plugin_items();
    static bool set_plugin_active(const std::string& plugin_path, bool active);
    static void ensure_user_config_on_tinker();
};

} // namespace miqu
