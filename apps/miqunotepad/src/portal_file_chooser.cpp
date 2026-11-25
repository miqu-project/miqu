#include "portal_file_chooser.hpp"

#include <systemd/sd-bus.h>

#include <atomic>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <string>

namespace miqu {

namespace {

std::atomic<uint64_t> g_token_counter{0};

std::string percent_decode(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '%' && i + 2 < input.size() &&
            std::isxdigit(static_cast<unsigned char>(input[i + 1])) &&
            std::isxdigit(static_cast<unsigned char>(input[i + 2]))) {
            unsigned int hex_val = 0;
            auto hex_digit = [](char c) -> unsigned int {
                if (c >= '0' && c <= '9') return static_cast<unsigned int>(c - '0');
                if (c >= 'a' && c <= 'f') return static_cast<unsigned int>(c - 'a' + 10);
                return static_cast<unsigned int>(c - 'A' + 10);
            };
            hex_val = (hex_digit(input[i + 1]) << 4) | hex_digit(input[i + 2]);
            out.push_back(static_cast<char>(hex_val));
            i += 2;
        } else {
            out.push_back(input[i]);
        }
    }
    return out;
}

std::string uri_to_local_path(const std::string& uri) {
    constexpr const char* prefix = "file://";
    if (uri.rfind(prefix, 0) != 0) return "";
    return percent_decode(uri.substr(7));
}

struct PortalWaitState {
    bool done = false;
    uint32_t response_code = 2;
    std::string path;
};

int on_portal_response(sd_bus_message* m, void* userdata, sd_bus_error* /*ret_error*/) {
    auto* state = static_cast<PortalWaitState*>(userdata);
    if (!state) return 0;

    uint32_t response = 2;
    if (sd_bus_message_read(m, "u", &response) < 0) {
        state->done = true;
        return 0;
    }
    state->response_code = response;

    if (response == 0 && sd_bus_message_enter_container(m, 'a', "{sv}") >= 0) {
        while (sd_bus_message_enter_container(m, 'e', "sv") > 0) {
            const char* key = nullptr;
            if (sd_bus_message_read(m, "s", &key) >= 0 && key && std::string(key) == "uris") {
                if (sd_bus_message_enter_container(m, 'v', "as") >= 0) {
                    if (sd_bus_message_enter_container(m, 'a', "s") >= 0) {
                        const char* uri = nullptr;
                        if (sd_bus_message_read(m, "s", &uri) > 0 && uri) {
                            state->path = uri_to_local_path(uri);
                        }
                        sd_bus_message_exit_container(m);
                    }
                    sd_bus_message_exit_container(m);
                }
            } else {
                sd_bus_message_skip(m, "v");
            }
            sd_bus_message_exit_container(m);
        }
        sd_bus_message_exit_container(m);
    }

    state->done = true;
    return 0;
}

int append_dict_string(sd_bus_message* m, const char* key, const char* val) {
    int r = sd_bus_message_open_container(m, 'e', "sv");
    if (r < 0) return r;
    r = sd_bus_message_append(m, "s", key);
    if (r < 0) return r;
    r = sd_bus_message_open_container(m, 'v', "s");
    if (r < 0) return r;
    r = sd_bus_message_append(m, "s", val);
    if (r < 0) return r;
    r = sd_bus_message_close_container(m);
    if (r < 0) return r;
    return sd_bus_message_close_container(m);
}

int append_dict_bool(sd_bus_message* m, const char* key, int val) {
    int r = sd_bus_message_open_container(m, 'e', "sv");
    if (r < 0) return r;
    r = sd_bus_message_append(m, "s", key);
    if (r < 0) return r;
    r = sd_bus_message_open_container(m, 'v', "b");
    if (r < 0) return r;
    r = sd_bus_message_append(m, "b", val);
    if (r < 0) return r;
    r = sd_bus_message_close_container(m);
    if (r < 0) return r;
    return sd_bus_message_close_container(m);
}

int append_dict_byte_array(sd_bus_message* m, const char* key, const std::string& val) {
    int r = sd_bus_message_open_container(m, 'e', "sv");
    if (r < 0) return r;
    r = sd_bus_message_append(m, "s", key);
    if (r < 0) return r;
    r = sd_bus_message_open_container(m, 'v', "ay");
    if (r < 0) return r;
    // Portal current_folder is a nul-terminated byte array
    r = sd_bus_message_append_array(m, 'y', val.c_str(), val.size() + 1);
    if (r < 0) return r;
    r = sd_bus_message_close_container(m);
    if (r < 0) return r;
    return sd_bus_message_close_container(m);
}

PortalPickResult run_portal_chooser(bool save_mode,
                                      const std::string& title,
                                      const std::string& current_name,
                                      const std::string& current_dir) {
    PortalPickResult result;

    sd_bus* bus = nullptr;
    if (sd_bus_open_user(&bus) < 0 || !bus) {
        return result;
    }

    const char* unique_name = nullptr;
    if (sd_bus_get_unique_name(bus, &unique_name) < 0 || !unique_name) {
        sd_bus_unref(bus);
        return result;
    }

    std::string sender(unique_name);
    if (!sender.empty() && sender[0] == ':') sender.erase(0, 1);
    for (char& c : sender) {
        if (c == '.') c = '_';
    }

    std::string token = "miqu_notepad_" + std::to_string(++g_token_counter);
    std::string request_path = "/org/freedesktop/portal/desktop/request/" + sender + "/" + token;

    PortalWaitState state;
    sd_bus_slot* slot = nullptr;
    if (sd_bus_match_signal(bus, &slot,
                            "org.freedesktop.portal.Desktop",
                            request_path.c_str(),
                            "org.freedesktop.portal.Request",
                            "Response",
                            on_portal_response,
                            &state) < 0) {
        sd_bus_unref(bus);
        return result;
    }

    sd_bus_message* call = nullptr;
    const char* method = save_mode ? "SaveFile" : "OpenFile";
    if (sd_bus_message_new_method_call(bus, &call,
                                       "org.freedesktop.portal.Desktop",
                                       "/org/freedesktop/portal/desktop",
                                       "org.freedesktop.portal.FileChooser",
                                       method) < 0) {
        sd_bus_slot_unref(slot);
        sd_bus_unref(bus);
        return result;
    }

    bool ok = (sd_bus_message_append(call, "ss", "", title.c_str()) >= 0) &&
              (sd_bus_message_open_container(call, 'a', "{sv}") >= 0);

    if (ok) ok = (append_dict_string(call, "handle_token", token.c_str()) >= 0);
    if (ok) ok = (append_dict_bool(call, "modal", 1) >= 0);
    if (ok && save_mode && !current_name.empty()) {
        ok = (append_dict_string(call, "current_name", current_name.c_str()) >= 0);
    }
    if (ok && !current_dir.empty()) {
        ok = (append_dict_byte_array(call, "current_folder", current_dir) >= 0);
    }
    if (ok) ok = (sd_bus_message_close_container(call) >= 0);

    if (!ok) {
        sd_bus_message_unref(call);
        sd_bus_slot_unref(slot);
        sd_bus_unref(bus);
        return result;
    }

    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message* reply = nullptr;
    int r = sd_bus_call(bus, call, 0, &error, &reply);
    sd_bus_message_unref(call);
    if (r < 0) {
        sd_bus_error_free(&error);
        if (reply) sd_bus_message_unref(reply);
        sd_bus_slot_unref(slot);
        sd_bus_unref(bus);
        return result;
    }

    const char* actual_path = nullptr;
    if (sd_bus_message_read(reply, "o", &actual_path) >= 0 && actual_path &&
        request_path != actual_path) {
        sd_bus_slot_unref(slot);
        slot = nullptr;
        sd_bus_match_signal(bus, &slot,
                            "org.freedesktop.portal.Desktop",
                            actual_path,
                            "org.freedesktop.portal.Request",
                            "Response",
                            on_portal_response,
                            &state);
    }
    sd_bus_message_unref(reply);
    sd_bus_error_free(&error);

    result.available = true;
    while (!state.done) {
        r = sd_bus_process(bus, nullptr);
        if (r < 0) break;
        if (r > 0) continue;
        r = sd_bus_wait(bus, static_cast<uint64_t>(200000)); // 200ms
        if (r < 0 && -r != EINTR) break;
    }

    if (state.response_code == 0 && !state.path.empty()) {
        result.accepted = true;
        result.path = std::move(state.path);
    }

    if (slot) sd_bus_slot_unref(slot);
    sd_bus_unref(bus);
    return result;
}

} // namespace

PortalPickResult PortalFileChooser::pick_open_file(const std::string& title,
                                                   const std::string& current_dir) {
    return run_portal_chooser(false, title, "", current_dir);
}

PortalPickResult PortalFileChooser::pick_save_file(const std::string& title,
                                                   const std::string& current_name,
                                                   const std::string& current_dir) {
    return run_portal_chooser(true, title, current_name, current_dir);
}

} // namespace miqu
