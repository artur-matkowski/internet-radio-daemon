#include "config.h"
#include "log.h"
#include <fstream>

using json = nlohmann::json;

static constexpr const char* CONFIG_PATH = "/etc/rpiradio/config.json";

nlohmann::json config_to_json(const Config& cfg) {
    json j;
    j["m3u_path"] = cfg.m3u_path;
    j["mqtt_host"] = cfg.mqtt_host;
    j["mqtt_port"] = cfg.mqtt_port;
    j["topic_prefix"] = cfg.topic_prefix;
    j["log_level"] = cfg.log_level;
    j["mpv_extra_args"] = cfg.mpv_extra_args;
    j["ipc_socket_path"] = cfg.ipc_socket_path;
    j["mpv_socket_path"] = cfg.mpv_socket_path;
    return j;
}

Config config_from_json(const nlohmann::json& j) {
    Config cfg;
    if (j.contains("m3u_path"))       cfg.m3u_path       = j["m3u_path"].get<std::string>();
    if (j.contains("mqtt_host"))      cfg.mqtt_host       = j["mqtt_host"].get<std::string>();
    if (j.contains("mqtt_port"))      cfg.mqtt_port       = j["mqtt_port"].get<int>();
    if (j.contains("topic_prefix"))   cfg.topic_prefix    = j["topic_prefix"].get<std::string>();
    if (j.contains("log_level"))      cfg.log_level       = j["log_level"].get<std::string>();
    if (j.contains("mpv_extra_args")) cfg.mpv_extra_args  = j["mpv_extra_args"].get<std::vector<std::string>>();
    if (j.contains("ipc_socket_path"))cfg.ipc_socket_path = j["ipc_socket_path"].get<std::string>();
    if (j.contains("mpv_socket_path"))cfg.mpv_socket_path = j["mpv_socket_path"].get<std::string>();
    return cfg;
}

Config config_load() {
    std::ifstream f(CONFIG_PATH);
    if (f.good()) {
        try {
            json j = json::parse(f);
            Config cfg = config_from_json(j);
            LOG_INFO("loaded config from %s", CONFIG_PATH);
            return cfg;
        } catch (const json::exception& e) {
            LOG_ERROR("failed to parse config %s: %s", CONFIG_PATH, e.what());
        }
    } else {
        LOG_INFO("no config at %s, using defaults", CONFIG_PATH);
    }

    return Config{};
}
