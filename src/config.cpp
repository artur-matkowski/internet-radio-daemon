#include "config.h"
#include "log.h"
#include <fstream>
#include <cstdlib>
#include <sys/stat.h>

using json = nlohmann::json;

static std::string ensure_parent_dir(const std::string& path) {
    auto pos = path.rfind('/');
    if (pos != std::string::npos) {
        std::string dir = path.substr(0, pos);
        mkdir(dir.c_str(), 0755);
    }
    return path;
}

std::string config_path() {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    std::string base;
    if (xdg && xdg[0]) {
        base = xdg;
    } else {
        const char* home = std::getenv("HOME");
        base = std::string(home ? home : "/tmp") + "/.config";
    }
    return base + "/rpiradio/config.json";
}

nlohmann::json config_to_json(const Config& cfg) {
    json j;
    j["m3u_path"] = cfg.m3u_path;
    j["mqtt_host"] = cfg.mqtt_host;
    j["mqtt_port"] = cfg.mqtt_port;
    j["topic_prefix"] = cfg.topic_prefix;
    j["evdev_name"] = cfg.evdev_name;
    j["bindings"] = cfg.bindings;
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
    if (j.contains("evdev_name"))     cfg.evdev_name      = j["evdev_name"].get<std::string>();
    if (j.contains("bindings"))       cfg.bindings        = j["bindings"].get<std::map<std::string, std::string>>();
    if (j.contains("log_level"))      cfg.log_level       = j["log_level"].get<std::string>();
    if (j.contains("mpv_extra_args")) cfg.mpv_extra_args  = j["mpv_extra_args"].get<std::vector<std::string>>();
    if (j.contains("ipc_socket_path"))cfg.ipc_socket_path = j["ipc_socket_path"].get<std::string>();
    if (j.contains("mpv_socket_path"))cfg.mpv_socket_path = j["mpv_socket_path"].get<std::string>();
    return cfg;
}

Config config_load() {
    std::string path = config_path();
    std::ifstream f(path);
    if (f.good()) {
        try {
            json j = json::parse(f);
            Config cfg = config_from_json(j);
            LOG_INFO("loaded config from %s", path.c_str());
            return cfg;
        } catch (const json::exception& e) {
            LOG_ERROR("failed to parse config %s: %s", path.c_str(), e.what());
        }
    }

    LOG_INFO("config not found at %s, creating default", path.c_str());
    Config cfg;
    config_save(cfg);
    return cfg;
}

void config_save(const Config& cfg) {
    std::string path = config_path();
    ensure_parent_dir(path);
    std::ofstream f(path);
    if (!f) {
        LOG_ERROR("cannot write config to %s", path.c_str());
        return;
    }
    f << config_to_json(cfg).dump(2) << "\n";
    LOG_INFO("saved config to %s", path.c_str());
}
