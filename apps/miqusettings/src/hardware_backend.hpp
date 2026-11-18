#pragma once

#include <string>
#include <vector>
#include <memory>
#include "settings_model.hpp"

namespace miqu {

struct HardwareSection {
    std::string title;
    std::string badge;
    std::vector<TelemetryEntry> entries;
};

class HardwareBackend {
public:
    static std::vector<HardwareSection> get_spec_by_id(const std::string& id);
    static std::string get_spec_description(const std::string& id);

    static std::vector<HardwareSection> get_system_specs();
    static std::vector<HardwareSection> get_service_specs();
    static std::vector<HardwareSection> get_display_specs();
    static std::vector<HardwareSection> get_audio_specs();
    static std::vector<HardwareSection> get_network_specs();
    static std::vector<HardwareSection> get_storage_specs();
    static std::vector<HardwareSection> get_power_specs();
    static std::vector<HardwareSection> get_input_specs();

    // Specific granular topics
    static std::vector<HardwareSection> get_sys_board();
    static std::vector<HardwareSection> get_sys_firmware();
    static std::vector<HardwareSection> get_sys_cpu();
    static std::vector<HardwareSection> get_sys_thermals();
    static std::vector<HardwareSection> get_sys_memory();
    static std::vector<HardwareSection> get_sys_uptime();

    static std::vector<HardwareSection> get_srv_os();
    static std::vector<HardwareSection> get_srv_desktop();
    static std::vector<HardwareSection> get_srv_network();
    static std::vector<HardwareSection> get_srv_security();
    static std::vector<HardwareSection> get_srv_sound();
    static std::vector<HardwareSection> get_srv_power();
    static std::vector<HardwareSection> get_srv_packages();

    static std::vector<HardwareSection> get_disp_gpu();
    static std::vector<HardwareSection> get_disp_monitors();
    static std::vector<HardwareSection> get_disp_backlight();

    static std::vector<HardwareSection> get_aud_cards();
    static std::vector<HardwareSection> get_aud_endpoints();

    static std::vector<HardwareSection> get_net_wifi();
    static std::vector<HardwareSection> get_net_ethernet();
    static std::vector<HardwareSection> get_net_bluetooth();
    static std::vector<HardwareSection> get_net_routing();

    static std::vector<HardwareSection> get_stor_drives();
    static std::vector<HardwareSection> get_stor_mounts();
    static std::vector<HardwareSection> get_stor_zram();

    static std::vector<HardwareSection> get_pwr_battery();
    static std::vector<HardwareSection> get_pwr_adapter();
    static std::vector<HardwareSection> get_pwr_profile();

    static std::vector<HardwareSection> get_inp_keyboards();
    static std::vector<HardwareSection> get_inp_pointers();
    static std::vector<HardwareSection> get_inp_touch();
    static std::vector<HardwareSection> get_inp_switches();

    // Utility helpers for querying system and sysfs
    static std::string read_file_trimmed(const std::string& path);
    static std::string run_cmd(const std::string& cmd);
    static std::string format_bytes(unsigned long long bytes);
};

} // namespace miqu
