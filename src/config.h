#pragma once

#include <string>
#include <map>
#include <vector>
#include <nlohmann/json.hpp>

struct Config {
    std::string m3u_path = "/etc/rpiradio/stations.m3u";
    std::string mqtt_host = "localhost";
    int mqtt_port = 1883;
    std::string topic_prefix = "rpiradio";
    std::string evdev_name;
    std::map<std::string, std::string> bindings;
    std::string log_level = "INFO";
    std::vector<std::string> mpv_extra_args;
    std::string ipc_socket_path = "/tmp/rpiradio.sock";
    std::string mpv_socket_path = "/tmp/rpiradio-mpv.sock";
};

std::string config_path();
Config config_load();
void config_save(const Config& cfg);
nlohmann::json config_to_json(const Config& cfg);
Config config_from_json(const nlohmann::json& j);
