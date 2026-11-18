#include "hardware_backend.hpp"
#include "sound_backend.hpp"
#include "battery_backend.hpp"
#include "monitor_backend.hpp"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <cstring>
#include <cmath>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>

namespace fs = std::filesystem;

namespace miqu {

std::string HardwareBackend::read_file_trimmed(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::string line;
    if (std::getline(file, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }
        size_t first = line.find_first_not_of(" \t");
        return (first != std::string::npos) ? line.substr(first) : "";
    }
    return "";
}

std::string HardwareBackend::run_cmd(const std::string& cmd) {
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return result;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
    while (!result.empty() && (result.back() == '\r' || result.back() == '\n')) {
        result.pop_back();
    }
    return result;
}

std::string HardwareBackend::format_bytes(unsigned long long bytes) {
    const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    double b = static_cast<double>(bytes);
    int idx = 0;
    while (b >= 1024.0 && idx < 4) {
        b /= 1024.0;
        idx++;
    }
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << b << " " << units[idx];
    return ss.str();
}

static std::string query_service_state(const std::string& service, bool user = false) {
    std::string cmd = user ? ("systemctl --user is-active " + service + " 2>/dev/null")
                           : ("systemctl is-active " + service + " 2>/dev/null");
    std::string out = HardwareBackend::run_cmd(cmd);
    if (out.find("active") != std::string::npos) return "Active (Running)";
    if (out.find("inactive") != std::string::npos) return "Inactive (Stopped)";
    if (out.find("failed") != std::string::npos) return "Failed";
    return "Not Loaded / Disabled";
}

static std::string query_process_state(const std::string& process) {
    std::string cmd = "pgrep -x " + process + " 2>/dev/null";
    std::string pid = HardwareBackend::run_cmd(cmd);
    if (!pid.empty()) {
        std::istringstream iss(pid);
        std::string p;
        if (iss >> p) {
            return "Running (PID " + p + ")";
        }
        return "Running";
    }
    return "Stopped / Inactive";
}

static std::string format_clock(long long khz) {
    if (khz <= 0) return "";
    double ghz = static_cast<double>(khz) / 1000000.0;
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << ghz << " GHz";
    return ss.str();
}

static void append_sections(std::vector<HardwareSection>& dest, std::vector<HardwareSection> src) {
    dest.insert(dest.end(), std::make_move_iterator(src.begin()), std::make_move_iterator(src.end()));
}

// ==============================================================================
// 1. Device Specifications (Subcategory Topics)
// ==============================================================================

std::vector<HardwareSection> HardwareBackend::get_sys_board() {
    std::vector<HardwareSection> sections;
    HardwareSection s;
    s.title = "Computer & Motherboard";

    std::string vendor = read_file_trimmed("/sys/class/dmi/id/sys_vendor");
    std::string product = read_file_trimmed("/sys/class/dmi/id/product_name");
    std::string version = read_file_trimmed("/sys/class/dmi/id/product_version");
    std::string chassis = read_file_trimmed("/sys/class/dmi/id/chassis_type");
    std::string board_name = read_file_trimmed("/sys/class/dmi/id/board_name");

    char hostname[256] = {0};
    gethostname(hostname, sizeof(hostname) - 1);

    if (!product.empty()) {
        s.entries.push_back({"Device Model", (!vendor.empty() ? vendor + " " : "") + product});
    }
    if (!version.empty()) {
        s.entries.push_back({"Version / SKU", version});
    }
    if (!board_name.empty()) {
        s.entries.push_back({"Mainboard", board_name});
    }
    if (hostname[0] != '\0') {
        s.entries.push_back({"Host Name", hostname});
    }

    if (!chassis.empty()) {
        int c = 0;
        try { c = std::stoi(chassis); } catch (...) {}
        std::string c_str = "Desktop / Workstation";
        if (c == 8 || c == 9 || c == 10 || c == 14) c_str = "Laptop / Portable";
        else if (c == 11) c_str = "Handheld";
        else if (c == 30 || c == 31 || c == 32) c_str = "Convertible / Tablet";
        s.entries.push_back({"Form Factor", c_str});
        s.badge = c_str;
    }

    sections.push_back(std::move(s));
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_sys_firmware() {
    std::vector<HardwareSection> sections;
    HardwareSection s;
    s.title = "BIOS & Firmware Architecture";

    std::string bios_vendor = read_file_trimmed("/sys/class/dmi/id/bios_vendor");
    std::string bios_ver = read_file_trimmed("/sys/class/dmi/id/bios_version");
    std::string bios_date = read_file_trimmed("/sys/class/dmi/id/bios_date");

    if (!bios_vendor.empty()) {
        s.entries.push_back({"Firmware Vendor", bios_vendor});
    }
    if (!bios_ver.empty()) {
        std::string b_str = bios_ver;
        if (!bios_date.empty()) b_str += " (" + bios_date + ")";
        s.entries.push_back({"BIOS Version", b_str});
    }

    bool efi_present = fs::exists("/sys/firmware/efi");
    s.entries.push_back({"Boot Mode", efi_present ? "UEFI" : "Legacy BIOS"});

    if (efi_present) {
        bool secure_boot = false;
        std::error_code ec;
        if (fs::exists("/sys/firmware/efi/efivars", ec)) {
            for (const auto& entry : fs::directory_iterator("/sys/firmware/efi/efivars", ec)) {
                if (entry.path().filename().string().rfind("SecureBoot-", 0) == 0) {
                    std::ifstream sb_file(entry.path(), std::ios::binary);
                    if (sb_file.is_open()) {
                        char buf[5] = {0};
                        if (sb_file.read(buf, 5)) {
                            secure_boot = (buf[4] == 1);
                        }
                    }
                    break;
                }
            }
        }
        s.entries.push_back({"Secure Boot", secure_boot ? "Enabled" : "Disabled"});
        s.badge = secure_boot ? "SecureBoot Enabled" : "UEFI Mode";
    }

    sections.push_back(std::move(s));
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_sys_cpu() {
    std::vector<HardwareSection> sections;
    HardwareSection s;
    s.title = "Central Processor (CPU)";

    std::string cpu_model;
    int logical_cores = 0;
    int physical_cores = 0;
    std::string cpu_mhz;

    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.rfind("model name", 0) == 0 && cpu_model.empty()) {
                size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    cpu_model = line.substr(colon + 2);
                }
            } else if (line.rfind("processor", 0) == 0) {
                logical_cores++;
            } else if (line.rfind("cpu cores", 0) == 0 && physical_cores == 0) {
                size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    try { physical_cores = std::stoi(line.substr(colon + 2)); } catch (...) {}
                }
            } else if (line.rfind("cpu MHz", 0) == 0 && cpu_mhz.empty()) {
                size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    cpu_mhz = line.substr(colon + 2);
                }
            }
        }
    }

    struct utsname u;
    if (uname(&u) == 0) {
        s.entries.push_back({"Architecture", u.machine});
    }

    if (!cpu_model.empty()) {
        s.entries.push_back({"Processor", cpu_model});
    }

    if (logical_cores > 0) {
        std::string cores_str = std::to_string(logical_cores) + " Threads";
        if (physical_cores > 0) {
            cores_str = std::to_string(physical_cores) + " Cores, " + cores_str;
        }
        s.entries.push_back({"Cores & Threads", cores_str});
        s.badge = std::to_string(logical_cores) + " Threads";
    }

    std::string cur_freq = read_file_trimmed("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
    std::string max_freq = read_file_trimmed("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
    if (!cur_freq.empty()) {
        try {
            long long cur_k = std::stoll(cur_freq);
            std::string freq_str = format_clock(cur_k);
            if (!max_freq.empty()) {
                long long max_k = std::stoll(max_freq);
                freq_str += " (Max: " + format_clock(max_k) + ")";
            }
            s.entries.push_back({"Clock Frequency", freq_str});
        } catch (...) {}
    } else if (!cpu_mhz.empty()) {
        s.entries.push_back({"Clock Frequency", cpu_mhz + " MHz"});
    }

    std::string gov = read_file_trimmed("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");
    if (!gov.empty()) {
        s.entries.push_back({"Scaling Governor", gov});
    }

    std::string l1d = read_file_trimmed("/sys/devices/system/cpu/cpu0/cache/index0/size");
    std::string l1i = read_file_trimmed("/sys/devices/system/cpu/cpu0/cache/index1/size");
    std::string l2 = read_file_trimmed("/sys/devices/system/cpu/cpu0/cache/index2/size");
    std::string l3 = read_file_trimmed("/sys/devices/system/cpu/cpu0/cache/index3/size");
    std::string cache_str;
    if (!l1d.empty() || !l1i.empty()) {
        cache_str += "L1: " + (!l1d.empty() ? l1d + " (Data)" : "") + (!l1i.empty() ? " / " + l1i + " (Inst)" : "");
    }
    if (!l2.empty()) {
        if (!cache_str.empty()) cache_str += ", ";
        cache_str += "L2: " + l2;
    }
    if (!l3.empty()) {
        if (!cache_str.empty()) cache_str += ", ";
        cache_str += "L3: " + l3;
    }
    if (!cache_str.empty()) {
        s.entries.push_back({"Cache Hierarchy", cache_str});
    }

    sections.push_back(std::move(s));
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_sys_thermals() {
    std::vector<HardwareSection> sections;
    HardwareSection s;
    s.title = "Hardware Thermals & Cooling";

    std::string cpu_pkg_temp;
    std::vector<std::string> core_temps;
    std::string pch_temp;
    std::string wifi_temp;
    std::string fan_rpm;

    std::error_code ec;
    if (fs::exists("/sys/class/hwmon", ec)) {
        for (const auto& entry : fs::directory_iterator("/sys/class/hwmon", ec)) {
            std::string name = read_file_trimmed(entry.path().string() + "/name");

            if (name == "coretemp") {
                for (int i = 1; i <= 8; ++i) {
                    std::string t_path = entry.path().string() + "/temp" + std::to_string(i) + "_input";
                    std::string l_path = entry.path().string() + "/temp" + std::to_string(i) + "_label";
                    std::string t_val = read_file_trimmed(t_path);
                    if (!t_val.empty()) {
                        try {
                            int millic = std::stoi(t_val);
                            int c = millic / 1000;
                            std::string label = read_file_trimmed(l_path);
                            if (label.empty()) label = "Core " + std::to_string(i - 1);
                            if (label.find("Package") != std::string::npos && cpu_pkg_temp.empty()) {
                                cpu_pkg_temp = std::to_string(c) + " °C";
                            } else {
                                core_temps.push_back(label + ": " + std::to_string(c) + " °C");
                            }
                        } catch (...) {}
                    }
                }
            } else if (name.rfind("pch", 0) == 0 && pch_temp.empty()) {
                std::string t_val = read_file_trimmed(entry.path().string() + "/temp1_input");
                if (!t_val.empty()) {
                    try {
                        float c = std::stof(t_val) / 1000.0f;
                        std::ostringstream ss;
                        ss << std::fixed << std::setprecision(1) << c << " °C";
                        pch_temp = ss.str();
                    } catch (...) {}
                }
            } else if (name.rfind("iwlwifi", 0) == 0 && wifi_temp.empty()) {
                std::string t_val = read_file_trimmed(entry.path().string() + "/temp1_input");
                if (!t_val.empty()) {
                    try {
                        float c = std::stof(t_val) / 1000.0f;
                        std::ostringstream ss;
                        ss << std::fixed << std::setprecision(1) << c << " °C";
                        wifi_temp = ss.str();
                    } catch (...) {}
                }
            }

            if (fan_rpm.empty()) {
                for (int i = 1; i <= 4; ++i) {
                    std::string f_path = entry.path().string() + "/fan" + std::to_string(i) + "_input";
                    std::string f_val = read_file_trimmed(f_path);
                    if (!f_val.empty()) {
                        fan_rpm = f_val + " RPM";
                        break;
                    }
                }
            }
        }
    }

    if (cpu_pkg_temp.empty()) {
        std::string tz0 = read_file_trimmed("/sys/class/thermal/thermal_zone0/temp");
        if (!tz0.empty()) {
            try {
                int c = std::stoi(tz0) / 1000;
                cpu_pkg_temp = std::to_string(c) + " °C (ACPI)";
            } catch (...) {}
        }
    }

    if (!cpu_pkg_temp.empty()) {
        s.entries.push_back({"CPU Package Temp", cpu_pkg_temp});
        s.badge = cpu_pkg_temp;
    }
    for (const auto& ct : core_temps) {
        s.entries.push_back({"Core Thermals", ct});
    }
    if (!pch_temp.empty()) {
        s.entries.push_back({"Platform Hub (PCH)", pch_temp});
    }
    if (!wifi_temp.empty()) {
        s.entries.push_back({"Wireless Adapter Temp", wifi_temp});
    }
    if (!fan_rpm.empty()) {
        s.entries.push_back({"Cooling Fan Speed", fan_rpm});
    } else {
        s.entries.push_back({"Cooling Fan Speed", "Passive / Silent (0 RPM)"});
    }

    sections.push_back(std::move(s));
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_sys_memory() {
    std::vector<HardwareSection> sections;
    HardwareSection s;
    s.title = "System Memory (RAM)";

    unsigned long long mem_total = 0;
    unsigned long long mem_available = 0;
    unsigned long long mem_buffers = 0;
    unsigned long long mem_cached = 0;
    unsigned long long swap_total = 0;
    unsigned long long swap_free = 0;

    std::ifstream meminfo("/proc/meminfo");
    if (meminfo.is_open()) {
        std::string line;
        while (std::getline(meminfo, line)) {
            std::istringstream iss(line);
            std::string key;
            unsigned long long val = 0;
            std::string unit;
            if (iss >> key >> val >> unit) {
                if (key == "MemTotal:") mem_total = val * 1024;
                else if (key == "MemAvailable:") mem_available = val * 1024;
                else if (key == "Buffers:") mem_buffers = val * 1024;
                else if (key == "Cached:") mem_cached = val * 1024;
                else if (key == "SwapTotal:") swap_total = val * 1024;
                else if (key == "SwapFree:") swap_free = val * 1024;
            }
        }
    }

    if (mem_total > 0) {
        s.entries.push_back({"Total Memory", format_bytes(mem_total)});
        if (mem_available > 0) {
            unsigned long long mem_used = mem_total - mem_available;
            int pct = static_cast<int>(std::round((static_cast<double>(mem_used) / mem_total) * 100.0));
            s.entries.push_back({"Memory Used", format_bytes(mem_used) + " (" + std::to_string(pct) + "%)"});
            s.entries.push_back({"Available Memory", format_bytes(mem_available)});
            s.badge = format_bytes(mem_total);
        }
        if (mem_buffers > 0) {
            s.entries.push_back({"Kernel Buffers", format_bytes(mem_buffers)});
        }
        if (mem_cached > 0) {
            s.entries.push_back({"Page Cache", format_bytes(mem_cached)});
        }
    }

    if (swap_total > 0) {
        unsigned long long swap_used = swap_total - swap_free;
        s.entries.push_back({"Swap Storage", format_bytes(swap_total) + " (" + format_bytes(swap_used) + " used)"});

        bool has_zram = false;
        std::ifstream swaps_f("/proc/swaps");
        if (swaps_f.is_open()) {
            std::string sw_line;
            while (std::getline(swaps_f, sw_line)) {
                if (sw_line.find("zram") != std::string::npos) {
                    has_zram = true;
                    break;
                }
            }
        }
        if (has_zram) {
            s.entries.push_back({"Swap Architecture", "ZRAM In-Memory (Compressed RAM Pool)"});
        }
    }

    sections.push_back(std::move(s));
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_sys_uptime() {
    std::vector<HardwareSection> sections;
    HardwareSection s;
    s.title = "System Activity & Uptime";

    std::ifstream uptime_f("/proc/uptime");
    if (uptime_f.is_open()) {
        double up_sec = 0;
        if (uptime_f >> up_sec) {
            int total_min = static_cast<int>(up_sec / 60.0);
            int hours = total_min / 60;
            int mins = total_min % 60;
            std::string up_str;
            if (hours > 0) {
                up_str = std::to_string(hours) + " hour" + (hours > 1 ? "s " : " ") + std::to_string(mins) + " minute" + (mins != 1 ? "s" : "");
            } else {
                up_str = std::to_string(mins) + " minute" + (mins != 1 ? "s" : "");
            }
            s.entries.push_back({"System Uptime", up_str});
            s.badge = up_str;
        }
    }

    std::ifstream load_file("/proc/loadavg");
    if (load_file.is_open()) {
        double l1 = 0, l5 = 0, l15 = 0;
        std::string proc_tasks;
        if (load_file >> l1 >> l5 >> l15 >> proc_tasks) {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(2) << l1 << " (1m), " << l5 << " (5m), " << l15 << " (15m)";
            s.entries.push_back({"Load Averages", ss.str()});

            auto slash = proc_tasks.find('/');
            if (slash != std::string::npos) {
                s.entries.push_back({"Active Tasks", proc_tasks.substr(0, slash) + " running / " + proc_tasks.substr(slash + 1) + " total"});
            }
        }
    }

    sections.push_back(std::move(s));
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_system_specs() {
    std::vector<HardwareSection> all;
    append_sections(all, get_sys_board());
    append_sections(all, get_sys_firmware());
    append_sections(all, get_sys_cpu());
    append_sections(all, get_sys_thermals());
    append_sections(all, get_sys_memory());
    append_sections(all, get_sys_uptime());
    return all;
}

// ==============================================================================
// 2. Operating System & Services (Subcategory Topics)
// ==============================================================================

std::vector<HardwareSection> HardwareBackend::get_srv_os() {
    std::vector<HardwareSection> sections;
    HardwareSection s;
    s.title = "Operating System & Environment";

    std::string os_name;
    std::ifstream os_rel("/etc/os-release");
    if (os_rel.is_open()) {
        std::string line;
        while (std::getline(os_rel, line)) {
            if (line.rfind("PRETTY_NAME=", 0) == 0) {
                os_name = line.substr(12);
                if (!os_name.empty() && os_name.front() == '"') os_name.erase(0, 1);
                if (!os_name.empty() && os_name.back() == '"') os_name.pop_back();
                break;
            }
        }
    }
    if (os_name.empty()) os_name = "Linux";

    struct utsname u;
    std::string k_ver = "Unknown";
    std::string arch = "x86_64";
    if (uname(&u) == 0) {
        k_ver = std::string(u.release);
        arch = u.machine;
    }

    s.badge = os_name;
    s.entries.push_back({"Distribution", os_name});
    s.entries.push_back({"Linux Kernel", k_ver + " (" + arch + ")"});
    s.entries.push_back({"Desktop Environment", "Miquland Desktop (Wayland Native)"});
    s.entries.push_back({"Wayland Compositor", "miquland (wlroots based)"});
    s.entries.push_back({"Session Protocol", "Wayland 1.23"});

    const char* sh = getenv("SHELL");
    s.entries.push_back({"Default User Shell", sh ? sh : "/bin/bash"});

    sections.push_back(std::move(s));
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_srv_desktop() {
    std::vector<HardwareSection> sections;
    HardwareSection s;
    s.title = "Desktop Environment & Shell (Miqu Core)";
    std::string land_st = query_process_state("miquland");
    s.badge = (land_st.find("Running") != std::string::npos) ? "miquland Active" : "Active";

    s.entries.push_back({"Wayland Compositor", "miquland (" + land_st + ")"});
    s.entries.push_back({"Desktop Workspace Shell", "miqudesk (" + query_process_state("miqudesk") + ")"});
    s.entries.push_back({"System Status Bar", "miqubar (" + query_process_state("miqubar") + ")"});
    s.entries.push_back({"Notification Daemon", "miqunotify (" + query_process_state("miqunotify") + ")"});
    s.entries.push_back({"Session Idle Manager", "miquidle (" + query_process_state("miquidle") + ")"});
    s.entries.push_back({"Authentication Agent", "miqupolkit (" + query_process_state("miqupolkit") + ")"});
    s.entries.push_back({"Display Manager / Greeter", "miqudm (" + query_process_state("miqudm") + ")"});
    s.entries.push_back({"Wallpaper Engine", "miqubg (" + query_process_state("miqubg") + ")"});

    sections.push_back(std::move(s));
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_srv_network() {
    std::vector<HardwareSection> sections;
    HardwareSection s;
    s.title = "Network & Connectivity Backends";
    std::string nm_st = query_service_state("NetworkManager");
    s.badge = (nm_st.find("Active") != std::string::npos) ? "Managed" : "Configured";

    s.entries.push_back({"Network Service", "NetworkManager (" + nm_st + ")"});
    s.entries.push_back({"Wireless Supplicant", "wpa_supplicant (" + query_service_state("wpa_supplicant") + ")"});
    s.entries.push_back({"Bluetooth Daemon", "BlueZ / bluetoothd (" + query_service_state("bluetooth") + ")"});
    s.entries.push_back({"DNS Resolver", "systemd-resolved (" + query_service_state("systemd-resolved") + ")"});

    sections.push_back(std::move(s));
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_srv_security() {
    std::vector<HardwareSection> sections;
    HardwareSection s;
    s.title = "Security & Firewall Backends";
    std::string ufw_st = query_service_state("ufw");
    std::string snitch_st = query_service_state("opensnitchd");
    if (snitch_st.find("Not") != std::string::npos) snitch_st = query_service_state("opensnitch");

    s.badge = (ufw_st.find("Active") != std::string::npos) ? "Firewall Active" : "Shield Available";

    s.entries.push_back({"Packet Firewall (UFW)", "ufw (" + ufw_st + ")"});
    s.entries.push_back({"Application Monitor", "OpenSnitch (" + snitch_st + ")"});
    s.entries.push_back({"Kernel Packet Engine", "Linux Netfilter (nftables / iptables)"});
    s.entries.push_back({"Privilege Authority", "Polkit (org.freedesktop.PolicyKit1)"});
    s.entries.push_back({"Session & Seat Manager", "systemd-logind (" + query_service_state("systemd-logind") + ")"});

    sections.push_back(std::move(s));
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_srv_sound() {
    std::vector<HardwareSection> sections;
    HardwareSection s;
    s.title = "Sound & Media Subsystems";
    std::string pw_st = query_service_state("pipewire", true);
    s.badge = (pw_st.find("Active") != std::string::npos) ? "PipeWire Active" : "ALSA";

    s.entries.push_back({"Audio Server", "PipeWire (" + pw_st + ")"});
    s.entries.push_back({"Session Policy Manager", "WirePlumber (" + query_service_state("wireplumber", true) + ")"});
    s.entries.push_back({"Desktop Portals", "xdg-desktop-portal (" + query_service_state("xdg-desktop-portal", true) + ")"});
    s.entries.push_back({"Hardware Interface", "ALSA (Advanced Linux Sound Architecture)"});

    sections.push_back(std::move(s));
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_srv_power() {
    std::vector<HardwareSection> sections;
    HardwareSection s;
    s.title = "Power & Platform Management";
    std::string ppd_st = query_service_state("power-profiles-daemon");
    s.badge = (ppd_st.find("Active") != std::string::npos) ? "Daemon Active" : "ACPI Default";

    s.entries.push_back({"Performance Profiles", "power-profiles-daemon (" + ppd_st + ")"});
    s.entries.push_back({"Power Device Abstraction", "UPower (" + query_service_state("upower") + ")"});
    s.entries.push_back({"Thermal / CPU Governor", "sysfs cpufreq & ACPI platform_profile"});

    sections.push_back(std::move(s));
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_srv_packages() {
    std::vector<HardwareSection> sections;
    HardwareSection s;
    s.title = "Package Ecosystem & Application Runtime";

    std::error_code ec;
    int pacman_count = 0;
    if (fs::exists("/var/lib/pacman/local", ec)) {
        for (const auto& entry : fs::directory_iterator("/var/lib/pacman/local", ec)) {
            if (entry.is_directory(ec)) pacman_count++;
        }
    }

    int flatpak_count = 0;
    if (fs::exists("/var/lib/flatpak/app", ec)) {
        for (const auto& entry : fs::directory_iterator("/var/lib/flatpak/app", ec)) {
            if (entry.is_directory(ec)) flatpak_count++;
        }
    }
    const char* home = getenv("HOME");
    if (home) {
        std::string user_flatpak = std::string(home) + "/.local/share/flatpak/app";
        if (fs::exists(user_flatpak, ec)) {
            for (const auto& entry : fs::directory_iterator(user_flatpak, ec)) {
                if (entry.is_directory(ec)) flatpak_count++;
            }
        }
    }

    if (pacman_count > 0) {
        s.entries.push_back({"Native Packages", std::to_string(pacman_count) + " installed (pacman/alpm)"});
        s.badge = std::to_string(pacman_count) + " packages";
    }
    if (flatpak_count > 0) {
        s.entries.push_back({"Flatpak Runtimes", std::to_string(flatpak_count) + " application bundles"});
    } else {
        s.entries.push_back({"Flatpak Runtimes", "0 bundles installed"});
    }
    s.entries.push_back({"Binary Format", "ELF 64-bit LSB (x86-64-v3)"});
    s.entries.push_back({"Desktop Framework", "Miqutoolkit (Cairo / Wayland Native)"});

    sections.push_back(std::move(s));
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_service_specs() {
    std::vector<HardwareSection> all;
    append_sections(all, get_srv_os());
    append_sections(all, get_srv_desktop());
    append_sections(all, get_srv_network());
    append_sections(all, get_srv_security());
    append_sections(all, get_srv_sound());
    append_sections(all, get_srv_power());
    append_sections(all, get_srv_packages());
    return all;
}

// ==============================================================================
// 3. Displays & Graphics (Subcategory Topics)
// ==============================================================================

std::vector<HardwareSection> HardwareBackend::get_disp_gpu() {
    std::vector<HardwareSection> sections;
    HardwareSection s;
    s.title = "Graphics Processor (GPU)";

    std::string lspci_out = run_cmd("lspci 2>/dev/null | grep -E 'VGA|3D|Display'");
    if (!lspci_out.empty()) {
        std::istringstream stream(lspci_out);
        std::string line;
        int gpu_num = 1;
        while (std::getline(stream, line)) {
            size_t colon = line.find(": ");
            if (colon != std::string::npos) {
                std::string label = (gpu_num == 1) ? "Primary GPU" : ("Secondary GPU " + std::to_string(gpu_num));
                s.entries.push_back({label, line.substr(colon + 2)});
                gpu_num++;
            }
        }
    }

    std::error_code ec;
    if (fs::exists("/sys/class/drm", ec)) {
        for (const auto& entry : fs::directory_iterator("/sys/class/drm", ec)) {
            std::string name = entry.path().filename().string();
            if (name.rfind("card", 0) == 0 && name.find('-') == std::string::npos) {
                std::string driver_sym = entry.path().string() + "/device/driver";
                if (fs::is_symlink(driver_sym, ec)) {
                    std::string drv = fs::read_symlink(driver_sym, ec).filename().string();
                    s.entries.push_back({"Kernel Driver", drv + " (" + name + ")"});
                    s.badge = drv;
                    break;
                }
            }
        }
    }

    std::string mesa_ver;
    if (fs::exists("/var/lib/pacman/local", ec)) {
        for (const auto& entry : fs::directory_iterator("/var/lib/pacman/local", ec)) {
            std::string fname = entry.path().filename().string();
            if (fname.rfind("mesa-", 0) == 0 && fname.size() > 5 && std::isdigit(fname[5])) {
                mesa_ver = fname.substr(5);
                break;
            }
        }
    }
    if (mesa_ver.empty()) {
        std::string gl_out = run_cmd("glxinfo -B 2>/dev/null | grep 'OpenGL version string'");
        if (!gl_out.empty()) {
            auto c = gl_out.find(':');
            if (c != std::string::npos) mesa_ver = gl_out.substr(c + 2);
        }
    }
    if (!mesa_ver.empty()) {
        s.entries.push_back({"Mesa 3D Driver", "Mesa " + mesa_ver});
    }
    s.entries.push_back({"Display Protocol", "Wayland (Native Client & Compositor)"});

    sections.push_back(std::move(s));
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_disp_monitors() {
    std::vector<HardwareSection> sections;
    HardwareSection s;
    s.title = "Connected Displays";

    auto monitors = MonitorBackend::get_monitors();
    if (monitors.empty()) {
        s.entries.push_back({"Display Output", "No active DRM output detected"});
    } else {
        s.badge = std::to_string(monitors.size()) + " Display" + (monitors.size() > 1 ? "s" : "");
        for (const auto& mon : monitors) {
            std::string desc = mon.description.empty() ? mon.name : mon.description;
            std::string res = std::to_string(mon.current_width) + " × " + std::to_string(mon.current_height);
            std::ostringstream ref_ss;
            ref_ss << std::fixed << std::setprecision(1) << mon.current_refresh << " Hz";

            s.entries.push_back({"Output (" + mon.name + ")", desc});
            s.entries.push_back({"Resolution & Refresh", res + " @ " + ref_ss.str()});
            if (mon.scale > 0.0f) {
                int scale_pct = static_cast<int>(std::round(mon.scale * 100.0f));
                s.entries.push_back({"Display Scale", std::to_string(scale_pct) + "%"});
            }
            if (mon.physical_width_mm > 0 && mon.physical_height_mm > 0) {
                s.entries.push_back({"Physical Size", std::to_string(mon.physical_width_mm) + " × " + std::to_string(mon.physical_height_mm) + " mm"});
            }
        }
    }

    sections.push_back(std::move(s));
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_disp_backlight() {
    std::vector<HardwareSection> sections;
    std::error_code ec;
    if (fs::exists("/sys/class/backlight", ec)) {
        for (const auto& entry : fs::directory_iterator("/sys/class/backlight", ec)) {
            HardwareSection s;
            std::string iface = entry.path().filename().string();
            s.title = "Backlight Hardware (" + iface + ")";

            std::string cur_b = read_file_trimmed(entry.path().string() + "/brightness");
            std::string max_b = read_file_trimmed(entry.path().string() + "/max_brightness");
            std::string type = read_file_trimmed(entry.path().string() + "/type");

            if (!cur_b.empty() && !max_b.empty()) {
                try {
                    int cur = std::stoi(cur_b);
                    int max = std::stoi(max_b);
                    if (max > 0) {
                        int pct = static_cast<int>(std::round((static_cast<float>(cur) / max) * 100.0f));
                        s.entries.push_back({"Current Level", std::to_string(cur) + " / " + std::to_string(max) + " steps (" + std::to_string(pct) + "%)"});
                        s.badge = std::to_string(pct) + "%";
                    }
                } catch (...) {}
            }
            if (!type.empty()) {
                s.entries.push_back({"Controller Type", type});
            }
            s.entries.push_back({"Interface Path", entry.path().string()});
            sections.push_back(std::move(s));
            break;
        }
    }
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_display_specs() {
    std::vector<HardwareSection> all;
    append_sections(all, get_disp_gpu());
    append_sections(all, get_disp_monitors());
    append_sections(all, get_disp_backlight());
    return all;
}

// ==============================================================================
// 4. Audio Subsystem (Subcategory Topics)
// ==============================================================================

std::vector<HardwareSection> HardwareBackend::get_aud_cards() {
    std::vector<HardwareSection> sections;
    HardwareSection s;
    s.title = "Audio Controller & Codecs";

    std::string cards = read_file_trimmed("/proc/asound/cards");
    if (!cards.empty()) {
        std::istringstream stream(cards);
        std::string line;
        while (std::getline(stream, line)) {
            size_t bracket = line.find('[');
            size_t close_bracket = line.find(']');
            if (bracket != std::string::npos && close_bracket != std::string::npos) {
                size_t colon = line.find(':', close_bracket);
                if (colon != std::string::npos) {
                    s.entries.push_back({"Sound Card", line.substr(colon + 2)});
                }
            }
        }
    }

    std::error_code ec;
    if (fs::exists("/proc/asound", ec)) {
        for (const auto& entry : fs::directory_iterator("/proc/asound", ec)) {
            std::string cname = entry.path().filename().string();
            if (cname.rfind("card", 0) == 0) {
                for (const auto& centry : fs::directory_iterator(entry.path(), ec)) {
                    std::string fname = centry.path().filename().string();
                    if (fname.rfind("codec#", 0) == 0) {
                        std::ifstream f(centry.path());
                        std::string line;
                        while (std::getline(f, line)) {
                            if (line.rfind("Codec: ", 0) == 0) {
                                s.entries.push_back({"Hardware Codec", line.substr(7)});
                                s.badge = line.substr(7);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    sections.push_back(std::move(s));
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_aud_endpoints() {
    std::vector<HardwareSection> sections;
    HardwareSection s;
    s.title = "Sound Server & Endpoints";

    auto state = SoundBackend::get_state();
    s.entries.push_back({"Audio Routing Engine", "PipeWire (Wayland Native Audio Graph)"});

    std::string def_out = "No active sink";
    for (const auto& dev : state.output_devices) {
        if (dev.is_default) {
            def_out = dev.name;
            break;
        }
    }
    s.entries.push_back({"Default Output Sink", def_out});
    s.entries.push_back({"Output Volume", std::to_string(state.output_volume) + "%" + (state.output_muted ? " (Muted)" : "")});

    std::string def_in = "No active source";
    for (const auto& dev : state.input_devices) {
        if (dev.is_default) {
            def_in = dev.name;
            break;
        }
    }
    s.entries.push_back({"Default Input Source", def_in});
    s.entries.push_back({"Input Gain", std::to_string(state.input_volume) + "%" + (state.input_muted ? " (Muted)" : "")});

    s.entries.push_back({"Available Sinks", std::to_string(state.output_devices.size()) + " audio endpoints"});
    s.entries.push_back({"Available Sources", std::to_string(state.input_devices.size()) + " capture endpoints"});
    s.badge = std::to_string(state.output_volume) + "%";

    sections.push_back(std::move(s));
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_audio_specs() {
    std::vector<HardwareSection> all;
    append_sections(all, get_aud_cards());
    append_sections(all, get_aud_endpoints());
    return all;
}

// ==============================================================================
// 5. Network Adapters (Subcategory Topics)
// ==============================================================================

static std::pair<std::string, std::string> get_interface_ips(const std::string& ifname) {
    struct ifaddrs* ifaddr = nullptr;
    getifaddrs(&ifaddr);
    std::string ipv4, ipv6;
    for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifname != ifa->ifa_name) continue;
        int family = ifa->ifa_addr->sa_family;
        if (family == AF_INET && ipv4.empty()) {
            char host[NI_MAXHOST];
            if (getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST) == 0) {
                ipv4 = host;
            }
        } else if (family == AF_INET6 && ipv6.empty()) {
            char host[NI_MAXHOST];
            if (getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in6), host, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST) == 0) {
                ipv6 = host;
            }
        }
    }
    if (ifaddr) freeifaddrs(ifaddr);
    return {ipv4, ipv6};
}

std::vector<HardwareSection> HardwareBackend::get_net_wifi() {
    std::vector<HardwareSection> sections;
    std::error_code ec;
    if (!fs::exists("/sys/class/net", ec)) return sections;

    for (const auto& entry : fs::directory_iterator("/sys/class/net", ec)) {
        std::string iface = entry.path().filename().string();
        if (iface == "lo") continue;
        bool is_wifi = fs::exists(entry.path().string() + "/wireless") || fs::exists(entry.path().string() + "/phy80211");
        if (!is_wifi) continue;

        std::string operstate = read_file_trimmed(entry.path().string() + "/operstate");
        std::string mac = read_file_trimmed(entry.path().string() + "/address");
        std::string mtu = read_file_trimmed(entry.path().string() + "/mtu");
        std::string speed = read_file_trimmed(entry.path().string() + "/speed");

        std::string driver;
        std::string driver_sym = entry.path().string() + "/device/driver";
        if (fs::is_symlink(driver_sym, ec)) {
            driver = fs::read_symlink(driver_sym, ec).filename().string();
        }

        HardwareSection s;
        s.title = "Wireless Interface (" + iface + ")";
        s.badge = (operstate == "up") ? "Link Active" : "Disconnected";

        s.entries.push_back({"Interface Name", iface});
        s.entries.push_back({"Device Type", "Wi-Fi (IEEE 802.11 Wireless)"});
        if (!mac.empty()) s.entries.push_back({"MAC Address", mac});
        if (!driver.empty()) s.entries.push_back({"Kernel Driver", driver});
        s.entries.push_back({"Operational State", operstate});
        if (!mtu.empty()) s.entries.push_back({"MTU", mtu + " bytes"});
        if (!speed.empty() && speed != "-1") s.entries.push_back({"Link Speed", speed + " Mbps"});

        auto [ipv4, ipv6] = get_interface_ips(iface);
        if (!ipv4.empty()) s.entries.push_back({"IPv4 Address", ipv4});
        if (!ipv6.empty()) s.entries.push_back({"IPv6 Address", ipv6});

        sections.push_back(std::move(s));
    }
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_net_ethernet() {
    std::vector<HardwareSection> sections;
    std::error_code ec;
    if (!fs::exists("/sys/class/net", ec)) return sections;

    for (const auto& entry : fs::directory_iterator("/sys/class/net", ec)) {
        std::string iface = entry.path().filename().string();
        if (iface == "lo") continue;
        bool is_wifi = fs::exists(entry.path().string() + "/wireless") || fs::exists(entry.path().string() + "/phy80211");
        if (is_wifi) continue;

        std::string operstate = read_file_trimmed(entry.path().string() + "/operstate");
        std::string mac = read_file_trimmed(entry.path().string() + "/address");
        std::string mtu = read_file_trimmed(entry.path().string() + "/mtu");
        std::string speed = read_file_trimmed(entry.path().string() + "/speed");
        std::string duplex = read_file_trimmed(entry.path().string() + "/duplex");

        std::string driver;
        std::string driver_sym = entry.path().string() + "/device/driver";
        if (fs::is_symlink(driver_sym, ec)) {
            driver = fs::read_symlink(driver_sym, ec).filename().string();
        }

        HardwareSection s;
        s.title = "Ethernet Interface (" + iface + ")";
        s.badge = (operstate == "up") ? "Link Active" : "Disconnected";

        s.entries.push_back({"Interface Name", iface});
        s.entries.push_back({"Device Type", "Ethernet (IEEE 802.3 Wired)"});
        if (!mac.empty()) s.entries.push_back({"MAC Address", mac});
        if (!driver.empty()) s.entries.push_back({"Kernel Driver", driver});
        s.entries.push_back({"Operational State", operstate});
        if (!mtu.empty()) s.entries.push_back({"MTU", mtu + " bytes"});
        if (!speed.empty() && speed != "-1") {
            std::string dup_str = duplex.empty() ? "" : (" (" + duplex + " Duplex)");
            s.entries.push_back({"Link Speed", speed + " Mbps" + dup_str});
        }

        auto [ipv4, ipv6] = get_interface_ips(iface);
        if (!ipv4.empty()) s.entries.push_back({"IPv4 Address", ipv4});
        if (!ipv6.empty()) s.entries.push_back({"IPv6 Address", ipv6});

        sections.push_back(std::move(s));
    }
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_net_bluetooth() {
    std::vector<HardwareSection> sections;
    std::error_code ec;
    if (fs::exists("/sys/class/bluetooth", ec)) {
        for (const auto& entry : fs::directory_iterator("/sys/class/bluetooth", ec)) {
            std::string bname = entry.path().filename().string();
            if (bname.rfind("hci", 0) == 0) {
                HardwareSection s;
                s.title = "Bluetooth Controller (" + bname + ")";
                std::string bmac = read_file_trimmed(entry.path().string() + "/address");
                if (!bmac.empty()) s.entries.push_back({"BD_ADDR (MAC)", bmac});
                std::string type = read_file_trimmed(entry.path().string() + "/type");
                if (!type.empty()) s.entries.push_back({"Controller Bus", type});
                s.badge = bname;
                sections.push_back(std::move(s));
                break;
            }
        }
    }
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_net_routing() {
    std::vector<HardwareSection> sections;
    HardwareSection s;
    s.title = "DNS & Network Routing";

    char hostname[256] = {0};
    if (gethostname(hostname, sizeof(hostname) - 1) == 0) {
        s.entries.push_back({"Host Name", hostname});
    }

    std::string def_route = run_cmd("ip route show default 2>/dev/null");
    if (!def_route.empty()) {
        std::istringstream stream(def_route);
        std::string word, gw, dev;
        while (stream >> word) {
            if (word == "via") stream >> gw;
            else if (word == "dev") stream >> dev;
        }
        if (!gw.empty()) {
            s.entries.push_back({"Default Gateway", gw + (!dev.empty() ? " (via " + dev + ")" : "")});
            s.badge = gw;
        }
    }

    std::ifstream res_file("/etc/resolv.conf");
    if (res_file.is_open()) {
        std::string line;
        std::string nameservers;
        std::string domain;
        while (std::getline(res_file, line)) {
            std::istringstream iss(line);
            std::string k, v;
            if (iss >> k >> v) {
                if (k == "nameserver") {
                    if (!nameservers.empty()) nameservers += ", ";
                    nameservers += v;
                } else if (k == "search" || k == "domain") {
                    domain = v;
                }
            }
        }
        if (!nameservers.empty()) {
            s.entries.push_back({"Nameservers", nameservers});
        }
        if (!domain.empty() && domain != ".") {
            s.entries.push_back({"Search Domain", domain});
        }
    }

    sections.push_back(std::move(s));
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_network_specs() {
    std::vector<HardwareSection> all;
    append_sections(all, get_net_wifi());
    append_sections(all, get_net_ethernet());
    append_sections(all, get_net_bluetooth());
    append_sections(all, get_net_routing());
    return all;
}

// ==============================================================================
// 6. Storage & Drives (Subcategory Topics)
// ==============================================================================

std::vector<HardwareSection> HardwareBackend::get_stor_drives() {
    std::vector<HardwareSection> sections;
    HardwareSection s;
    s.title = "Physical Storage Drives";

    std::string lsblk_out = run_cmd("lsblk -d -n -o NAME,MODEL,SIZE,ROTA,TRAN 2>/dev/null");
    if (!lsblk_out.empty()) {
        std::istringstream stream(lsblk_out);
        std::string line;
        while (std::getline(stream, line)) {
            std::istringstream liness(line);
            std::string name;
            liness >> name;
            if (name.rfind("loop", 0) == 0 || name.rfind("zram", 0) == 0) continue;
            s.entries.push_back({"Drive (/dev/" + name + ")", line.substr(name.length() + 1)});
        }
    }
    if (!s.entries.empty()) {
        s.badge = std::to_string(s.entries.size()) + " Drive" + (s.entries.size() > 1 ? "s" : "");
        sections.push_back(std::move(s));
    }
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_stor_mounts() {
    std::vector<HardwareSection> sections;
    HardwareSection s;
    s.title = "Filesystem Mounts";

    const std::vector<std::string> mount_points = {"/", "/boot", "/home"};
    for (const auto& mp : mount_points) {
        struct statvfs stat;
        if (statvfs(mp.c_str(), &stat) == 0) {
            unsigned long long total = stat.f_blocks * stat.f_frsize;
            unsigned long long free = stat.f_bavail * stat.f_frsize;
            if (total == 0) continue;
            unsigned long long used = total - free;
            int pct = static_cast<int>(std::round((static_cast<double>(used) / total) * 100.0));

            s.entries.push_back({"Mount (" + mp + ")", format_bytes(used) + " of " + format_bytes(total) + " (" + std::to_string(pct) + "% used)"});
            s.entries.push_back({"Available (" + mp + ")", format_bytes(free) + " free"});
        }
    }
    sections.push_back(std::move(s));
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_stor_zram() {
    std::vector<HardwareSection> sections;
    std::error_code ec;
    if (fs::exists("/sys/block/zram0", ec)) {
        HardwareSection s;
        s.title = "In-Memory Compressed Storage (ZRAM)";
        s.badge = "RAM Swap";

        std::string disksize_s = read_file_trimmed("/sys/block/zram0/disksize");
        std::string comp_algo = read_file_trimmed("/sys/block/zram0/comp_algorithm");
        std::string mem_used_total = read_file_trimmed("/sys/block/zram0/mem_used_total");

        s.entries.push_back({"Device Node", "/dev/zram0"});
        if (!disksize_s.empty()) {
            try {
                unsigned long long sz = std::stoull(disksize_s);
                s.entries.push_back({"Virtual Capacity", format_bytes(sz)});
                s.badge = format_bytes(sz);
            } catch (...) {}
        }
        if (!comp_algo.empty()) {
            auto start = comp_algo.find('[');
            auto end = comp_algo.find(']');
            if (start != std::string::npos && end != std::string::npos && end > start) {
                s.entries.push_back({"Compression Algorithm", comp_algo.substr(start + 1, end - start - 1) + " (Transparent RAM Compression)"});
            } else {
                s.entries.push_back({"Compression Algorithm", comp_algo});
            }
        }
        if (!mem_used_total.empty()) {
            try {
                unsigned long long mu = std::stoull(mem_used_total);
                s.entries.push_back({"Physical RAM Backing", format_bytes(mu)});
            } catch (...) {}
        }
        s.entries.push_back({"Purpose", "Zero-Latency Compressed In-Memory Swap"});
        sections.push_back(std::move(s));
    }
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_storage_specs() {
    std::vector<HardwareSection> all;
    append_sections(all, get_stor_drives());
    append_sections(all, get_stor_mounts());
    append_sections(all, get_stor_zram());
    return all;
}

// ==============================================================================
// 7. Battery & Power (Subcategory Topics)
// ==============================================================================

std::vector<HardwareSection> HardwareBackend::get_pwr_battery() {
    std::vector<HardwareSection> sections;
    std::error_code ec;
    if (!fs::exists("/sys/class/power_supply", ec)) return sections;

    for (const auto& entry : fs::directory_iterator("/sys/class/power_supply", ec)) {
        std::string name = entry.path().filename().string();
        if (name.rfind("BAT", 0) == 0) {
            HardwareSection s;
            s.title = "Battery Cell Telemetry (" + name + ")";

            std::string status = read_file_trimmed(entry.path().string() + "/status");
            std::string tech = read_file_trimmed(entry.path().string() + "/technology");
            std::string model = read_file_trimmed(entry.path().string() + "/model_name");
            std::string mfg = read_file_trimmed(entry.path().string() + "/manufacturer");
            std::string cap = read_file_trimmed(entry.path().string() + "/capacity");
            std::string cycles = read_file_trimmed(entry.path().string() + "/cycle_count");
            std::string volt = read_file_trimmed(entry.path().string() + "/voltage_now");

            s.badge = !status.empty() ? (status + " (" + cap + "%)") : "Battery";

            if (!mfg.empty() || !model.empty()) {
                s.entries.push_back({"Model & Vendor", mfg + " " + model});
            }
            if (!tech.empty()) {
                s.entries.push_back({"Chemistry", tech});
            }
            if (!status.empty()) {
                s.entries.push_back({"State", status + " (" + cap + "%)"});
            }

            long long c_full = BatteryBackend::read_sysfs_ll(entry.path().string() + "/charge_full", 0);
            long long c_des = BatteryBackend::read_sysfs_ll(entry.path().string() + "/charge_full_design", 0);
            if (c_full == 0) c_full = BatteryBackend::read_sysfs_ll(entry.path().string() + "/energy_full", 0);
            if (c_des == 0) c_des = BatteryBackend::read_sysfs_ll(entry.path().string() + "/energy_full_design", 0);

            if (c_des > 0 && c_full > 0) {
                float health = (static_cast<float>(c_full) / static_cast<float>(c_des)) * 100.0f;
                std::ostringstream h_ss;
                h_ss << std::fixed << std::setprecision(1) << health << "%";
                s.entries.push_back({"Battery Health", h_ss.str() + " of original design capacity"});
            }

            if (!volt.empty()) {
                try {
                    float v = std::stof(volt) / 1000000.0f;
                    std::ostringstream v_ss;
                    v_ss << std::fixed << std::setprecision(2) << v << " V";
                    s.entries.push_back({"Instantaneous Voltage", v_ss.str()});
                } catch (...) {}
            }

            if (!cycles.empty() && cycles != "0") {
                s.entries.push_back({"Cycle Count", cycles + " recharge cycles"});
            }

            sections.push_back(std::move(s));
        }
    }
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_pwr_adapter() {
    std::vector<HardwareSection> sections;
    std::error_code ec;
    if (!fs::exists("/sys/class/power_supply", ec)) return sections;

    for (const auto& entry : fs::directory_iterator("/sys/class/power_supply", ec)) {
        std::string name = entry.path().filename().string();
        if (name == "AC" || name.rfind("ACAD", 0) == 0) {
            HardwareSection s;
            s.title = "AC Power Adapter (" + name + ")";
            std::string online = read_file_trimmed(entry.path().string() + "/online");
            s.badge = (online == "1") ? "Connected" : "Disconnected";
            s.entries.push_back({"Adapter Status", (online == "1") ? "Mains AC Connected" : "Running on Battery Power"});
            sections.push_back(std::move(s));
        }
    }
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_pwr_profile() {
    std::vector<HardwareSection> sections;
    HardwareSection s;
    s.title = "ACPI Platform Power Profile";
    std::string prof = BatteryBackend::get_power_profile();
    s.badge = prof;
    s.entries.push_back({"Active Profile", prof});

    std::string acpi_choices = read_file_trimmed("/sys/firmware/acpi/platform_profile_choices");
    if (!acpi_choices.empty()) {
        s.entries.push_back({"Supported Profiles", acpi_choices});
    }

    std::string gov = read_file_trimmed("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");
    if (!gov.empty()) {
        s.entries.push_back({"CPU Energy Policy", gov});
    }

    sections.push_back(std::move(s));
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_power_specs() {
    std::vector<HardwareSection> all;
    append_sections(all, get_pwr_battery());
    append_sections(all, get_pwr_adapter());
    append_sections(all, get_pwr_profile());
    return all;
}

// ==============================================================================
// 8. Input Devices (Subcategory Topics)
// ==============================================================================

static void parse_input_devices(
    std::vector<std::pair<std::string, std::string>>& keyboards,
    std::vector<std::pair<std::string, std::string>>& pointers,
    std::vector<std::pair<std::string, std::string>>& touchscreens,
    std::vector<std::pair<std::string, std::string>>& others
) {
    std::ifstream dev_file("/proc/bus/input/devices");
    if (!dev_file.is_open()) return;

    std::string line, cur_name, cur_handlers;

    auto flush_device = [&]() {
        if (cur_name.empty()) return;
        std::string lower = cur_name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        if (lower.find("touchscreen") != std::string::npos || lower.find("digitizer") != std::string::npos) {
            touchscreens.push_back({cur_name, cur_handlers});
        } else if (lower.find("mouse") != std::string::npos || lower.find("touchpad") != std::string::npos ||
                   lower.find("stick") != std::string::npos || cur_handlers.find("mouse") != std::string::npos) {
            pointers.push_back({cur_name, cur_handlers});
        } else if (lower.find("keyboard") != std::string::npos || cur_handlers.find("kbd") != std::string::npos) {
            if (lower.find("button") != std::string::npos || lower.find("switch") != std::string::npos || lower.find("hotkey") != std::string::npos) {
                others.push_back({cur_name, cur_handlers});
            } else {
                keyboards.push_back({cur_name, cur_handlers});
            }
        } else {
            others.push_back({cur_name, cur_handlers});
        }

        cur_name.clear();
        cur_handlers.clear();
    };

    while (std::getline(dev_file, line)) {
        if (line.empty()) {
            flush_device();
            continue;
        }
        if (line.rfind("N: Name=", 0) == 0) {
            cur_name = line.substr(8);
            if (!cur_name.empty() && cur_name.front() == '"') cur_name.erase(0, 1);
            if (!cur_name.empty() && cur_name.back() == '"') cur_name.pop_back();
        } else if (line.rfind("H: Handlers=", 0) == 0) {
            cur_handlers = line.substr(12);
        }
    }
    flush_device();
}

std::vector<HardwareSection> HardwareBackend::get_inp_keyboards() {
    std::vector<HardwareSection> sections;
    std::vector<std::pair<std::string, std::string>> keyboards, pointers, touchscreens, others;
    parse_input_devices(keyboards, pointers, touchscreens, others);

    if (!keyboards.empty()) {
        HardwareSection s;
        s.title = "Keyboards";
        s.badge = std::to_string(keyboards.size()) + " Keyboard" + (keyboards.size() > 1 ? "s" : "");
        for (const auto& [name, handlers] : keyboards) {
            s.entries.push_back({name, handlers});
        }
        sections.push_back(std::move(s));
    }
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_inp_pointers() {
    std::vector<HardwareSection> sections;
    std::vector<std::pair<std::string, std::string>> keyboards, pointers, touchscreens, others;
    parse_input_devices(keyboards, pointers, touchscreens, others);

    if (!pointers.empty()) {
        HardwareSection s;
        s.title = "Mice & Touchpads";
        s.badge = std::to_string(pointers.size()) + " Pointer" + (pointers.size() > 1 ? "s" : "");
        for (const auto& [name, handlers] : pointers) {
            s.entries.push_back({name, handlers});
        }
        sections.push_back(std::move(s));
    }
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_inp_touch() {
    std::vector<HardwareSection> sections;
    std::vector<std::pair<std::string, std::string>> keyboards, pointers, touchscreens, others;
    parse_input_devices(keyboards, pointers, touchscreens, others);

    if (!touchscreens.empty()) {
        HardwareSection s;
        s.title = "Touchscreens & Digitizers";
        s.badge = std::to_string(touchscreens.size()) + " Device" + (touchscreens.size() > 1 ? "s" : "");
        for (const auto& [name, handlers] : touchscreens) {
            s.entries.push_back({name, handlers});
        }
        sections.push_back(std::move(s));
    }
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_inp_switches() {
    std::vector<HardwareSection> sections;
    std::vector<std::pair<std::string, std::string>> keyboards, pointers, touchscreens, others;
    parse_input_devices(keyboards, pointers, touchscreens, others);

    if (!others.empty()) {
        HardwareSection s;
        s.title = "Chassis Switches & Hotkeys";
        s.badge = std::to_string(others.size()) + " Switch" + (others.size() > 1 ? "es" : "");
        for (const auto& [name, handlers] : others) {
            s.entries.push_back({name, handlers});
        }
        sections.push_back(std::move(s));
    }
    return sections;
}

std::vector<HardwareSection> HardwareBackend::get_input_specs() {
    std::vector<HardwareSection> all;
    append_sections(all, get_inp_keyboards());
    append_sections(all, get_inp_pointers());
    append_sections(all, get_inp_touch());
    append_sections(all, get_inp_switches());
    return all;
}

// ==============================================================================
// Generic Dispatch & Detailed Description Lookup
// ==============================================================================

std::vector<HardwareSection> HardwareBackend::get_spec_by_id(const std::string& id) {
    if (id == "spec_sys_board") return get_sys_board();
    if (id == "spec_sys_firmware") return get_sys_firmware();
    if (id == "spec_sys_cpu") return get_sys_cpu();
    if (id == "spec_sys_thermals") return get_sys_thermals();
    if (id == "spec_sys_memory") return get_sys_memory();
    if (id == "spec_sys_uptime") return get_sys_uptime();

    if (id == "spec_srv_os") return get_srv_os();
    if (id == "spec_srv_desktop") return get_srv_desktop();
    if (id == "spec_srv_network") return get_srv_network();
    if (id == "spec_srv_security") return get_srv_security();
    if (id == "spec_srv_sound") return get_srv_sound();
    if (id == "spec_srv_power") return get_srv_power();
    if (id == "spec_srv_packages") return get_srv_packages();

    if (id == "spec_disp_gpu") return get_disp_gpu();
    if (id == "spec_disp_monitors") return get_disp_monitors();
    if (id == "spec_disp_backlight") return get_disp_backlight();

    if (id == "spec_aud_cards") return get_aud_cards();
    if (id == "spec_aud_endpoints") return get_aud_endpoints();

    if (id == "spec_net_wifi") return get_net_wifi();
    if (id == "spec_net_ethernet") return get_net_ethernet();
    if (id == "spec_net_bluetooth") return get_net_bluetooth();
    if (id == "spec_net_routing") return get_net_routing();

    if (id == "spec_stor_drives") return get_stor_drives();
    if (id == "spec_stor_mounts") return get_stor_mounts();
    if (id == "spec_stor_zram") return get_stor_zram();

    if (id == "spec_pwr_battery") return get_pwr_battery();
    if (id == "spec_pwr_adapter") return get_pwr_adapter();
    if (id == "spec_pwr_profile") return get_pwr_profile();

    if (id == "spec_inp_keyboards") return get_inp_keyboards();
    if (id == "spec_inp_pointers") return get_inp_pointers();
    if (id == "spec_inp_touch") return get_inp_touch();
    if (id == "spec_inp_switches") return get_inp_switches();

    // Fallbacks for whole subcategories
    if (id == "cat_spec_system" || id == "spec_system") return get_system_specs();
    if (id == "cat_spec_services" || id == "spec_services") return get_service_specs();
    if (id == "cat_spec_display" || id == "spec_display") return get_display_specs();
    if (id == "cat_spec_audio" || id == "spec_audio") return get_audio_specs();
    if (id == "cat_spec_network" || id == "spec_network") return get_network_specs();
    if (id == "cat_spec_storage" || id == "spec_storage") return get_storage_specs();
    if (id == "cat_spec_power" || id == "spec_power") return get_power_specs();
    if (id == "cat_spec_input" || id == "spec_input") return get_input_specs();

    return {};
}

std::string HardwareBackend::get_spec_description(const std::string& id) {
    if (id == "spec_sys_board") return "System product model, motherboard vendor, chassis & device identity";
    if (id == "spec_sys_firmware") return "Motherboard firmware vendor, BIOS revision, UEFI runtime & Secure Boot verification";
    if (id == "spec_sys_cpu") return "CPU microarchitecture, thread count, live clocks, governor & cache hierarchy";
    if (id == "spec_sys_thermals") return "Real-time thermal sensor telemetry for CPU package, motherboard PCH & fan RPM";
    if (id == "spec_sys_memory") return "Volatile memory footprint, kernel buffers, page cache & swap allocation";
    if (id == "spec_sys_uptime") return "System uptime duration, 1m/5m/15m load averages & active task scheduling";

    if (id == "spec_srv_os") return "Operating system distribution, kernel version, desktop compositor & protocol";
    if (id == "spec_srv_desktop") return "Miquland core desktop environment daemons, bar, notification & idle services";
    if (id == "spec_srv_network") return "Active connectivity backends, NetworkManager, wireless supplicant & BlueZ";
    if (id == "spec_srv_security") return "Kernel Netfilter state, UFW firewall rules, OpenSnitch monitor & Polkit";
    if (id == "spec_srv_sound") return "PipeWire multimedia routing server, WirePlumber session manager & ALSA";
    if (id == "spec_srv_power") return "Platform power profile manager, UPower device abstraction & ACPI daemon";
    if (id == "spec_srv_packages") return "Native pacman package database, Flatpak runtimes & ELF architecture";

    if (id == "spec_disp_gpu") return "Graphics adapter PCI controller, kernel DRM driver & userspace Mesa driver";
    if (id == "spec_disp_monitors") return "Connected display panels, native resolution, refresh rate & physical size";
    if (id == "spec_disp_backlight") return "Hardware display panel luminance controller & sysfs brightness steps";

    if (id == "spec_aud_cards") return "Physical sound cards, PCI audio controllers & hardware codec chips";
    if (id == "spec_aud_endpoints") return "Active PipeWire playback and capture endpoints, gain levels & volume state";

    if (id == "spec_net_wifi") return "802.11 wireless adapters, chipset drivers, MAC addresses & link metrics";
    if (id == "spec_net_ethernet") return "802.3 wired Ethernet controllers, duplex modes, link speed & IP addressing";
    if (id == "spec_net_bluetooth") return "Bluetooth HCI host controller interface, BD_ADDR MAC & bus transport";
    if (id == "spec_net_routing") return "Local hostname, default gateway route, active upstream DNS servers & search domains";

    if (id == "spec_stor_drives") return "Physical NVMe and SATA block drives, storage capacity & rotational flag";
    if (id == "spec_stor_mounts") return "Mounted filesystem volumes, total capacity, allocated space & usage percentage";
    if (id == "spec_stor_zram") return "Zero-latency dynamic RAM compressed swap partition with ZSTD algorithm";

    if (id == "spec_pwr_battery") return "Battery cell chemistry, instantaneous voltage, charge cycles & health degradation";
    if (id == "spec_pwr_adapter") return "Mains AC adapter connectivity state and electrical power source";
    if (id == "spec_pwr_profile") return "ACPI platform power profile, hardware performance presets & CPU energy governor";

    if (id == "spec_inp_keyboards") return "Physical and virtual keyboard input devices parsed from kernel evdev";
    if (id == "spec_inp_pointers") return "Pointing devices, optical mice, touchpads & TrackPoints";
    if (id == "spec_inp_touch") return "Touchscreen digitizers, capacitive multi-touch surfaces & stylus sensors";
    if (id == "spec_inp_switches") return "Chassis lid switches, power keys, hotkey buttons & tablet mode sensors";

    return "Hardware, driver, and system daemon diagnostics";
}

} // namespace miqu
