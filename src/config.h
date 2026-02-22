#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

inline constexpr const char* DEFAULT_IPC_SOCKET_PATH = "/run/rpiradio/rpiradio.sock";

struct Config {
    std::string m3u_path = "/etc/rpiradio/stations.m3u";
    std::string mqtt_host = "localhost";
    int mqtt_port = 1883;
    std::string topic_prefix = "rpiradio";
    std::string log_level = "INFO";
    std::vector<std::string> mpv_extra_args;
    std::string ipc_socket_path = DEFAULT_IPC_SOCKET_PATH;
    std::string mpv_socket_path = "/run/rpiradio/mpv.sock";
};

Config config_load();
nlohmann::json config_to_json(const Config& cfg);
Config config_from_json(const nlohmann::json& j);
