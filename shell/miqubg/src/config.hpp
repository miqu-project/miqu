#pragma once

#include "wallpaper_manager.hpp"
#include <string>
#include <map>
#include <vector>
#include <chrono>
#include <atomic>
#include <mutex>

namespace miqubg {

class Config {
public:
    static Config& get();

    bool load(const std::string& custom_path = "");
    bool save(const std::string& custom_path = "");
    static std::string init_user_config();

    static std::string get_user_config_path();
    static std::string get_system_config_path();

    std::string get_config_path() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_config_path;
    }
    void set_config_path(const std::string& path) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config_path = path;
    }

    OutputConfig get_default_config() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_default_config;
    }
    void set_default_config(const OutputConfig& cfg) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_default_config = cfg;
    }

    std::map<std::string, OutputConfig> get_output_configs() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_output_configs;
    }
    void set_output_config(const std::string& output, const OutputConfig& cfg) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (output.empty() || output == "*") {
            m_default_config = cfg;
            m_output_configs.clear();
        } else {
            m_output_configs[output] = cfg;
        }
    }

    OutputConfig get_config_for_output(const std::string& output) const;

    std::vector<std::string> get_loaded_files() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_loaded_files;
    }

    bool consume_internal_write_flag();

    // Utility methods
    static std::string mode_to_string(miqu::FitMode mode);
    static miqu::FitMode string_to_mode(const std::string& str);
    static std::string color_to_hex(const miqu::Color& c);
    static miqu::Color hex_to_color(const std::string& hex, const miqu::Color& fallback = miqu::Color::rgb(0.0f, 0.0f, 0.0f));
    static std::string expand_path(const std::string& path);

private:
    Config();
    void load_file_internal(const std::string& path, int depth);

    mutable std::mutex m_mutex;
    std::string m_config_path;
    OutputConfig m_default_config;
    std::map<std::string, OutputConfig> m_output_configs;
    std::vector<std::string> m_loaded_files;

    std::atomic<bool> m_internal_write_flag{false};
    std::chrono::steady_clock::time_point m_internal_write_time;
};

} // namespace miqubg
